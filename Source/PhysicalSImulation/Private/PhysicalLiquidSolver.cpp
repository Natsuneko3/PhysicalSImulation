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

BEGIN_SHADER_PARAMETER_STRUCT(FPSShaderParametersPS,)
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
	SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, InTexture1)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FInstanceCullingGlobalUniforms, InstanceCulling)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

class FSpawnParticleCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FSpawnParticleCS);
	SHADER_USE_PARAMETER_STRUCT(FSpawnParticleCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_SRV(Buffer<int>, InIDBuffer)
		SHADER_PARAMETER_SRV(Buffer<float>, InAttributeBuffer)
		SHADER_PARAMETER_UAV(RWBuffer<int>, ParticleIDBuffer)
		SHADER_PARAMETER_UAV(RWBuffer<float>, ParticleAttributeBuffer)
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
		SHADER_PARAMETER_SRV(Buffer<int>, ParticleIDBufferSRV)
		SHADER_PARAMETER_SRV(Buffer<float>, ParticleAttributeBufferSRV)
		SHADER_PARAMETER_UAV(RWBuffer<int>, ParticleIDBuffer)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<int>, RasterizeTexture)
		SHADER_PARAMETER_UAV(RWBuffer<float>, ParticleAttributeBuffer)
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

void FPhysicalLiquidSolver::Update_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView)
{
	if(!SpriteVertexBuffer.IsValid() || !CubeVertexFactory.IsValid())
	{
		return;
	}
	Frame++;
	float DeltaTime = SceneProxy->World->GetDeltaSeconds();
	FRHICommandListImmediate& RHICmdList = GraphBuilder.RHICmdList;
	float CurrentNumParticle = LastNumParticle + DeltaTime * SceneProxy->LiquidSolverParameter->SpawnRate;
	/*const FRHITextureCreateDesc Desc =
		FRHITextureCreateDesc::Create3D(TEXT("LiuquidRasterize3DTexture"), GridSize, PF_R16F)
		.SetFlags(ETextureCreateFlags::ShaderResource | ETextureCreateFlags::UAV);*/

	const FRDGTextureDesc Desc = FRDGTextureDesc::Create3D(GridSize, PF_G16R16F, FClearValueBinding::None, TexCreate_ShaderResource | TexCreate_UAV);
	FRDGTextureRef RasterizeTexture = GraphBuilder.CreateTexture(Desc,TEXT("LiuquidRasterize3DTexture"));
	//FTextureRHIRef TextureRHI = RHICreateTexture(Desc); // = SceneProxy->OutputTextures[0]->GetResource()->GetTextureRHI();;

	/*if (!TextureRHI)
	{
		return;
	}*/


	const auto ShaderMap = GetGlobalShaderMap(InView.FeatureLevel);
	int DeadParticleNum = 0;

	//Read back to check the value
	if (ParticleIDReadback && ParticleIDReadback->IsReady())
	{
		// last frame readback buffer
		int* IDBuffer = (int*)RHICmdList.LockBuffer(ParticleIDBuffer.Buffer, 0, sizeof(int), RLM_WriteOnly);
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
						//DrawDebugPoint(Context->World,Position,2,FColor::Green);
						/*UE_LOG(LogSimulation,Log,TEXT("ID: %i"),i);
						UE_LOG(LogSimulation,Log,TEXT("Active: %i"),ID);
						UE_LOG(LogSimulation,Log,TEXT("Age: %f"),age);
						UE_LOG(LogSimulation,Log,TEXT("Position: %f,%f,%f"),PositionX,PositionY,PositionZ);
						UE_LOG(LogSimulation,Log,TEXT("Velocity: %f,%f,%f"),VelocityX,VelocityY,VelocityZ);
						UE_LOG(LogSimulation,Log,TEXT("========================"));*/
					}
				}


				ParticleReadback->Unlock();
			}
		}

		ParticleIDReadback->Unlock();
		IDBuffer[0] = 0;
		RHICmdList.UnlockBuffer(ParticleIDBuffer.Buffer);
	}

	//CurrentNumParticle -= float(DeadParticleNum);
	if (CurrentNumParticle > 1)
	{
		const int ParticleElement = int(CurrentNumParticle);
		const FIntVector DispatchCount = FIntVector(FMath::DivideAndRoundUp(ParticleElement, 64), 1, 1);
		const TShaderMapRef<FSpawnParticleCS> SpawnComputeShader(ShaderMap);
		{
			//Spawn Or Initial Particle
			if (LastNumParticle != CurrentNumParticle)
			{
				FRWBuffer NextIDBuffer;
				FRWBuffer NextParticleBuffer;


				NextIDBuffer.Initialize(RHICmdList,TEXT("NextIDBuffer"), sizeof(int), ParticleElement, PF_R32_UINT, ERHIAccess::UAVCompute, BUF_Static | BUF_SourceCopy);
				NextParticleBuffer.Initialize(RHICmdList,TEXT("NextParticleBuffer"), sizeof(float), ParticleElement * NUMATTRIBUTE, PF_R32_FLOAT, ERHIAccess::UAVCompute);
				/*NextIDBuffer.Initialize(TEXT("NextIDBuffer"), sizeof(int), ParticleElement, PF_R32_SINT, ERHIAccess::UAVCompute, BUF_Static | BUF_SourceCopy);
				NextParticleBuffer.Initialize(TEXT("NextParticleBuffer"), sizeof(float), ParticleElement * NUMATTRIBUTE, PF_R32_FLOAT, ERHIAccess::UAVCompute);*/
				//TODO the new buffer need to initial?
				FSpawnParticleCS::FParameters* Parameters = GraphBuilder.AllocParameters<FSpawnParticleCS::FParameters>();
				Parameters->InAttributeBuffer = ParticleAttributeBuffer.SRV;
				Parameters->InIDBuffer = ParticleIDBuffer.SRV;
				Parameters->LastNumParticle = CurrentNumParticle;
				Parameters->ParticleIDBuffer = NextIDBuffer.UAV;
				Parameters->ParticleAttributeBuffer = NextParticleBuffer.UAV;

				FComputeShaderUtils::Dispatch(RHICmdList,
				                              SpawnComputeShader,
				                              *Parameters,
				                              DispatchCount);

				RHICmdList.Transition(FRHITransitionInfo(NextIDBuffer.UAV, ERHIAccess::UAVCompute, ERHIAccess::SRVMask | ERHIAccess::CopySrc));
				RHICmdList.Transition(FRHITransitionInfo(NextParticleBuffer.UAV, ERHIAccess::UAVCompute, ERHIAccess::SRVMask | ERHIAccess::CopySrc));
				Swap(NextIDBuffer, ParticleIDBuffer);
				Swap(NextParticleBuffer, ParticleAttributeBuffer);
			}
		}

		//ClearRenderTarget(RHICmdList, TextureRHI);
		//FUnorderedAccessViewRHIRef RasterizeTexture = RHICreateUnorderedAccessView(TextureRHI); //RHICmdList.CreateUnorderedAccessView(TextureRHI);

		TShaderMapRef<FLiquidParticleCS> LiquidComputeShader(ShaderMap);
		FLiquidParticleCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FLiquidParticleCS::FParameters>();
		SetLiuquidParameter(PassParameters->LiuquidParameter, InView);
		PassParameters->ParticleAttributeBufferSRV = ParticleAttributeBuffer.SRV;
		PassParameters->ParticleIDBufferSRV = ParticleIDBuffer.SRV;
		PassParameters->ParticleIDBuffer = ParticleIDBuffer.UAV;
		PassParameters->ParticleAttributeBuffer = ParticleAttributeBuffer.UAV;
		PassParameters->RasterizeTexture = GraphBuilder.CreateUAV(RasterizeTexture);
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
			ERDGPassFlags::Raster, [LiquidComputeShader,PassParameters,DispatchCount](FRHICommandListImmediate& RHICmdList)
			{
				SCOPE_CYCLE_COUNTER(STAT_Rasterize);
				FComputeShaderUtils::Dispatch(RHICmdList,
				                              LiquidComputeShader,
				                              *PassParameters,
				                              DispatchCount);
			});

		EnqueueGPUReadback(RHICmdList);
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
	const FRDGTextureDesc TDDesc = FRDGTextureDesc::Create2D(FIntPoint(GridSize.X, GridSize.Y), PF_G16R16F, FClearValueBinding::None, TexCreate_ShaderResource | TexCreate_UAV | TexCreate_RenderTargetable);
	FRDGTextureRef TestOutPut = GraphBuilder.CreateTexture(TDDesc,TEXT("TestTexture"));
	FRDGTextureRef TestOutPut2 = GraphBuilder.CreateTexture(TDDesc,TEXT("TestTexture2"));
	AddClearRenderTargetPass(GraphBuilder, TestOutPut2, FLinearColor::Red);


	FPSShaderParametersPS* PSShaderPamaters = GraphBuilder.AllocParameters<FPSShaderParametersPS>();
	PSShaderPamaters->InTexture1 = GraphBuilder.CreateSRV(TestOutPut2);
	PSShaderPamaters->View = InView.ViewUniformBuffer; // RHICmdList.CreateShaderResourceView(TextureRHI,0);
	PSShaderPamaters->InstanceCulling = FInstanceCullingContext::CreateDummyInstanceCullingUniformBuffer(GraphBuilder);
	PSShaderPamaters->RenderTargets[0] = FRenderTargetBinding(TestOutPut, ERenderTargetLoadAction::EClear);
	auto DrawCubeMesh = [&](FPhysicalSimulationSceneProxy* InSceneProxy)
	{
		GraphBuilder.AddPass(
			RDG_EVENT_NAME("DrawPSCubeMesh"),
			PSShaderPamaters,
			ERDGPassFlags::Raster, [this,&CaptureView = InView,InSceneProxy](FRHICommandListImmediate& RHICmdList)
			{
				DrawDynamicMeshPass(CaptureView, RHICmdList,
				                    [&](FDynamicPassMeshDrawListContext* DynamicMeshPassContext)
				                    {
					                    FMeshBatch MeshBatch; // = Collector.AllocateMesh();
					                    MeshBatch.bUseWireframeSelectionColoring = InSceneProxy->IsSelected();
					                    MeshBatch.VertexFactory = CubeVertexFactory.Get();
					                    MeshBatch.MaterialRenderProxy = InSceneProxy->GetMeterial()->GetRenderProxy();
					                    MeshBatch.ReverseCulling = false;
					                    MeshBatch.Type = PT_TriangleList;
					                    MeshBatch.DepthPriorityGroup = SDPG_World;
					                    MeshBatch.bCanApplyViewModeOverrides = true;
					                    MeshBatch.bUseForMaterial = true;
					                    MeshBatch.CastShadow = false;
					                    MeshBatch.bUseForDepthPass = false;


					                    MeshBatch.Elements[0].IndexBuffer = &GScreenRectangleIndexBuffer;
					                    MeshBatch.Elements[0].FirstIndex = 0;
					                    MeshBatch.Elements[0].NumPrimitives = 1;
					                    MeshBatch.Elements[0].MinVertexIndex = 0;
					                    MeshBatch.Elements[0].MaxVertexIndex = 2;
					                    MeshBatch.Elements[0].PrimitiveUniformBuffer = GIdentityPrimitiveUniformBuffer.GetUniformBufferRHI();
					                    MeshBatch.Elements[0].PrimitiveIdMode = PrimID_ForceZero;

					                    //Combine vertex buffer and its shader for rendering
					                    FPhysicalSimulationMeshProcessor PassMeshProcessor(CaptureView.Family->Scene->GetRenderScene(), &CaptureView, DynamicMeshPassContext);
					                    const uint64 DefaultBatchElementMask = ~0ull;
					                    PassMeshProcessor.AddMeshBatch(MeshBatch, DefaultBatchElementMask, InSceneProxy);
				                    }
				);
			}
		);
	};

	DrawCubeMesh(SceneProxy);
	//VertexBuffer.ReleaseRHI();
	LastNumParticle = CurrentNumParticle;
}

void FPhysicalLiquidSolver::Release()
{
	ParticleIDBuffer.Release();
	ParticleAttributeBuffer.Release();
	if(SpriteVertexBuffer)
	{
		SpriteVertexBuffer->ReleaseResource();
	}
	else
	{
		SpriteVertexBuffer.Reset();
	}
	if(CubeVertexFactory)
	{
		CubeVertexFactory->ReleaseResource();
	}
	else
	{
		CubeVertexFactory.Reset();
	}
}

void FPhysicalLiquidSolver::Initial(FRHICommandListBase& RHICmdList)
{
	SpriteVertexBuffer = MakeUnique<FPSCubeVertexBuffer>();
	SpriteVertexBuffer->InitResource(RHICmdList);
	CubeVertexFactory = MakeUnique<FPSCubeVertexFactory>(SceneProxy->FeatureLevel, SpriteVertexBuffer.Get());
	CubeVertexFactory->InitResource(RHICmdList);

	ParticleIDBuffer.Initialize(RHICmdList,TEXT("InitialIDBuffer"), sizeof(int), 1, PF_R32_UINT, ERHIAccess::UAVCompute);
	ParticleAttributeBuffer.Initialize(RHICmdList,TEXT("InitialParticleBuffer"), sizeof(float), NUMATTRIBUTE, PF_R32_FLOAT, ERHIAccess::UAVCompute);
	float* Particle = (float*)RHICmdList.LockBuffer(ParticleAttributeBuffer.Buffer, 0, sizeof(float), RLM_WriteOnly);
	int* ID = (int*)RHICmdList.LockBuffer(ParticleIDBuffer.Buffer, 0, sizeof(int), RLM_WriteOnly);
	ID[0] = 0;
	for (int i = 0; i < NUMATTRIBUTE; i++)
	{
		Particle[i] = (float)i;
	}
	RHICmdList.UnlockBuffer(ParticleIDBuffer.Buffer);
	RHICmdList.UnlockBuffer(ParticleAttributeBuffer.Buffer);
	/*VertexFactory = MakeUnique<FPhysicalSimulationVertexFactory>(ERHIFeatureLevel::SM5);
	VertexFactory->SetInstanceBuffer(ParticleAttributeBuffer.SRV);*/
	//VertexFactory->InitRHI(RHICmdList);
}


void FPhysicalLiquidSolver::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector, const FPhysicalSimulationSceneProxy* InSceneProxy)
{
	/*FMeshBatch MeshBatch = Collector.AllocateMesh();
	MeshBatch.bUseWireframeSelectionColoring = SceneProxy->IsSelected();
	MeshBatch.VertexFactory = VertexFactory.Get();
	MeshBatch.MaterialRenderProxy = SceneProxy->GetMeterial()->GetRenderProxy();
	MeshBatch.ReverseCulling = false;
	MeshBatch.Type = PT_TriangleList;
	MeshBatch.DepthPriorityGroup = SDPG_World;
	MeshBatch.bCanApplyViewModeOverrides = true;
	MeshBatch.bUseForMaterial = true;
	MeshBatch.CastShadow = false;
	MeshBatch.bUseForDepthPass = false;

	const FStaticMeshLODResources& LODModel = SceneProxy->GetStaticMesh()->GetRenderData()->LODResources[0];
	VertexFactory->SetUpVertexBuffer(LODModel);
	VertexFactory->InitResource();
	FMeshBatchElement& BatchElement = MeshBatch.Elements[0];
	BatchElement.PrimitiveUniformBuffer = SceneProxy->GetUniformBuffer();
	BatchElement.IndexBuffer = &LODModel.IndexBuffer;
	BatchElement.FirstIndex = 0;
	BatchElement.MinVertexIndex = 0;
	BatchElement.NumInstances = LastNumParticle;
	BatchElement.MaxVertexIndex = LODModel.GetNumVertices() - 1;
	BatchElement.NumPrimitives = LODModel.GetNumTriangles();
	BatchElement.VertexFactoryUserData = VertexFactory->GetUniformBuffer();
	for(int i = 0;i< Views.Num();i++)
	{
		Collector.AddMesh(i,MeshBatch);
		//INC_DWORD_STAT_BY(STAT_PSParticlePolys, LastNumParticle * LODModel.GetNumTriangles());
	}*/
}

void FPhysicalLiquidSolver::PostSimulation()
{
}

void FPhysicalLiquidSolver::DrawCube()
{
}

void FPhysicalLiquidSolver::EnqueueGPUReadback(FRHICommandListImmediate& RHICmdList)
{
	if (!ParticleReadback && !ParticleIDReadback)
	{
		ParticleReadback = new FRHIGPUBufferReadback(TEXT("Physicial Simulation Particle ReadBack"));
		ParticleIDReadback = new FRHIGPUBufferReadback(TEXT("Physicial Simulation Particle ID ReadBack"));
	}
	ParticleReadback->EnqueueCopy(RHICmdList, ParticleAttributeBuffer.Buffer);
	ParticleIDReadback->EnqueueCopy(RHICmdList, ParticleIDBuffer.Buffer);

	//TODO should we need copy all particle buffer?
}
