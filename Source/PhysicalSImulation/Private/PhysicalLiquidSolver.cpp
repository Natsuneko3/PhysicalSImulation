#include "PhysicalLiquidSolver.h"

#include "GPUSortManager.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "ShaderParameterStruct.h"

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

	if(!AllocatedInstanceCounts)
	{
		AllocatedInstanceCounts = 1;
		ParticleIDBuffer.Initialize(RHICmdList,TEXT("InitialIDBuffer"),sizeof(uint32),CurrentNumParticle,PF_R32_UINT,ERHIAccess::UAVCompute);
		ParticleAttributeBuffer.Initialize(RHICmdList,TEXT("InitialParticleBuffer"),sizeof(float),CurrentNumParticle * 6,PF_FloatRGB,ERHIAccess::UAVCompute);
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
	if(CurrentNumParticle > 0)
	{
		FRWBuffer NextCountBuffer;
		int32 UsedIndexCounts[] = {int32(CurrentNumParticle)};
		NextCountBuffer.Initialize(RHICmdList,TEXT("NextParticleBuffer"),sizeof(uint32),int(CurrentNumParticle),PF_R32_UINT,ERHIAccess::UAVCompute, BUF_Static | BUF_SourceCopy);

		FRHIUnorderedAccessView* UAVs[] = { NextCountBuffer.UAV };
		CopyUIntBufferToTargets(RHICmdList, Context->FeatureLevel, ParticleIDBuffer.SRV, UAVs, UsedIndexCounts, 0, 1);
		RHICmdList.Transition(FRHITransitionInfo(NextCountBuffer.UAV, ERHIAccess::UAVCompute, ERHIAccess::SRVMask | ERHIAccess::CopySrc));
		Swap(NextCountBuffer, ParticleIDBuffer);
	}


	FLiquidParticleCS::FParameters PassParameters; //= GraphBuilder.AllocParameters<FLiquidParticleCS::FParameters>();
	auto ShaderMap = GetGlobalShaderMap(InView.FeatureLevel);
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
	FPhysicalSolverBase::Initial(Context);
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
	ParticleReadback->EnqueueCopy(RHICmdList, ParticleBuffer.Buffer);

//TODO should we need copy all particle buffer?


}
