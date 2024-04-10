      #include "PhysicalLiquidSolver.h"

#include "GPUSortManager.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "ShaderParameterStruct.h"

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

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_SRV(Buffer<uint32>,		InIDBuffer)
		SHADER_PARAMETER_SRV(Buffer<float>,		InAttributeBuffer)
		SHADER_PARAMETER_UAV(RWBuffer<uint32>,		ParticleIDBuffer)
		SHADER_PARAMETER_UAV(RWBuffer<float>,		ParticleAttributeBuffer)
	SHADER_PARAMETER(uint32,LastNumParticle)
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), 64);
        OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), 1);
		OutEnvironment.SetDefine(TEXT("NUMATTRIBUTE"), NUMATTRIBUTE);

	}
};
IMPLEMENT_GLOBAL_SHADER(FSpawnParticleCS,  "/PluginShader/InitialParticle.usf", "SpawnParticleCS", SF_Compute);

class FLiquidParticleCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FLiquidParticleCS);
	SHADER_USE_PARAMETER_STRUCT(FLiquidParticleCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
	    SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLiuquidParameter, LiuquidParameter)
		SHADER_PARAMETER(int, ShaderType)
		SHADER_PARAMETER_UAV(RWBuffer<uint32>,		ParticleIDBuffer)
	SHADER_PARAMETER_UAV(RWTexture3D<float>, RasterizeTexture)
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
IMPLEMENT_GLOBAL_SHADER(FLiquidParticleCS, "/PluginShader/MPM.usf", "MainCS", SF_Compute);

FPhysicalLiquidSolver::FPhysicalLiquidSolver()
{
	LastNumParticle = 0;
	AllocatedInstanceCounts = 0;
}

void FPhysicalLiquidSolver::SetParameter(FSolverParameter* InParameter)
{
	FPhysicalSolverBase::SetParameter(InParameter);
}

void FPhysicalLiquidSolver::Update_RenderThread(FRDGBuilder& GraphBuilder,FPhysicalSolverContext* Context,FSceneView& InView)
{
	float DeltaTime = Context->SolverParameter->FluidParameter.SolverBaseParameter.dt;
	FRHICommandListImmediate& RHICmdList = GraphBuilder.RHICmdList;
	float CurrentNumParticle = LastNumParticle + DeltaTime * Context->SpawnRate;

	const auto ShaderMap = GetGlobalShaderMap(InView.FeatureLevel);
    uint32 DeadParticleNum = 0;

	//if we are in Initial stage. this will be nullptr
	if (ParticleIDReadback && ParticleIDReadback->IsReady())
	{
		// last frame readback buffer
		uint32* Particles = (uint32*)ParticleIDReadback->Lock(LastNumParticle * sizeof(uint32));
		DeadParticleNum = Particles[0];

		//uint32 Offset = 0;
		if(CVarPhysicalParticleDebug.GetValueOnAnyThread() == 1)
		{
			if(ParticleReadback && ParticleReadback->IsReady())
			{
				float* ParticleDebug = (float*)ParticleReadback->Lock(LastNumParticle * NUMATTRIBUTE * sizeof(float));
				for(int i = 0;i<int(LastNumParticle);i++)
				{
					uint32 ID = Particles[i];
					float age = ParticleDebug[i * NUMATTRIBUTE];
					float PositionX = ParticleDebug[i * NUMATTRIBUTE+1];
					float PositionY = ParticleDebug[i * NUMATTRIBUTE+2];
					float PositionZ = ParticleDebug[i* NUMATTRIBUTE+3];

					float VelocityX = ParticleDebug[i * NUMATTRIBUTE+4];
					float VelocityY = ParticleDebug[i * NUMATTRIBUTE+5];
					float VelocityZ = ParticleDebug[i * NUMATTRIBUTE+6];
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
	}

	//CurrentNumParticle -= float(DeadParticleNum);
	if(CurrentNumParticle > 0)
	{
		int ParticleElement = int(CurrentNumParticle);
		FIntVector DispatchCount = FIntVector(FMath::DivideAndRoundUp(ParticleElement , 64), 1, 1);
		if(!AllocatedInstanceCounts)
		{
			AllocatedInstanceCounts = 1;
			ParticleIDBuffer.Initialize(RHICmdList,TEXT("InitialIDBuffer"),sizeof(uint32),ParticleElement ,PF_R32_UINT,ERHIAccess::UAVCompute);
			ParticleAttributeBuffer.Initialize(RHICmdList,TEXT("InitialParticleBuffer"),sizeof(float),ParticleElement * NUMATTRIBUTE,PF_R32_FLOAT,ERHIAccess::UAVCompute);

			TShaderMapRef<FSpawnParticleCS> ComputeShader(ShaderMap);
			FSpawnParticleCS::FParameters Parameters;
			Parameters.InAttributeBuffer = ParticleAttributeBuffer.SRV;
			Parameters.InIDBuffer = ParticleIDBuffer.SRV;
			Parameters.LastNumParticle = CurrentNumParticle;
			Parameters.ParticleIDBuffer = ParticleIDBuffer.UAV;
			Parameters.ParticleAttributeBuffer = ParticleAttributeBuffer.UAV;

			FComputeShaderUtils::Dispatch(RHICmdList,
			ComputeShader,
			Parameters,
			DispatchCount);
		}else
		{
			//Spawn Or Initial Particle
			if(LastNumParticle != CurrentNumParticle)
			{
				FRWBuffer NextIDBuffer;
				FRWBuffer NextParticleBuffer;


				NextIDBuffer.Initialize(RHICmdList,TEXT("NextIDBuffer"),sizeof(uint32),ParticleElement,PF_R32_UINT,ERHIAccess::UAVCompute, BUF_Static | BUF_SourceCopy);
				NextParticleBuffer.Initialize(RHICmdList,TEXT("NextParticleBuffer"),sizeof(float),ParticleElement * NUMATTRIBUTE,PF_R32_FLOAT,ERHIAccess::UAVCompute);


				TShaderMapRef<FSpawnParticleCS> ComputeShader(ShaderMap);
				FSpawnParticleCS::FParameters Parameters;
				Parameters.InAttributeBuffer = ParticleAttributeBuffer.SRV;
				Parameters.InIDBuffer = ParticleIDBuffer.SRV;
				Parameters.LastNumParticle = CurrentNumParticle;
				Parameters.ParticleIDBuffer = NextIDBuffer.UAV;
				Parameters.ParticleAttributeBuffer = NextParticleBuffer.UAV;

				FComputeShaderUtils::Dispatch(RHICmdList,
				ComputeShader,
				Parameters,
				DispatchCount);

				RHICmdList.Transition(FRHITransitionInfo(NextIDBuffer.UAV, ERHIAccess::UAVCompute, ERHIAccess::SRVMask | ERHIAccess::CopySrc));
				RHICmdList.Transition(FRHITransitionInfo(NextParticleBuffer.UAV, ERHIAccess::UAVCompute, ERHIAccess::SRVMask | ERHIAccess::CopySrc));
				Swap(NextIDBuffer, ParticleIDBuffer);
				Swap(NextParticleBuffer, ParticleAttributeBuffer);
			}
		}

		FUnorderedAccessViewRHIRef RasterizeTexture = RHICmdList.CreateUnorderedAccessView(Context->OutputTextures[0]->GetResource()->GetTextureRHI());
		auto DispatchShader = [&](int DoShaderType)
		{
			FLiquidParticleCS::FParameters PassParameters; //= GraphBuilder.AllocParameters<FLiquidParticleCS::FParameters>();

			TShaderMapRef<FLiquidParticleCS> ComputeShader(ShaderMap);
			PassParameters.View = InView.ViewUniformBuffer;
			PassParameters.ShaderType = DoShaderType;
			PassParameters.ParticleIDBuffer = ParticleIDBuffer.UAV;
			PassParameters.ParticleAttributeBuffer = ParticleAttributeBuffer.UAV;
			PassParameters.LiuquidParameter = Context->SolverParameter->LiuquidParameter;
			PassParameters.RasterizeTexture = RasterizeTexture;
			FComputeShaderUtils::Dispatch(RHICmdList,
				ComputeShader,
				PassParameters,
				DispatchCount);
		};

		{
			//Particle To Grid

			/*DispatchShader(0);
			DispatchShader(1);
			DispatchShader(2);*/
		}



		EnqueueGPUReadback(RHICmdList);

		LastNumParticle = CurrentNumParticle;
	}

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
