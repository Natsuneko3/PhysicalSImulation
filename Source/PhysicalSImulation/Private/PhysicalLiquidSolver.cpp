#include "PhysicalLiquidSolver.h"
#include "GPUSortManager.h"
#include "MeshPassProcessor.h"
#include "MeshPassProcessor.inl"
#include "PhysicalSimulationMeshProcessor.h"
#include "PhysicalSimulationSceneProxy.h"

#include "PhysicalSimulationSystem.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "ShaderParameterStruct.h"
#include "PhysicalSolver.h"
DECLARE_CYCLE_STAT(TEXT("Particle To Cell"), STAT_P2G, STATGROUP_PS)
DECLARE_CYCLE_STAT(TEXT("Cell To Particle"), STAT_G2P, STATGROUP_PS)
DECLARE_CYCLE_STAT(TEXT("Rasterize"), STAT_Rasterize, STATGROUP_PS)

//DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Physical Simulation Particle Polys"), STAT_PSParticlePolys, STATGROUP_Particles );
//DEFINE_STAT(STAT_PSParticlePolys);
#define NUMATTRIBUTE 7
TAutoConsoleVariable<int32> CVarPhysicalParticleDebug(
	TEXT("r.PhysicalParticleDebugLog"),
	1,
	TEXT("Debug Physical Simulation .")
	TEXT("0: Disabled (default)\n")
	TEXT("1: Print particle Position and Velocity \n"),
	ECVF_Default);



class FSpawnParticleCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FSpawnParticleCS);
	SHADER_USE_PARAMETER_STRUCT(FSpawnParticleCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<int>, InIDBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float>, InAttributeBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<int>, ParticleIDBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, ParticleAttributeBuffer)
		SHADER_PARAMETER(int, LastNumParticle)
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), 64);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), 1);
		OutEnvironment.SetDefine(TEXT("NUMATTRIBUTE"), NUMATTRIBUTE);
	}
};


class FLiquidParticleCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FLiquidParticleCS);
	SHADER_USE_PARAMETER_STRUCT(FLiquidParticleCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)

		SHADER_PARAMETER_STRUCT_INCLUDE(FLiuquidParameter, LiuquidParameter)
		SHADER_PARAMETER(int, ShaderType)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<int>, ParticleIDBufferSRV)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float>, ParticleAttributeBufferSRV)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint32>, ParticleIDBuffer)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<int>, RasterizeTexture)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, ParticleAttributeBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), 64);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), 1);
		OutEnvironment.SetDefine(TEXT("P2G"), 0);
		OutEnvironment.SetDefine(TEXT("G2P"), 1);
		OutEnvironment.SetDefine(TEXT("RASTERIZE"), 2);
		OutEnvironment.SetDefine(TEXT("NUMATTRIBUTE"), NUMATTRIBUTE);
	}
};

class FTestShaderCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTestShaderCS);
	SHADER_USE_PARAMETER_STRUCT(FTestShaderCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
		SHADER_PARAMETER(int, Time)
		SHADER_PARAMETER_UAV(RWTexture3D<float>, OutTestShader)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), 8);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), 8);
	}
};

IMPLEMENT_GLOBAL_SHADER(FTestShaderCS, "/PluginShader/test.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FSpawnParticleCS, "/PluginShader/InitialParticle.usf", "SpawnParticleCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FLiquidParticleCS, "/PluginShader/MPM.usf", "MainCS", SF_Compute);

FPhysicalLiquidSolver::FPhysicalLiquidSolver(FPhysicalSimulationSceneProxy* InSceneProxy)
	: FPhysicalSolverBase(InSceneProxy)
{
	LastNumParticle = 0;
	AllocatedInstanceCounts = 0;
	GridSize = SceneProxy->GridSize;
	//FPSCubeVertexBuffer VertexBuffer;
	ENQUEUE_RENDER_COMMAND(InitPSVertexFactory)(
		[this](FRHICommandListImmediate& RHICmdList)
		{
			Initial(RHICmdList);
		});
}

FPhysicalLiquidSolver::~FPhysicalLiquidSolver()
{
	Release();
}

void FPhysicalLiquidSolver::SetLiuquidParameter(FLiuquidParameter& Parameter, FSceneView& InView)
{
	Parameter.GravityScale = SceneProxy->LiquidSolverParameter->GravityScale;
	Parameter.LifeTime = SceneProxy->LiquidSolverParameter->LifeTime;
	SetupSolverBaseParameters(Parameter.SolverBaseParameter, InView);
}

void FPhysicalLiquidSolver::PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView)
{
	if (!PSVertexBuffer.IsValid() || !PSVertexFactory.IsValid())
	{
		return;
	}
	Frame++;
	float DeltaTime = SceneProxy->World->GetDeltaSeconds();
	/*FRDGBufferUAVRef ParticleIDBufferUAV = GraphBuilder.CreateUAV(ParticleIDBuffer,PF_R32_UINT);
	FRDGBufferUAVRef ParticleAttributeBufferUAV = GraphBuilder.CreateUAV(ParticleAttributeBuffer,PF_R16F);*/

	if (!ParticleIDBuffer && !ParticleAttributeBuffer)
	{
		FRDGBufferDesc IntBufferDesc = FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), 1);
		FRDGBufferDesc floatBufferDesc = FRDGBufferDesc::CreateBufferDesc(sizeof(float), NUMATTRIBUTE);
		ParticleIDBuffer = GraphBuilder.CreateBuffer(IntBufferDesc,TEXT("IDBuffer"));
		ParticleAttributeBuffer = GraphBuilder.CreateBuffer(floatBufferDesc,TEXT("AttrbuteBuffer"));
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(ParticleIDBuffer,PF_R32_UINT), 0);
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(ParticleAttributeBuffer,PF_R16F), 0.0);
	}

	float CurrentNumParticle = LastNumParticle + DeltaTime * SceneProxy->LiquidSolverParameter->SpawnRate;
	const FRDGTextureDesc Desc = FRDGTextureDesc::Create3D(GridSize, PF_G16R16F, FClearValueBinding::None, TexCreate_ShaderResource | TexCreate_UAV);
	RasterizeTexture = GraphBuilder.CreateTexture(Desc,TEXT("LiuquidRasterize3DTexture"));
	FRDGTextureUAVRef RasterizeTextureUAV = GraphBuilder.CreateUAV(RasterizeTexture);
	AddClearUAVPass(GraphBuilder, RasterizeTextureUAV, 0.f);

	const auto ShaderMap = GetGlobalShaderMap(InView.FeatureLevel);
	int DeadParticleNum = 0;

	//Read back to check the value
	if (ParticleIDReadback && ParticleIDReadback->IsReady())
	{
		// last frame readback buffer

		//int* IDBuffer = (int*)RHICmdList.LockBuffer(ParticleIDBuffer, 0, sizeof(int), RLM_WriteOnly);
		int* Particles = (int*)ParticleIDReadback->Lock(LastNumParticle * sizeof(int));
		DeadParticleNum = Particles[0];

		//int Offset = 0;
		if (CVarPhysicalParticleDebug.GetValueOnAnyThread() == 1)
		{
			if (ParticleReadback && ParticleReadback->IsReady())
			{
				float* ParticleDebug = (float*)ParticleReadback->Lock(LastNumParticle * NUMATTRIBUTE * sizeof(float));

				UPhysicalSimulationSystem* SubSystem = SceneProxy->World->GetSubsystem<UPhysicalSimulationSystem>();
				if (SubSystem)
				{
					SubSystem->OutParticle.Empty();
					for (int i = 0; i < int(LastNumParticle); i++)
					{
						int ID = Particles[i];
						float age = ParticleDebug[i * NUMATTRIBUTE];
						float PositionX = ParticleDebug[i * NUMATTRIBUTE + 1];
						float PositionY = ParticleDebug[i * NUMATTRIBUTE + 2];
						float PositionZ = ParticleDebug[i * NUMATTRIBUTE + 3];

						float VelocityX = ParticleDebug[i * NUMATTRIBUTE + 4];
						float VelocityY = ParticleDebug[i * NUMATTRIBUTE + 5];
						float VelocityZ = ParticleDebug[i * NUMATTRIBUTE + 6];
						FVector Position = FVector(PositionX, PositionY, PositionZ);
						SubSystem->OutParticle.Add(Position);
					}
				}


				ParticleReadback->Unlock();
			}
		}

		ParticleIDReadback->Unlock();
		//IDBuffer[0] = 0;
		//RHICmdList.UnlockBuffer(ParticleIDBuffer.Buffer);
	}
	//CurrentNumParticle -= float(DeadParticleNum);
	if (CurrentNumParticle > 1)
	{
		const int ParticleElement = int(CurrentNumParticle);
		const FIntVector DispatchCount = FIntVector(FMath::DivideAndRoundUp(ParticleElement, 64), 1, 1);
		const TShaderMapRef<FSpawnParticleCS> SpawnComputeShader(ShaderMap);

		//Spawn Or Initial Particle
		if (LastNumParticle != CurrentNumParticle)
		{
			LastNumParticle = FMath::Max(1,LastNumParticle);
			FRDGBufferRef LastIDBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), LastNumParticle),TEXT("LastIDBUffer"));
			FRDGBufferRef LastParticleBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(float), LastNumParticle * NUMATTRIBUTE),TEXT("LastIDBUffer"));;
			AddCopyBufferPass(GraphBuilder, LastIDBuffer, ParticleIDBuffer);
			AddCopyBufferPass(GraphBuilder, LastParticleBuffer, ParticleAttributeBuffer);
			ParticleIDBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), CurrentNumParticle),TEXT("CenterIDBUffer"));
			ParticleAttributeBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(float), CurrentNumParticle * NUMATTRIBUTE),TEXT("CenterIDBUffer"));

			//TODO the new buffer need to initial?
			FSpawnParticleCS::FParameters* Parameters = GraphBuilder.AllocParameters<FSpawnParticleCS::FParameters>();
			Parameters->InAttributeBuffer = GraphBuilder.CreateSRV(LastIDBuffer); //ParticleAttributeBuffer.SRV;
			Parameters->InIDBuffer = GraphBuilder.CreateSRV(LastParticleBuffer); //ParticleIDBuffer.SRV;
			Parameters->LastNumParticle = CurrentNumParticle;
			Parameters->ParticleIDBuffer = GraphBuilder.CreateUAV(ParticleIDBuffer,PF_R32_UINT); //NextIDBuffer.UAV;
			Parameters->ParticleAttributeBuffer = GraphBuilder.CreateUAV(ParticleAttributeBuffer,PF_R16F); //NextParticleBuffer.UAV;
			GraphBuilder.AddPass(
				RDG_EVENT_NAME("DrawPSCubeMesh"),
				Parameters,
				ERDGPassFlags::Raster, [Parameters,SpawnComputeShader,DispatchCount](FRHICommandListImmediate& RHICmdList)
				{
					FComputeShaderUtils::Dispatch(RHICmdList,
					                              SpawnComputeShader,
					                              *Parameters,
					                              DispatchCount);
				});


		}

		TShaderMapRef<FLiquidParticleCS> LiquidComputeShader(ShaderMap);
		FLiquidParticleCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FLiquidParticleCS::FParameters>();
		SetLiuquidParameter(PassParameters->LiuquidParameter, InView);
		PassParameters->ParticleAttributeBufferSRV = GraphBuilder.CreateSRV(ParticleAttributeBuffer); //ParticleAttributeBuffer.SRV;
		PassParameters->ParticleIDBufferSRV = GraphBuilder.CreateSRV(ParticleIDBuffer);
		PassParameters->ParticleIDBuffer = GraphBuilder.CreateUAV(ParticleIDBuffer,PF_R32_UINT);
		PassParameters->ParticleAttributeBuffer = GraphBuilder.CreateUAV(ParticleAttributeBuffer, PF_R32_FLOAT);
		PassParameters->RasterizeTexture = RasterizeTextureUAV;
		PassParameters->ShaderType = 0;

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("DrawPSCubeMesh"),
			PassParameters,
			ERDGPassFlags::Raster, [LiquidComputeShader,PassParameters,DispatchCount](FRHICommandListImmediate& RHICmdList)
			{
				SCOPE_CYCLE_COUNTER(STAT_P2G);
				FComputeShaderUtils::Dispatch(RHICmdList,
				                              LiquidComputeShader,
				                              *PassParameters,
				                              DispatchCount);
			});

		PassParameters->ShaderType = 1;
		GraphBuilder.AddPass(
			RDG_EVENT_NAME("DrawPSCubeMesh"),
			PassParameters,
			ERDGPassFlags::Raster, [LiquidComputeShader,PassParameters,DispatchCount](FRHICommandListImmediate& RHICmdList)
			{
				SCOPE_CYCLE_COUNTER(STAT_G2P);
				FComputeShaderUtils::Dispatch(RHICmdList,
				                              LiquidComputeShader,
				                              *PassParameters,
				                              DispatchCount);
			});

		PassParameters->ShaderType = 2;
		GraphBuilder.AddPass(
			RDG_EVENT_NAME("DrawPSCubeMesh"),
			PassParameters,
			ERDGPassFlags::Raster, [LiquidComputeShader,PassParameters,DispatchCount,
				&InParticleAttributeBuffer = ParticleAttributeBuffer,&InParticleIDBuffer = ParticleIDBuffer](FRHICommandListImmediate& RHICmdList)
			{
				SCOPE_CYCLE_COUNTER(STAT_Rasterize);
				FComputeShaderUtils::Dispatch(RHICmdList,
				                              LiquidComputeShader,
				                              *PassParameters,
				                              DispatchCount);
				/*RHICmdList.Transition(FRHITransitionInfo(InParticleIDBuffer->GetRHI(), ERHIAccess::UAVCompute, ERHIAccess::SRVMask | ERHIAccess::CopySrc));
				RHICmdList.Transition(FRHITransitionInfo(InParticleAttributeBuffer->GetRHI(), ERHIAccess::UAVCompute, ERHIAccess::SRVMask | ERHIAccess::CopySrc));*/
			});
		GraphBuilder.SetBufferAccessFinal(ParticleIDBuffer, ERHIAccess::SRVMask | ERHIAccess::CopySrc);
		GraphBuilder.SetBufferAccessFinal(ParticleAttributeBuffer, ERHIAccess::SRVMask | ERHIAccess::CopySrc);
		EnqueueGPUReadback(GraphBuilder);
	}
	else
	{
		//FUnorderedAccessViewRHIRef RasterizeTexture = RHICmdList.CreateUnorderedAccessView(TextureRHI);

		/*TShaderMapRef<FTestShaderCS> TestShader(ShaderMap);
		FTestShaderCS::FParameters TestParameters;
		TestParameters.OutTestShader = RasterizeTexture;
		TestParameters.Time = Frame;
		FComputeShaderUtils::Dispatch(RHICmdList, TestShader, TestParameters, FIntVector(64, 64, 64));*/
	}

	LastNumParticle = CurrentNumParticle;
}

void FPhysicalLiquidSolver::Release()
{
	/*ParticleIDBuffer.Release();
	ParticleAttributeBuffer.Release();*/
	if (PSVertexBuffer)
	{
		PSVertexBuffer->ReleaseResource();
	}
	else
	{
		PSVertexBuffer.Reset();
	}
	if (PSVertexFactory)
	{
		PSVertexFactory->ReleaseResource();
	}
	else
	{
		PSVertexFactory.Reset();
	}
}

void FPhysicalLiquidSolver::Initial(FRHICommandListImmediate& RHICmdList)
{
	InitialPlaneMesh(RHICmdList);
}

void FPhysicalLiquidSolver::Render_RenderThread(FPostOpaqueRenderParameters& Parameters)
{
	const FSceneView* View = static_cast<FSceneView*>(Parameters.Uid);
	FRDGBuilder& GraphBuilder = *Parameters.GraphBuilder;
	FPSShaderParametersPS* PSShaderPamaters = GraphBuilder.AllocParameters<FPSShaderParametersPS>();
	if (!RasterizeTexture)
	{
		return;
	}

	PSShaderPamaters->InTexture1 = GraphBuilder.CreateSRV(RasterizeTexture);
	PSShaderPamaters->View = View->ViewUniformBuffer; // RHICmdList.CreateShaderResourceView(TextureRHI,0);
	PSShaderPamaters->InstanceCulling = FInstanceCullingContext::CreateDummyInstanceCullingUniformBuffer(GraphBuilder);
	PSShaderPamaters->RenderTargets[0] = FRenderTargetBinding(Parameters.ColorTexture, ERenderTargetLoadAction::ENoAction);
	auto DrawCubeMesh = [&](FPhysicalSimulationSceneProxy* InSceneProxy)
	{
		GraphBuilder.AddPass(
			RDG_EVENT_NAME("DrawPSCubeMesh"),
			PSShaderPamaters,
			ERDGPassFlags::Raster, [this,&InView = View,InSceneProxy](FRHICommandListImmediate& RHICmdList)
			{
				DrawDynamicMeshPass(*InView, RHICmdList,
				                    [&](FDynamicPassMeshDrawListContext* DynamicMeshPassContext)
				                    {
					                    FMeshBatch MeshBatch; 
					                    
					                    MeshBatch.VertexFactory = PSVertexFactory.Get();
					                    MeshBatch.MaterialRenderProxy = InSceneProxy->GetMeterial()->GetRenderProxy();
					                    MeshBatch.ReverseCulling = false;
					                    MeshBatch.Type = PT_TriangleList;
					                    MeshBatch.DepthPriorityGroup = SDPG_World;
					                    MeshBatch.bCanApplyViewModeOverrides = true;
					                    MeshBatch.bUseForMaterial = true;
					                    MeshBatch.CastShadow = false;
					                    MeshBatch.bUseForDepthPass = false;

					                    MeshBatch.Elements[0].IndexBuffer = PSVertexBuffer->GetIndexPtr();
					                    MeshBatch.Elements[0].FirstIndex = 0;
					                    MeshBatch.Elements[0].NumPrimitives = 2;
					                    MeshBatch.Elements[0].MinVertexIndex = 0;
					                    MeshBatch.Elements[0].MaxVertexIndex = 3;
					                    MeshBatch.Elements[0].VertexFactoryUserData = PSVertexFactory->GetUniformBuffer();//GIdentityPrimitiveUniformBuffer.GetUniformBufferRHI();

					                    //Combine vertex buffer and its shader for rendering
					                    FPhysicalSimulationMeshProcessor PassMeshProcessor(InView->Family->Scene->GetRenderScene(), InView, DynamicMeshPassContext);
					                    const uint64 DefaultBatchElementMask = ~0ull;
					                    PassMeshProcessor.AddMeshBatch(MeshBatch, DefaultBatchElementMask, InSceneProxy);
				                    }
				);
			}
		);
	};

	DrawCubeMesh(SceneProxy);
}

void FPhysicalLiquidSolver::PostSimulation()
{
}

void FPhysicalLiquidSolver::DrawCube()
{
}

void FPhysicalLiquidSolver::EnqueueGPUReadback(FRDGBuilder& GraphBuilder)
{
	if (!ParticleReadback && !ParticleIDReadback)
	{
		ParticleReadback = new FRHIGPUBufferReadback(TEXT("Physicial Simulation Particle ReadBack"));
		ParticleIDReadback = new FRHIGPUBufferReadback(TEXT("Physicial Simulation Particle ID ReadBack"));
	}

	AddEnqueueCopyPass(GraphBuilder, ParticleIDReadback, ParticleIDBuffer, sizeof(int));
	AddEnqueueCopyPass(GraphBuilder, ParticleIDReadback, ParticleAttributeBuffer, sizeof(float));
	/*ParticleReadback->EnqueueCopy(RHICmdList, ParticleAttributeBuffer.Buffer);
	ParticleIDReadback->EnqueueCopy(RHICmdList, ParticleIDBuffer.Buffer);*/

	//TODO should we need copy all particle buffer?
}
