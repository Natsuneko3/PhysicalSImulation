#include "PhysicalLiquidSolver.h"
#include "MeshPassProcessor.inl"
#include "PhysicalSimulationMeshProcessor.h"
#include "PhysicalSimulationSceneProxy.h"
#include "PhysicalSimulationSystem.h"
#include "ShaderParameterStruct.h"
#include "PhysicalSolver.h"
#include "PixelShaderUtils.h"

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
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, InIDBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float>, InAttributeBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, ParticleIDBuffer)
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

class LiquidShaderVS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(LiquidShaderVS);
	SHADER_USE_PARAMETER_STRUCT(LiquidShaderVS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER(FMatrix44f, LocalToWorld)
		SHADER_PARAMETER(uint32, bIsfacingCamera)
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
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};

//IMPLEMENT_GLOBAL_SHADER(FTestShaderCS, "/Plugin/PhysicalSimulation/test.usf", "MainPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FSpawnParticleCS, "/Plugin/PhysicalSimulation/InitialParticle.usf", "SpawnParticleCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FLiquidParticleCS, "/Plugin/PhysicalSimulation/MPM.usf", "MainCS", SF_Compute);

IMPLEMENT_GLOBAL_SHADER(LiquidShaderVS, "/Plugin/PhysicalSimulation/LiquidShader.usf", "MainVS", SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(LiquidShaderPS, "/Plugin/PhysicalSimulation/LiquidShader.usf", "MainPS", SF_Pixel);

FPhysicalLiquidSolver::FPhysicalLiquidSolver(FPhysicalSimulationSceneProxy* InSceneProxy)
	: FPhysicalSolverBase(InSceneProxy)
{
	SceneProxy = InSceneProxy;
	LastNumParticle = 1;
	AllocatedInstanceCounts = 0;
	GridSize = SceneProxy->GridSize;
	//FPSCubeVertexBuffer VertexBuffer;

}

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

	FRDGBufferRef ParticleIDBuffer = GraphBuilder.RegisterExternalBuffer(ParticleIDBufferPool);
	FRDGBufferRef ParticleAttributeBuffer =  GraphBuilder.RegisterExternalBuffer(ParticleAttributeBufferPool);

	float CurrentNumParticle = LastNumParticle +  DeltaTime * SceneProxy->LiquidSolverParameter->SpawnRate;
	FRDGTextureRef RasterizeTexture = GraphBuilder.RegisterExternalTexture(RasterizeTexturePool,TEXT("LiuquidRasterize3DTexture"));
	FRDGTextureUAVRef RasterizeTextureUAV = GraphBuilder.CreateUAV(RasterizeTexture);
	//AddClearUAVPass(GraphBuilder, RasterizeTextureUAV, 0.f);

	const auto ShaderMap = GetGlobalShaderMap(InView.FeatureLevel);
	int DeadParticleNum = 0;

	//Read back to check the value
	if (CVarPhysicalParticleDebug.GetValueOnAnyThread() == 1)
	{
		// last frame readback buffer

		if (ParticleIDReadback && ParticleIDReadback->IsReady())
		{
			int* Particles = (int*)ParticleIDReadback->Lock(LastNumParticle * sizeof(int));
			DeadParticleNum = Particles[0];
			if (ParticleReadback && ParticleReadback->IsReady())
			{
				float* ParticleDebug = (float*)ParticleReadback->Lock(int(LastNumParticle) * NUMATTRIBUTE * sizeof(float));

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
			SCOPE_CYCLE_COUNTER(STAT_SpawParticle);
			LastNumParticle = FMath::Max(1, LastNumParticle);
			FRDGBufferRef LastIDBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), int(LastNumParticle)),TEXT("LastIDBUffer"));
			FRDGBufferRef LastParticleBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(float), int(LastNumParticle) * NUMATTRIBUTE),TEXT("LastParticleBUffer"));;
			AddCopyBufferPass(GraphBuilder, LastIDBuffer, ParticleIDBuffer);
			AddCopyBufferPass(GraphBuilder, LastParticleBuffer, ParticleAttributeBuffer);

			//New Buffer Pool
			//UE_LOG(LogTemp,Log,TEXT("Extend: %f"),CurrentNumParticle);

			ParticleIDBufferPool = AllocatePooledBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), ParticleElement) , TEXT("ParticleIDBuffer"));
			ParticleAttributeBufferPool = AllocatePooledBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(float) , ParticleElement * NUMATTRIBUTE), TEXT("ParticleAttributeBuffer"));
			 /*ParticleIDBuffer = GraphBuilder.RegisterExternalBuffer(ParticleIDBufferPool);
			 ParticleAttributeBuffer =  GraphBuilder.RegisterExternalBuffer(ParticleAttributeBufferPool);*/
			//TODO the new buffer need to initial?
			FSpawnParticleCS::FParameters* Parameters = GraphBuilder.AllocParameters<FSpawnParticleCS::FParameters>();
			Parameters->InAttributeBuffer = GraphBuilder.CreateSRV(LastIDBuffer, PF_R32_UINT); //ParticleAttributeBuffer.SRV;
			Parameters->InIDBuffer = GraphBuilder.CreateSRV(LastParticleBuffer, PF_R32_FLOAT); //ParticleIDBuffer.SRV;
			Parameters->LastNumParticle = LastNumParticle;
			Parameters->ParticleIDBuffer = GraphBuilder.CreateUAV(ParticleIDBuffer, PF_R32_UINT); //NextIDBuffer.UAV;
			Parameters->ParticleAttributeBuffer = GraphBuilder.CreateUAV(ParticleAttributeBuffer, PF_R32_FLOAT); //NextParticleBuffer.UAV;
			FComputeShaderUtils::AddPass(GraphBuilder,RDG_EVENT_NAME("ExtendParticle"), SpawnComputeShader, Parameters, DispatchCount);
		}
		FRDGBufferRef LastParticleBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(float), ParticleElement * NUMATTRIBUTE),TEXT("LastParticleBUffer"));
		AddCopyBufferPass(GraphBuilder, LastParticleBuffer, ParticleAttributeBuffer);
		TShaderMapRef<FLiquidParticleCS> LiquidComputeShader(ShaderMap);
		FLiquidParticleCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FLiquidParticleCS::FParameters>();
		SetLiuquidParameter(PassParameters->LiuquidParameter, InView,SceneProxy);
		PassParameters->ParticleAttributeBufferSRV = GraphBuilder.CreateSRV(LastParticleBuffer, PF_R32_FLOAT); //ParticleAttributeBuffer.SRV;
		PassParameters->ParticleIDBufferSRV = GraphBuilder.CreateSRV(ParticleIDBuffer, PF_R32_UINT);
		PassParameters->ParticleIDBuffer = GraphBuilder.CreateUAV(ParticleIDBuffer, PF_R32_UINT);
		PassParameters->ParticleAttributeBuffer = GraphBuilder.CreateUAV(LastParticleBuffer, PF_R32_FLOAT);
		PassParameters->RasterizeTexture = RasterizeTextureUAV;


		SCOPE_CYCLE_COUNTER(STAT_P2G);
		PassParameters->ShaderType = 0;
		FComputeShaderUtils::AddPass(GraphBuilder,RDG_EVENT_NAME("P2G"), LiquidComputeShader, PassParameters, DispatchCount);

		SCOPE_CYCLE_COUNTER(STAT_G2P);
		PassParameters->ShaderType = 1;
		FComputeShaderUtils::AddPass(GraphBuilder,RDG_EVENT_NAME("G2P"), LiquidComputeShader, PassParameters, DispatchCount);

		SCOPE_CYCLE_COUNTER(STAT_Rasterize);
		PassParameters->ShaderType = 2;
		FComputeShaderUtils::AddPass(GraphBuilder,RDG_EVENT_NAME("RasterizeToTexture"), LiquidComputeShader, PassParameters, DispatchCount);
		//EnqueueGPUReadback(GraphBuilder);
		GraphBuilder.QueueBufferExtraction(LastParticleBuffer,&ParticleAttributeBufferPool);
	}
	LastNumParticle = CurrentNumParticle ;//> 1000? 1 : CurrentNumParticle;
	AddEnqueueCopyPass(GraphBuilder,ParticleIDReadback,ParticleIDBuffer,sizeof(uint32));
	AddEnqueueCopyPass(GraphBuilder,ParticleReadback,ParticleAttributeBuffer,sizeof(float));

}

void FPhysicalLiquidSolver::Release()
{
	VertexBufferRHI.SafeRelease();
	IndexBufferRHI.SafeRelease();
	RasterizeTexturePool.SafeRelease();ParticleAttributeBufferPool.SafeRelease();
	ParticleIDBufferPool.SafeRelease();
}

void FPhysicalLiquidSolver::Initial_RenderThread(FRHICommandListImmediate& RHICmdList)
{
	//InitialPlaneMesh(RHICmdList);
	ParticleIDReadback = new FRHIGPUBufferReadback(TEXT("ParticleIDReadback"));
	ParticleReadback = new FRHIGPUBufferReadback(TEXT("ParticleReadback"));
	FPooledRenderTargetDesc LBMSolverBufferDesc(FPooledRenderTargetDesc::CreateVolumeDesc(GridSize.X, GridSize.Y, GridSize.Z, PF_R32_FLOAT, // PF_R16F, //
																							  FClearValueBinding(), TexCreate_None, TexCreate_ShaderResource | TexCreate_RenderTargetable | TexCreate_UAV, false));
	GRenderTargetPool.FindFreeElement(RHICmdList, LBMSolverBufferDesc, RasterizeTexturePool, TEXT("RasterizeTexture"));
	FRDGBufferDesc IntBufferDesc = FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), 1);
	FRDGBufferDesc floatBufferDesc = FRDGBufferDesc::CreateBufferDesc(sizeof(float), NUMATTRIBUTE);
	ParticleIDBufferPool = AllocatePooledBuffer(IntBufferDesc , TEXT("ParticleIDBuffer"));
	ParticleAttributeBufferPool = AllocatePooledBuffer(floatBufferDesc, TEXT("ParticleAttributeBuffer"));
	InitialCubeMesh();
}

void FPhysicalLiquidSolver::Render_RenderThread(FPostOpaqueRenderParameters& Parameters)
{
	const FSceneView* View = static_cast<FSceneView*>(Parameters.Uid);
	FRDGBuilder& GraphBuilder = *Parameters.GraphBuilder;

	if (!RasterizeTexturePool || !View->IsPrimarySceneView()  )
	{
		return;
	};

	//AddClearUAVPass(GraphBuilder,GraphBuilder.CreateUAV(CapsuleTileIntersectionCountsBuffer),100.f);
	SCOPE_CYCLE_COUNTER(STAT_DrawLiuquidMesh);
	LiquidShaderVS::FParameters* InVSParameters = GraphBuilder.AllocParameters<LiquidShaderVS::FParameters>();
	LiquidShaderPS::FParameters* InPSParameters = GraphBuilder.AllocParameters<LiquidShaderPS::FParameters>();
	InVSParameters->View = View->ViewUniformBuffer;
	InVSParameters->LocalToWorld = FMatrix44f(SceneProxy->ActorTransform->ToMatrixWithScale());
	InVSParameters->bIsfacingCamera = *SceneProxy->bFacingCamera;
	//InVSParameters->ParticleBuffer = GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalBuffer(ParticleAttributeBufferPool));

	InPSParameters->RenderTargets[0] = FRenderTargetBinding(Parameters.ColorTexture, ERenderTargetLoadAction::ELoad);
	InPSParameters->Rasterize = GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalTexture(RasterizeTexturePool));
	InPSParameters->Depth = GraphBuilder.CreateSRV(Parameters.DepthTexture);
	InPSParameters->DepthSampler = TStaticSamplerState<SF_Point>::GetRHI();
	InPSParameters->View = View->ViewUniformBuffer;

	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<LiquidShaderVS> VertexShader(GlobalShaderMap);
	TShaderMapRef<LiquidShaderPS> PixelShader(GlobalShaderMap);

	DrawMesh(VertexShader, PixelShader, InVSParameters, InPSParameters, *Parameters.GraphBuilder,Parameters.ViewportRect,1);
}

void FPhysicalLiquidSolver::PostSimulation()
{
}



void FPhysicalLiquidSolver::EnqueueGPUReadback(FRDGBuilder& GraphBuilder)
{

}
