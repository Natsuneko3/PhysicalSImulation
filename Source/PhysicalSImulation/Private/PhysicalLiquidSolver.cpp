#include "PhysicalLiquidSolver.h"

#include "GPUSortManager.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "ShaderParameterStruct.h"

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
	SHADER_PARAMETER(uint32,NewNumParticle)
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), 64);
        OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), 1);

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
		SHADER_PARAMETER_STRUCT_INCLUDE(FSolverBaseParameter, SolverBaseParameter)
		SHADER_PARAMETER(int, ShaderType)
		SHADER_PARAMETER_UAV(RWBuffer<uint32>,		ParticleIDBuffer)
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
	}
};
IMPLEMENT_GLOBAL_SHADER(FLiquidParticleCS, "/PluginShader/MPM.usf", "MainCS", SF_Compute);

FPhysicalLiquidSolver::FPhysicalLiquidSolver()
{
	CurrentNumParticle = 64;
}

void FPhysicalLiquidSolver::SetParameter(FSolverParameter* InParameter)
{
	FPhysicalSolverBase::SetParameter(InParameter);
}

void FPhysicalLiquidSolver::Update_RenderThread(FRDGBuilder& GraphBuilder,FPhysicalSolverContext* Context,FSceneView& InView)
{
	float DeltaTime = Context->SolverParameter->FluidParameter.SolverBaseParameter.dt;
	FRHICommandListImmediate& RHICmdList = GraphBuilder.RHICmdList;
	CurrentNumParticle += FMath::DivideAndRoundDown(DeltaTime,Context->SpawnRate);
	const auto ShaderMap = GetGlobalShaderMap(InView.FeatureLevel);
	if(!AllocatedInstanceCounts)
	{
		AllocatedInstanceCounts = 1;
#if ENGINE_MINOR_VERSION > 1
		ParticleIDBuffer.Initialize(RHICmdList,TEXT("InitialIDBuffer"),sizeof(uint32),CurrentNumParticle,PF_R32_UINT,ERHIAccess::UAVCompute);
		ParticleAttributeBuffer.Initialize(RHICmdList,TEXT("InitialParticleBuffer"),sizeof(float),CurrentNumParticle * 6,PF_FloatRGB,ERHIAccess::UAVCompute);
#else
		ParticleIDBuffer.Initialize(TEXT("InitialIDBuffer"),sizeof(uint32),CurrentNumParticle,PF_R32_UINT,ERHIAccess::UAVCompute);
		ParticleAttributeBuffer.Initialize(TEXT("InitialParticleBuffer"),sizeof(float),CurrentNumParticle * 6,PF_FloatRGB,ERHIAccess::UAVCompute);
#endif

	}

	if(CurrentNumParticle > 0)
	{
		FRWBuffer NextIDBuffer;
		FRWBuffer NextParticleBuffer;
		int32 UsedIndexCounts[] = {int32(CurrentNumParticle)};
		NextIDBuffer.Initialize(TEXT("NextIDBuffer"),sizeof(uint32),int(CurrentNumParticle),PF_R32_UINT,ERHIAccess::UAVCompute, BUF_Static | BUF_SourceCopy);
		NextParticleBuffer.Initialize(TEXT("NextParticleBuffer"),sizeof(float),CurrentNumParticle * 6,PF_FloatRGB,ERHIAccess::UAVCompute);

		TShaderMapRef<FSpawnParticleCS> ComputeShader(ShaderMap);

		RHICmdList.Transition(FRHITransitionInfo(NextIDBuffer.UAV, ERHIAccess::UAVCompute, ERHIAccess::SRVMask | ERHIAccess::CopySrc));
		Swap(NextIDBuffer, ParticleIDBuffer);
	}

	/*if (ParticleReadback && ParticleReadback->IsReady())
	{
		// last frame readback buffer
		uint32* Particles = (uint32*)(ParticleReadback->Lock(CurrentNumParticle * sizeof(uint32)));
		//uint32 Offset = 0;
		for(int i = 0;i<int(CurrentNumParticle);i++)
		{
			UE_LOG(LogSimulation,Log,TEXT("Test: %i"),Particles[i]);

		}
		UE_LOG(LogSimulation,Log,TEXT("========================"));
		ParticleReadback->Unlock();
	}*/



	FLiquidParticleCS::FParameters PassParameters; //= GraphBuilder.AllocParameters<FLiquidParticleCS::FParameters>();

	TShaderMapRef<FLiquidParticleCS> ComputeShader(ShaderMap);
	PassParameters.View = InView.ViewUniformBuffer;
	PassParameters.ShaderType = 0;
	PassParameters.ParticleIDBuffer = ParticleIDBuffer.UAV;
	PassParameters.ParticleAttributeBuffer = ParticleAttributeBuffer.UAV;
	PassParameters.SolverBaseParameter = Context->SolverParameter->FluidParameter.SolverBaseParameter;
	/*FComputeShaderUtils::AddPass(GraphBuilder,
									 RDG_EVENT_NAME("ParticleToCell"),
									 ERDGPassFlags::Compute,
									 ComputeShader,
									 PassParameters,
									 FComputeShaderUtils::GetGroupCount(FIntVector(GridSize.X, GridSize.Y, 1), ThreadNum));*/
	FComputeShaderUtils::Dispatch(RHICmdList,
		ComputeShader,
		PassParameters,
		FIntVector(FMath::DivideAndRoundUp(int(CurrentNumParticle) , 64), 1, 1));


	EnqueueGPUReadback(RHICmdList);
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

	if (!ParticleReadback)
	{
		ParticleReadback = new FRHIGPUBufferReadback(TEXT("Niagara GPU Instance Count Readback"));
	}
	ParticleReadback->EnqueueCopy(RHICmdList, ParticleIDBuffer.Buffer);

//TODO should we need copy all particle buffer?


}
