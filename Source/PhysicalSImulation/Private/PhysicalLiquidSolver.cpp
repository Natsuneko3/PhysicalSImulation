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
#include "PixelShaderUtils.h"
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
	RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		/*OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), 8);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), 8);*/
	}
};
class FTestVS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FTestVS);
	SHADER_USE_PARAMETER_STRUCT(FTestVS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

	}
};
class FTestPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTestPS);
	SHADER_USE_PARAMETER_STRUCT(FTestPS, FGlobalShader);

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)

		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FTestShaderCS, "/PluginShader/test.usf", "MainPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FSpawnParticleCS, "/PluginShader/InitialParticle.usf", "SpawnParticleCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FLiquidParticleCS, "/PluginShader/MPM.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTestVS, "/Plugin/VdbVolume/Private/VdbPrincipled.usf", "MainVS", SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FTestPS, "/Plugin/VdbVolume/Private/VdbPrincipled.usf", "MainPS", SF_Pixel);

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
	FRDGBufferUAVRef ParticleAttributeBufferUAV = GraphBuilder.CreateUAV(ParticleAttributeBuffer,PF_R32_FLOAT);*/

	if (1)
	{
		FRDGBufferDesc IntBufferDesc = FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), 1);
		FRDGBufferDesc floatBufferDesc = FRDGBufferDesc::CreateBufferDesc(sizeof(float), NUMATTRIBUTE);
		ParticleIDBuffer = GraphBuilder.CreateBuffer(IntBufferDesc,TEXT("IDBuffer"));
		ParticleAttributeBuffer = GraphBuilder.CreateBuffer(floatBufferDesc,TEXT("AttrbuteBuffer"));
		//GraphBuilder.SetBufferAccessFinal(ParticleIDBuffer, ERHIAccess::VertexOrIndexBuffer | ERHIAccess::SRVMask);
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(ParticleIDBuffer, PF_R32_UINT), 0);
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(ParticleAttributeBuffer, PF_R32_FLOAT), 0.0);
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
			LastNumParticle = FMath::Max(1, LastNumParticle);
			FRDGBufferRef LastIDBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), LastNumParticle),TEXT("LastIDBUffer"));
			FRDGBufferRef LastParticleBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(float), LastNumParticle * NUMATTRIBUTE),TEXT("LastIDBUffer"));;
			AddCopyBufferPass(GraphBuilder, LastIDBuffer, ParticleIDBuffer);
			AddCopyBufferPass(GraphBuilder, LastParticleBuffer, ParticleAttributeBuffer);
			ParticleIDBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), CurrentNumParticle),TEXT("CenterIDBUffer"));
			ParticleAttributeBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(float), CurrentNumParticle * NUMATTRIBUTE),TEXT("CenterIDBUffer"));

			//TODO the new buffer need to initial?
			FSpawnParticleCS::FParameters* Parameters = GraphBuilder.AllocParameters<FSpawnParticleCS::FParameters>();
			Parameters->InAttributeBuffer = GraphBuilder.CreateSRV(LastIDBuffer, PF_R32_UINT); //ParticleAttributeBuffer.SRV;
			Parameters->InIDBuffer = GraphBuilder.CreateSRV(LastParticleBuffer, PF_R32_FLOAT); //ParticleIDBuffer.SRV;
			Parameters->LastNumParticle = CurrentNumParticle;
			Parameters->ParticleIDBuffer = GraphBuilder.CreateUAV(ParticleIDBuffer, PF_R32_UINT); //NextIDBuffer.UAV;
			Parameters->ParticleAttributeBuffer = GraphBuilder.CreateUAV(ParticleAttributeBuffer, PF_R32_FLOAT); //NextParticleBuffer.UAV;
			GraphBuilder.AddPass(
				RDG_EVENT_NAME("ExtendParticle"),
				Parameters,
				ERDGPassFlags::Compute, [Parameters,SpawnComputeShader,DispatchCount](FRHICommandListImmediate& RHICmdList)
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
		PassParameters->ParticleAttributeBufferSRV = GraphBuilder.CreateSRV(ParticleAttributeBuffer, PF_R32_FLOAT); //ParticleAttributeBuffer.SRV;
		PassParameters->ParticleIDBufferSRV = GraphBuilder.CreateSRV(ParticleIDBuffer, PF_R32_UINT);
		PassParameters->ParticleIDBuffer = GraphBuilder.CreateUAV(ParticleIDBuffer, PF_R32_UINT);
		PassParameters->ParticleAttributeBuffer = GraphBuilder.CreateUAV(ParticleAttributeBuffer, PF_R32_FLOAT);
		PassParameters->RasterizeTexture = RasterizeTextureUAV;

		PassParameters->ShaderType = 0;
		FComputeShaderUtils::AddPass(GraphBuilder,RDG_EVENT_NAME("P2G"), LiquidComputeShader, PassParameters, DispatchCount);

		PassParameters->ShaderType = 1;
		FComputeShaderUtils::AddPass(GraphBuilder,RDG_EVENT_NAME("G2P"), LiquidComputeShader, PassParameters, DispatchCount);

		PassParameters->ShaderType = 2;
		FComputeShaderUtils::AddPass(GraphBuilder,RDG_EVENT_NAME("RasterizeToTexture"), LiquidComputeShader, PassParameters, DispatchCount);
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

	if (!RasterizeTexture || !View->Family->Scene->GetRenderScene())
	{
		return;
	}

	PSShaderPamaters->InTexture1 = GraphBuilder.CreateSRV(RasterizeTexture);
	PSShaderPamaters->View = View->ViewUniformBuffer; // RHICmdList.CreateShaderResourceView(TextureRHI,0);
	PSShaderPamaters->InstanceCulling = FInstanceCullingContext::CreateDummyInstanceCullingUniformBuffer(GraphBuilder);
	PSShaderPamaters->RenderTargets[0] = FRenderTargetBinding(Parameters.ColorTexture, ERenderTargetLoadAction::ENoAction);


	/*FTestShaderCS::FParameters* TestPama = GraphBuilder.AllocParameters<FTestShaderCS::FParameters>();
	TestPama->Time = 0;
	TestPama->RenderTargets[0] = FRenderTargetBinding(Parameters.ColorTexture, ERenderTargetLoadAction::ELoad);
	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FTestShaderCS> PixelShader(ShaderMap);
	FPixelShaderUtils::AddFullscreenPass(
		GraphBuilder,
		ShaderMap,
		RDG_EVENT_NAME("LandscapeTextureHeightPatch"),
		PixelShader,
		TestPama,
		FIntRect(0, 0, Parameters.ColorTexture->Desc.Extent.X, Parameters.ColorTexture->Desc.Extent.Y));*/
	FBufferRHIRef VertexBufferRHI;
	FBufferRHIRef IndexBufferRHI;
	if (VertexBufferRHI == nullptr || !VertexBufferRHI.IsValid())
	{
		// Setup vertex buffer
		TResourceArray<FFilterVertex, VERTEXBUFFER_ALIGNMENT> Vertices;
		Vertices.SetNumUninitialized(8);

		FVector3f BboxMin(0, 0, 0);
		FVector3f BboxMax(1, 1, 1);

		// Front face
		Vertices[0].Position = FVector4f(BboxMin.X, BboxMin.Y, BboxMin.Z, 1.f);	Vertices[0].UV = FVector2f(0.f, 0.f);
		Vertices[1].Position = FVector4f(BboxMax.X, BboxMin.Y, BboxMin.Z, 1.f);	Vertices[1].UV = FVector2f(1.f, 0.f);
		Vertices[2].Position = FVector4f(BboxMin.X, BboxMax.Y, BboxMin.Z, 1.f);	Vertices[2].UV = FVector2f(0.f, 1.f);
		Vertices[3].Position = FVector4f(BboxMax.X, BboxMax.Y, BboxMin.Z, 1.f);	Vertices[3].UV = FVector2f(1.f, 1.f);
		// Back face		   FVector4f
		Vertices[4].Position = FVector4f(BboxMin.X, BboxMin.Y, BboxMax.Z, 1.f);	Vertices[0].UV = FVector2f(1.f, 1.f);
		Vertices[5].Position = FVector4f(BboxMax.X, BboxMin.Y, BboxMax.Z, 1.f);	Vertices[1].UV = FVector2f(1.f, 0.f);
		Vertices[6].Position = FVector4f(BboxMin.X, BboxMax.Y, BboxMax.Z, 1.f);	Vertices[2].UV = FVector2f(0.f, 1.f);
		Vertices[7].Position = FVector4f(BboxMax.X, BboxMax.Y, BboxMax.Z, 1.f);	Vertices[3].UV = FVector2f(0.f, 0.f);

		FRHIResourceCreateInfo CreateInfoVB(TEXT("VdbVolumeMeshVB"), &Vertices);
		VertexBufferRHI = RHICreateVertexBuffer(Vertices.GetResourceDataSize(), BUF_Static, CreateInfoVB);
	}

	if (IndexBufferRHI == nullptr || !IndexBufferRHI.IsValid())
	{
		// Setup index buffer
		const uint16 Indices[] = {
			// bottom face
			0, 1, 2,
			1, 3, 2,
			// right face
			1, 5, 3,
			3, 5, 7,
			// front face
			3, 7, 6,
			2, 3, 6,
			// left face
			2, 4, 0,
			2, 6, 4,
			// back face
			0, 4, 5,
			1, 0, 5,
			// top face
			5, 4, 6,
			5, 6, 7 };

		TResourceArray<uint16, INDEXBUFFER_ALIGNMENT> IndexBuffer;
		const uint32 NumIndices = UE_ARRAY_COUNT(Indices);
		IndexBuffer.AddUninitialized(NumIndices);
		FMemory::Memcpy(IndexBuffer.GetData(), Indices, NumIndices * sizeof(uint16));

		FRHIResourceCreateInfo CreateInfoIB(TEXT("VdbVolumeMeshIB"), &IndexBuffer);
		IndexBufferRHI = RHICreateIndexBuffer(sizeof(uint16), IndexBuffer.GetResourceDataSize(), BUF_Static, CreateInfoIB);

    }

	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FTestVS> VertexShader(GlobalShaderMap);
	TShaderMapRef<FTestPS> PixelShader(GlobalShaderMap);
	const FIntRect& ViewportRect = Parameters.ViewportRect;
	FTestPS::FParameters* PSParameters = GraphBuilder.AllocParameters<FTestPS::FParameters>();
	PSParameters->RenderTargets[0] = FRenderTargetBinding(Parameters.ColorTexture, ERenderTargetLoadAction::ELoad);
	GraphBuilder.AddPass(
				RDG_EVENT_NAME("DrawPSCubeMesh"),
				PSParameters,
				ERDGPassFlags::Raster | ERDGPassFlags::NeverCull,
				[this, PSParameters, VertexShader, PixelShader, ViewportRect, View,VertexBufferRHI,IndexBufferRHI](FRHICommandList& RHICmdList)
				{
					FTestVS::FParameters ParametersVS;
					ParametersVS.View = View->ViewUniformBuffer;

					FGraphicsPipelineStateInitializer GraphicsPSOInit;
					RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
					GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero>::GetRHI();


					GraphicsPSOInit.RasterizerState = GetStaticRasterizerState<true>(FM_Solid,  CM_CW );
					GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Less>::GetRHI();
					GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
					GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
					GraphicsPSOInit.PrimitiveType = PT_TriangleList;
					SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

					SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), ParametersVS);
					SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(),*PSParameters);

					RHICmdList.SetViewport(ViewportRect.Min.X, ViewportRect.Min.Y, 0.0f, ViewportRect.Max.X, ViewportRect.Max.Y, 1.0f);
					RHICmdList.SetStreamSource(0, VertexBufferRHI, 0);
					RHICmdList.DrawIndexedPrimitive(IndexBufferRHI, 0, 0, 8, 0, 12, 1);
				});

	/*GraphBuilder.AddPass(
		RDG_EVENT_NAME("DrawPSCubeMesh"),
		PSShaderPamaters,
		ERDGPassFlags::Raster, [this,&InView = *View,&InSceneProxy = SceneProxy](FRHICommandListImmediate& RHICmdList)
		{
	RHICmdList.SetViewport(ViewportRect.Min.X, ViewportRect.Min.Y, 0.0f, ViewportRect.Max.X, ViewportRect.Max.Y, 1.0f);
			RHICmdList.SetScissorRect(false, 0, 0, 0, 0);
			DrawDynamicMeshPass(InView, RHICmdList,
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
				                    MeshBatch.Elements[0].VertexFactoryUserData = PSVertexFactory->GetUniformBuffer(); //GIdentityPrimitiveUniformBuffer.GetUniformBufferRHI();

				                    //Combine vertex buffer and its shader for rendering
				                    FPhysicalSimulationMeshProcessor PassMeshProcessor(InView.Family->Scene->GetRenderScene(), &InView, DynamicMeshPassContext);
				                    const uint64 DefaultBatchElementMask = ~0ull;
				                    PassMeshProcessor.AddMeshBatch(MeshBatch, DefaultBatchElementMask, InSceneProxy);
			                    }
			);
		}
	);*/
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
