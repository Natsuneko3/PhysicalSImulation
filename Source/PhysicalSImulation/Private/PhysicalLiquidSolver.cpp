#include "PhysicalLiquidSolver.h"
#include "GPUSortManager.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "ShaderParameterStruct.h"
DECLARE_CYCLE_STAT(TEXT("Particle To Cell"),STAT_P2G,STATGROUP_PS)
DECLARE_CYCLE_STAT(TEXT("Cell To Particle"),STAT_G2P,STATGROUP_PS)
DECLARE_CYCLE_STAT(TEXT("Rasterize"),STAT_Rasterize,STATGROUP_PS)

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

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_SRV(Buffer<int>,		InIDBuffer)
		SHADER_PARAMETER_SRV(Buffer<float>,		InAttributeBuffer)
		SHADER_PARAMETER_UAV(RWBuffer<int>,		ParticleIDBuffer)
		SHADER_PARAMETER_UAV(RWBuffer<float>,		ParticleAttributeBuffer)
	SHADER_PARAMETER(int,LastNumParticle)
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

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )

		SHADER_PARAMETER_STRUCT_INCLUDE(FLiuquidParameter, LiuquidParameter)
		SHADER_PARAMETER(int, ShaderType)
		SHADER_PARAMETER_UAV(RWBuffer<int>,		ParticleIDBuffer)
	SHADER_PARAMETER_UAV(RWTexture3D<int>, RasterizeTexture)
	SHADER_PARAMETER_UAV(RWBuffer<float>,		ParticleAttributeBuffer)
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

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
SHADER_PARAMETER(int,Time)
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
IMPLEMENT_GLOBAL_SHADER(FSpawnParticleCS,  "/PluginShader/InitialParticle.usf", "SpawnParticleCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FLiquidParticleCS, "/PluginShader/MPM.usf", "MainCS", SF_Compute);

FPhysicalLiquidSolver::FPhysicalLiquidSolver()
{
	LastNumParticle = 0;
	AllocatedInstanceCounts = 0;
	GridSize = FIntVector(64);
}

void FPhysicalLiquidSolver::SetParameter(FSolverParameter* InParameter)
{
	int x = FMath::Max(InParameter->FluidParameter.SolverBaseParameter.GridSize.X - 1,8);
	int y = FMath::Max(InParameter->FluidParameter.SolverBaseParameter.GridSize.Y - 1,8);
	int z = FMath::Max(InParameter->FluidParameter.SolverBaseParameter.GridSize.Z - 1,8);
	GridSize = FIntVector(x,y,z);
}

void FPhysicalLiquidSolver::Update_RenderThread(FRDGBuilder& GraphBuilder,FPhysicalSolverContext* Context,FSceneView& InView)
{
	Frame++;
	float DeltaTime = Context->SolverParameter->FluidParameter.SolverBaseParameter.dt;
	FRHICommandListImmediate& RHICmdList = GraphBuilder.RHICmdList;
	float CurrentNumParticle = LastNumParticle + DeltaTime * Context->SpawnRate;

	const auto ShaderMap = GetGlobalShaderMap(InView.FeatureLevel);
    int DeadParticleNum = 0;

	//if we are in Initial stage. this will be nullptr
	if (ParticleIDReadback && ParticleIDReadback->IsReady() && AllocatedInstanceCounts)
	{
		// last frame readback buffer
		int* IDBuffer = (int*)RHICmdList.LockBuffer(ParticleIDBuffer.Buffer,0,sizeof(int),RLM_WriteOnly);
		int* Particles = (int*)ParticleIDReadback->Lock(LastNumParticle * sizeof(int));
		DeadParticleNum = Particles[0];

		//int Offset = 0;
		if(CVarPhysicalParticleDebug.GetValueOnAnyThread() == 1)
		{
			if(ParticleReadback && ParticleReadback->IsReady())
			{
				float* ParticleDebug = (float*)ParticleReadback->Lock(LastNumParticle * NUMATTRIBUTE * sizeof(float));
				for(int i = 0;i<int(LastNumParticle);i++)
				{
					int ID = Particles[i];
					float age = ParticleDebug[i * NUMATTRIBUTE];
					float PositionX = ParticleDebug[i * NUMATTRIBUTE + 1];
					float PositionY = ParticleDebug[i * NUMATTRIBUTE + 2];
					float PositionZ = ParticleDebug[i * NUMATTRIBUTE + 3];

					float VelocityX = ParticleDebug[i * NUMATTRIBUTE + 4];
					float VelocityY = ParticleDebug[i * NUMATTRIBUTE + 5];
					float VelocityZ = ParticleDebug[i * NUMATTRIBUTE + 6];
					UE_LOG(LogSimulation,Log,TEXT("ID: %i"),i);
					UE_LOG(LogSimulation,Log,TEXT("Active: %i"),ID);
					UE_LOG(LogSimulation,Log,TEXT("Age: %f"),age);
					UE_LOG(LogSimulation,Log,TEXT("Position: %f,%f,%f"),PositionX,PositionY,PositionZ);
					UE_LOG(LogSimulation,Log,TEXT("Velocity: %f,%f,%f"),VelocityX,VelocityY,VelocityZ);
					UE_LOG(LogSimulation,Log,TEXT("========================"));
				}

				ParticleReadback->Unlock();
			}
		}

		ParticleIDReadback->Unlock();
		IDBuffer[0] = 0;
		RHICmdList.UnlockBuffer(ParticleIDBuffer.Buffer);
	}

	//CurrentNumParticle -= float(DeadParticleNum);
	if(CurrentNumParticle > 1)
	{
		const int ParticleElement = int(CurrentNumParticle);
		const FIntVector DispatchCount = FIntVector(FMath::DivideAndRoundUp(ParticleElement , 64), 1, 1);
		const TShaderMapRef<FSpawnParticleCS> SpawnComputeShader(ShaderMap);
		if(!AllocatedInstanceCounts)
		{
			AllocatedInstanceCounts = 1;
			/*ParticleIDBuffer.Initialize(RHICmdList,TEXT("InitialIDBuffer"),sizeof(int),ParticleElement ,PF_R32_UINT,ERHIAccess::UAVCompute);
			ParticleAttributeBuffer.Initialize(RHICmdList,TEXT("InitialParticleBuffer"),sizeof(float),ParticleElement * NUMATTRIBUTE,PF_R32_FLOAT,ERHIAccess::UAVCompute);*/
			ParticleIDBuffer.Initialize(TEXT("InitialIDBuffer"),sizeof(int),ParticleElement ,PF_R32_SINT,ERHIAccess::UAVCompute);
			ParticleAttributeBuffer.Initialize(TEXT("InitialParticleBuffer"),sizeof(float),ParticleElement * NUMATTRIBUTE,PF_R32_FLOAT,ERHIAccess::UAVCompute);
			RHICmdList.ClearUAVUint(ParticleIDBuffer.UAV,FUintVector4(0));
			RHICmdList.ClearUAVFloat(ParticleAttributeBuffer.UAV,FVector4f(0.0));

			FSpawnParticleCS::FParameters Parameters;
			Parameters.View = InView.ViewUniformBuffer;
			Parameters.InAttributeBuffer = ParticleAttributeBuffer.SRV;
			Parameters.InIDBuffer = ParticleIDBuffer.SRV;
			Parameters.LastNumParticle = CurrentNumParticle;
			Parameters.ParticleIDBuffer = ParticleIDBuffer.UAV;
			Parameters.ParticleAttributeBuffer = ParticleAttributeBuffer.UAV;

			FComputeShaderUtils::Dispatch(RHICmdList,
			SpawnComputeShader,
			Parameters,
			DispatchCount);
		}else
		{
			//Spawn Or Initial Particle
			if(LastNumParticle != CurrentNumParticle)
			{
				FRWBuffer NextIDBuffer;
				FRWBuffer NextParticleBuffer;


				/*
				NextIDBuffer.Initialize(RHICmdList,TEXT("NextIDBuffer"),sizeof(int),ParticleElement,PF_R32_UINT,ERHIAccess::UAVCompute, BUF_Static | BUF_SourceCopy);
				NextParticleBuffer.Initialize(RHICmdList,TEXT("NextParticleBuffer"),sizeof(float),ParticleElement * NUMATTRIBUTE,PF_R32_FLOAT,ERHIAccess::UAVCompute);*/
				NextIDBuffer.Initialize(TEXT("NextIDBuffer"),sizeof(int),ParticleElement,PF_R32_SINT,ERHIAccess::UAVCompute, BUF_Static | BUF_SourceCopy);
				NextParticleBuffer.Initialize(TEXT("NextParticleBuffer"),sizeof(float),ParticleElement * NUMATTRIBUTE,PF_R32_FLOAT,ERHIAccess::UAVCompute);

				FSpawnParticleCS::FParameters Parameters;
				Parameters.InAttributeBuffer = ParticleAttributeBuffer.SRV;
				Parameters.InIDBuffer = ParticleIDBuffer.SRV;
				Parameters.LastNumParticle = CurrentNumParticle;
				Parameters.ParticleIDBuffer = NextIDBuffer.UAV;
				Parameters.ParticleAttributeBuffer = NextParticleBuffer.UAV;

				FComputeShaderUtils::Dispatch(RHICmdList,
				SpawnComputeShader,
				Parameters,
				DispatchCount);

				RHICmdList.Transition(FRHITransitionInfo(NextIDBuffer.UAV, ERHIAccess::UAVCompute, ERHIAccess::SRVMask | ERHIAccess::CopySrc));
				RHICmdList.Transition(FRHITransitionInfo(NextParticleBuffer.UAV, ERHIAccess::UAVCompute, ERHIAccess::SRVMask | ERHIAccess::CopySrc));
				Swap(NextIDBuffer, ParticleIDBuffer);
				Swap(NextParticleBuffer, ParticleAttributeBuffer);
			}
		}

		ClearRenderTarget(RHICmdList,Context->OutputTextures[0]->GetResource()->GetTextureRHI());
		FUnorderedAccessViewRHIRef RasterizeTexture = RHICreateUnorderedAccessView(Context->OutputTextures[0]->GetResource()->GetTextureRHI());//RHICmdList.CreateUnorderedAccessView(Context->OutputTextures[0]->GetResource()->GetTextureRHI());

		TShaderMapRef<FLiquidParticleCS> LiquidComputeShader(ShaderMap);
		FLiquidParticleCS::FParameters PassParameters;
		PassParameters.ParticleIDBuffer = ParticleIDBuffer.UAV;
		PassParameters.ParticleAttributeBuffer = ParticleAttributeBuffer.UAV;
		PassParameters.LiuquidParameter = Context->SolverParameter->LiuquidParameter;
		PassParameters.RasterizeTexture = RasterizeTexture;
		{
			SCOPE_CYCLE_COUNTER(STAT_P2G);
			PassParameters.ShaderType = 0;

			FComputeShaderUtils::Dispatch(RHICmdList,
				LiquidComputeShader,
				PassParameters,
				DispatchCount);
		}

		{
			SCOPE_CYCLE_COUNTER(STAT_G2P);
			PassParameters.ShaderType = 1;

			FComputeShaderUtils::Dispatch(RHICmdList,
				LiquidComputeShader,
				PassParameters,
				DispatchCount);
		}
		RHICmdList.ClearUAVFloat(RasterizeTexture,FVector4f(.0,.0,.0,.0));
		{
			SCOPE_CYCLE_COUNTER(STAT_Rasterize);
			PassParameters.ShaderType = 2;

			FComputeShaderUtils::Dispatch(RHICmdList,
				LiquidComputeShader,
				PassParameters,
				DispatchCount);
		}

		EnqueueGPUReadback(RHICmdList);


	}else
	{
		FUnorderedAccessViewRHIRef RasterizeTexture = RHICmdList.CreateUnorderedAccessView(Context->OutputTextures[0]->GetResource()->GetTextureRHI());

		TShaderMapRef<FTestShaderCS> TestShader(ShaderMap);
		FTestShaderCS::FParameters TestParameters;
		TestParameters.OutTestShader = RasterizeTexture;
		TestParameters.Time = Frame;
		FComputeShaderUtils::Dispatch(RHICmdList,TestShader,TestParameters,FIntVector(64,64,64));
	}
	LastNumParticle = CurrentNumParticle;
}

void FPhysicalLiquidSolver::Initial(FPhysicalSolverContext* Context)
{

}

void FPhysicalLiquidSolver::Release()
{
	ParticleIDBuffer.Release();
	ParticleAttributeBuffer.Release();
}

void FPhysicalLiquidSolver::PostSimulation()
{
}

void FPhysicalLiquidSolver::EnqueueGPUReadback(FRHICommandListImmediate& RHICmdList)
{

	if (!ParticleReadback &&!ParticleIDReadback)
	{
		ParticleReadback = new FRHIGPUBufferReadback(TEXT("Physicial Simulation Particle ReadBack"));
		ParticleIDReadback = new FRHIGPUBufferReadback(TEXT("Physicial Simulation Particle ID ReadBack"));
	}
	ParticleReadback->EnqueueCopy(RHICmdList, ParticleAttributeBuffer.Buffer);
	ParticleIDReadback->EnqueueCopy(RHICmdList, ParticleIDBuffer.Buffer);

//TODO should we need copy all particle buffer?


}
