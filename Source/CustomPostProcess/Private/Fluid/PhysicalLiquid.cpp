#include "MeshPassProcessor.inl"
#include "ShaderParameterStruct.h"
#include "PixelShaderUtils.h"
#include "PhysicalLiquid.h"

DECLARE_CYCLE_STAT(TEXT("Particle To Cell"), STAT_P2G, STATGROUP_PS)
DECLARE_CYCLE_STAT(TEXT("Cell To Particle"), STAT_G2P, STATGROUP_PS)
DECLARE_CYCLE_STAT(TEXT("Rasterize"), STAT_Rasterize, STATGROUP_PS)
DECLARE_CYCLE_STAT(TEXT("SpawParticle"), STAT_SpawParticle, STATGROUP_PS)
DECLARE_CYCLE_STAT(TEXT("DrawLiuquidMesh"), STAT_DrawLiuquidMesh, STATGROUP_PS)

#define NUMATTRIBUTE 7
TAutoConsoleVariable<int32> CVarPhysicalParticleDebug(
	TEXT("r.PhysicalParticleDebugLog"),
	0,
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
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float>, InAttributeBuffer)
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
		//SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float>, ParticleAttributeBufferSRV)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<int>, RasterizeTexture)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float>, ParticleAttributeBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, OutParticleAttributeBuffer)
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

/*
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
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), 8);#1#
	}
};*/

class LiquidShaderVS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(LiquidShaderVS);
	SHADER_USE_PARAMETER_STRUCT(LiquidShaderVS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER(FMatrix44f, LocalToWorld)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float>,ParticleBuffer)
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

class LiquidShaderPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(LiquidShaderPS);
	SHADER_USE_PARAMETER_STRUCT(LiquidShaderPS, FGlobalShader);

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture3D,Rasterize)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D,Depth)
		SHADER_PARAMETER_SAMPLER(SamplerState,DepthSampler)
	END_SHADER_PARAMETER_STRUCT()
};

//IMPLEMENT_GLOBAL_SHADER(FTestShaderCS, "/Plugin/PhysicalSimulation/test.usf", "MainPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FSpawnParticleCS, "/Plugin/PhysicalSimulation/InitialParticle.usf", "SpawnParticleCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FLiquidParticleCS, "/Plugin/PhysicalSimulation/MPM.usf", "MainCS", SF_Compute);

IMPLEMENT_GLOBAL_SHADER(LiquidShaderVS, "/Plugin/PhysicalSimulation/LiquidShader.usf", "MainVS", SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(LiquidShaderPS, "/Plugin/PhysicalSimulation/LiquidShader.usf", "MainPS", SF_Pixel);

BEGIN_SHADER_PARAMETER_STRUCT(LiquidShaderParamater, )
	SHADER_PARAMETER_STRUCT_INCLUDE(LiquidShaderVS::FParameters, VS)
	SHADER_PARAMETER_STRUCT_INCLUDE(LiquidShaderPS::FParameters, PS)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()


FPhysicalLiquidSolver::~FPhysicalLiquidSolver()
{
	Release();
}

void FPhysicalLiquidSolver::SetLiuquidParameter(FLiuquidParameter& Parameter, FSceneView& InView,FPhysicalSimulationSceneProxy* InSceneProxy)
{
	Parameter.GravityScale = InSceneProxy->LiquidSolverParameter->GravityScale;
	Parameter.LifeTime = InSceneProxy->LiquidSolverParameter->LifeTime;
	SetupSolverBaseParameters(Parameter.SolverBaseParameter, InView,InSceneProxy);
}

void FPhysicalLiquidSolver::PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView)
{
	Frame ++;//= Frame + 1;
	float DeltaTime = SceneProxy->World->GetDeltaSeconds();

	FRDGBufferRef ParticleAttributeBuffer =  GraphBuilder.RegisterExternalBuffer(ParticleAttributeBufferPool);

	int CurrentNumParticle = LastNumParticle + FMath::RoundToInt( DeltaTime * SceneProxy->LiquidSolverParameter->SpawnRate);
	CurrentNumParticle = FMath::Min(65534,CurrentNumParticle);
	FRDGTextureRef RasterizeTexture = GraphBuilder.RegisterExternalTexture(RasterizeTexturePool,TEXT("LiuquidRasterize3DTexture"));
	FRDGTextureUAVRef RasterizeTextureUAV = GraphBuilder.CreateUAV(RasterizeTexture);

	const auto ShaderMap = GetGlobalShaderMap(InView.FeatureLevel);

	if (CurrentNumParticle > 1)
	{
		const int ParticleElement = CurrentNumParticle;
		const FIntVector DispatchCount = FIntVector(FMath::DivideAndRoundUp(ParticleElement, 64), 1, 1);
		const TShaderMapRef<FSpawnParticleCS> SpawnComputeShader(ShaderMap);

		//Spawn Or Initial Particle
		if (LastNumParticle != CurrentNumParticle)
		{
			SCOPE_CYCLE_COUNTER(STAT_SpawParticle);
			FRDGBufferDesc NewParticleDesc = FRDGBufferDesc::CreateBufferDesc(sizeof(float), FMath::Max(1, CurrentNumParticle) * NUMATTRIBUTE);
			ParticleAttributeBuffer = GraphBuilder.CreateBuffer(NewParticleDesc,TEXT("CenterParticleBuffer"),ERDGBufferFlags::MultiFrame);

			FSpawnParticleCS::FParameters* Parameters = GraphBuilder.AllocParameters<FSpawnParticleCS::FParameters>();
			Parameters->InAttributeBuffer = GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalBuffer(ParticleAttributeBufferPool),PF_R32_FLOAT);
			Parameters->LastNumParticle = LastNumParticle;
			Parameters->ParticleAttributeBuffer = GraphBuilder.CreateUAV(ParticleAttributeBuffer, PF_R32_FLOAT);

			FComputeShaderUtils::AddPass(GraphBuilder,RDG_EVENT_NAME("ExtendParticle"), SpawnComputeShader, Parameters, DispatchCount);
			AllocatePooledBuffer(NewParticleDesc,ParticleAttributeBufferPool,TEXT("NewParticleAttributeBuffer"));
		}

		TShaderMapRef<FLiquidParticleCS> LiquidComputeShader(ShaderMap);
		FLiquidParticleCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FLiquidParticleCS::FParameters>();
		SetLiuquidParameter(PassParameters->LiuquidParameter, InView,SceneProxy);
		PassParameters->ParticleAttributeBuffer = GraphBuilder.CreateSRV(ParticleAttributeBuffer, PF_R32_FLOAT);
		PassParameters->OutParticleAttributeBuffer = GraphBuilder.CreateUAV(GraphBuilder.RegisterExternalBuffer(ParticleAttributeBufferPool),PF_R32_FLOAT);
		PassParameters->RasterizeTexture = RasterizeTextureUAV;


		/*
		SCOPE_CYCLE_COUNTER(STAT_P2G);
		PassParameters->ShaderType = 0;
		FComputeShaderUtils::AddPass(GraphBuilder,RDG_EVENT_NAME("P2G"), LiquidComputeShader, PassParameters, DispatchCount);

		SCOPE_CYCLE_COUNTER(STAT_G2P);
		PassParameters->ShaderType = 1;
		FComputeShaderUtils::AddPass(GraphBuilder,RDG_EVENT_NAME("G2P"), LiquidComputeShader, PassParameters, DispatchCount);*/

		SCOPE_CYCLE_COUNTER(STAT_Rasterize);
		PassParameters->ShaderType = 2;
		FComputeShaderUtils::AddPass(GraphBuilder,RDG_EVENT_NAME("RasterizeToTexture"), LiquidComputeShader, PassParameters, DispatchCount);
	}
	LastNumParticle = CurrentNumParticle;
}

void FPhysicalLiquidSolver::PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs)
{

	if (!RasterizeTexturePool||!ParticleAttributeBufferPool || LastNumParticle == 0)
	{
		return;
	};
	const FViewInfo& ViewInfo = static_cast<const FViewInfo&>(View);
	//AddClearUAVPass(GraphBuilder,GraphBuilder.CreateUAV(CapsuleTileIntersectionCountsBuffer),100.f);
	SCOPE_CYCLE_COUNTER(STAT_DrawLiuquidMesh);


	LiquidShaderParamater* PassParameters = GraphBuilder.AllocParameters<LiquidShaderParamater>();
	PassParameters->VS.View = View.ViewUniformBuffer;
	PassParameters->VS.LocalToWorld = FMatrix44f(SceneProxy->ActorTransform->ToMatrixWithScale());
	PassParameters->VS.ParticleBuffer = GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalBuffer(ParticleAttributeBufferPool),PF_R32_FLOAT);

	PassParameters->RenderTargets[0] = FRenderTargetBinding((*Inputs.SceneTextures)->SceneColorTexture, ERenderTargetLoadAction::ELoad);
	PassParameters->PS.Rasterize = GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalTexture(RasterizeTexturePool));
	PassParameters->PS.Depth = GraphBuilder.CreateSRV((*Inputs.SceneTextures)->SceneDepthTexture);
	PassParameters->PS.DepthSampler = TStaticSamplerState<SF_Point>::GetRHI();
	PassParameters->PS.View = View.ViewUniformBuffer;

	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<LiquidShaderVS> VertexShader(GlobalShaderMap);
	TShaderMapRef<LiquidShaderPS> PixelShader(GlobalShaderMap);


	GraphBuilder.AddPass(
			RDG_EVENT_NAME("DrawInstanceMesh:%i",LastNumParticle),
			PassParameters,
			ERDGPassFlags::Raster,
			[VertexShader,PixelShader,PassParameters,&ViewInfo,this](FRHICommandList& RHICmdList)
			{
				FGraphicsPipelineStateInitializer GraphicsPSOInit;
				RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
				GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero>::GetRHI();
				GraphicsPSOInit.RasterizerState = GetStaticRasterizerState<true>(FM_Solid, CM_CW);
				GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI();
				GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
				GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
				GraphicsPSOInit.PrimitiveType = PT_TriangleList;
				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

				SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), PassParameters->VS);
				SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), PassParameters->PS);

				RHICmdList.SetViewport(ViewInfo.ViewRect.Min.X, ViewInfo.ViewRect.Min.Y, 0.0f, ViewInfo.ViewRect.Max.X, ViewInfo.ViewRect.Max.Y, 1.0f);
				RHICmdList.SetStreamSource(0, VertexBufferRHI, 0);
				RHICmdList.DrawPrimitive(0, NumPrimitives, LastNumParticle);
				//RHICmdList.DrawIndexedPrimitive(IndexBufferRHI, 0, 0, NumVertices, 0, NumPrimitives, LastNumParticle);
			});
}

void FPhysicalLiquidSolver::Release()
{
	VertexBufferRHI.SafeRelease();
	IndexBufferRHI.SafeRelease();
	RasterizeTexturePool.SafeRelease();ParticleAttributeBufferPool.SafeRelease();
}

void FPhysicalLiquidSolver::Initial_RenderThread(FRHICommandListImmediate& RHICmdList)
{
	//InitialPlaneMesh(RHICmdList);
	ParticleIDReadback = new FRHIGPUBufferReadback(TEXT("ParticleIDReadback"));
	ParticleReadback = new FRHIGPUBufferReadback(TEXT("ParticleReadback"));
	FPooledRenderTargetDesc LBMSolverBufferDesc(FPooledRenderTargetDesc::CreateVolumeDesc(GridSize.X, GridSize.Y, GridSize.Z, PF_R32_FLOAT, // PF_R16F, //
																							  FClearValueBinding(), TexCreate_None, TexCreate_ShaderResource | TexCreate_RenderTargetable | TexCreate_UAV, false));
	GRenderTargetPool.FindFreeElement(RHICmdList, LBMSolverBufferDesc, RasterizeTexturePool, TEXT("RasterizeTexture"));
	FRDGBufferDesc floatBufferDesc = FRDGBufferDesc::CreateBufferDesc(sizeof(float), NUMATTRIBUTE);
	ParticleAttributeBufferPool = AllocatePooledBuffer(floatBufferDesc, TEXT("ParticleAttributeBuffer"));

	float* ParticleData = (float*)RHICmdList.LockBuffer(ParticleAttributeBufferPool->GetRHI(),0,NUMATTRIBUTE * sizeof(float),RLM_WriteOnly);
	for(int i = 1;i<NUMATTRIBUTE;i++)
	{
		ParticleData[i] = 10;
	}
	RHICmdList.UnlockBuffer(ParticleAttributeBufferPool->GetRHI());
	InitialCubeMesh(RHICmdList);
}
