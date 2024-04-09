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
	SHADER_PARAMETER(uint32,LastNumParticle)
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
	}
};
IMPLEMENT_GLOBAL_SHADER(FLiquidParticleCS, "/PluginShader/MPM.usf", "MainCS", SF_Compute);

FPhysicalLiquidSolver::FPhysicalLiquidSolver()
{
	CurrentNumParticle = 64;
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
	const uint32 NewSpawnParticle = CurrentNumParticle + FMath::DivideAndRoundDown(DeltaTime,Context->SpawnRate);
	FIntVector DispatchCount = FIntVector(FMath::DivideAndRoundUp(int(NewSpawnParticle) , 64), 1, 1);
	const auto ShaderMap = GetGlobalShaderMap(InView.FeatureLevel);

	if (ParticleReadback && ParticleReadback->IsReady())
	{
		// last frame readback buffer
		float* Particles = (float*)ParticleReadback->Lock(NewSpawnParticle * sizeof(float));
		//uint32 Offset = 0;
		for(int i = 0;i<int(NewSpawnParticle);i++)
		{
			float PositionX = Particles[0 ];

			UE_LOG(LogSimulation,Log,TEXT("Test: %f"),PositionX);

		}
		UE_LOG(LogSimulation,Log,TEXT("========================"));
		ParticleReadback->Unlock();
	}
	if(!AllocatedInstanceCounts)
	{
		AllocatedInstanceCounts = 1;
#if ENGINE_MINOR_VERSION > 3
		ParticleIDBuffer.Initialize(RHICmdList,TEXT("InitialIDBuffer"),sizeof(uint32),CurrentNumParticle,PF_R32_UINT,ERHIAccess::UAVCompute);
		ParticleAttributeBuffer.Initialize(RHICmdList,TEXT("InitialParticleBuffer"),sizeof(float),CurrentNumParticle * 6,PF_R32_FLOAT,ERHIAccess::UAVCompute);
#else
		ParticleIDBuffer.Initialize(TEXT("InitialIDBuffer"),sizeof(uint32),CurrentNumParticle,PF_R32_UINT,ERHIAccess::UAVCompute);
		ParticleAttributeBuffer.Initialize(TEXT("InitialParticleBuffer"),sizeof(float),CurrentNumParticle * 6,PF_FloatRGB,ERHIAccess::UAVCompute);
#endif

	}

	//Spawn Or Initial Particle
	/*if(NewSpawnParticle != CurrentNumParticle)
	{
		FRWBuffer NextIDBuffer;
		FRWBuffer NextParticleBuffer;

#if ENGINE_MINOR_VERSION > 3
		NextIDBuffer.Initialize(RHICmdList,TEXT("NextIDBuffer"),sizeof(uint32),int(CurrentNumParticle),PF_R32_UINT,ERHIAccess::UAVCompute, BUF_Static | BUF_SourceCopy);
		NextParticleBuffer.Initialize(RHICmdList,TEXT("NextParticleBuffer"),sizeof(float),CurrentNumParticle * 6,PF_FloatRGB,ERHIAccess::UAVCompute);
#else
		NextIDBuffer.Initialize(TEXT("NextIDBuffer"),sizeof(uint32),int(CurrentNumParticle),PF_R32_UINT,ERHIAccess::UAVCompute, BUF_Static | BUF_SourceCopy);
		NextParticleBuffer.Initialize(TEXT("NextParticleBuffer"),sizeof(float),CurrentNumParticle * 6,PF_FloatRGB,ERHIAccess::UAVCompute);
#endif


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
	}*/



	FUnorderedAccessViewRHIRef RasterizeTexture = RHICmdList.CreateUnorderedAccessView(Context->OutputTextures[0]->GetResource()->GetTextureRHI());
	auto DispatchShader = [&](int DoShaderType)
	{
		FLiquidParticleCS::FParameters PassParameters; //= GraphBuilder.AllocParameters<FLiquidParticleCS::FParameters>();

		TShaderMapRef<FLiquidParticleCS> ComputeShader(ShaderMap);
		PassParameters.View = InView.ViewUniformBuffer;
		PassParameters.ShaderType = DoShaderType;
		PassParameters.ParticleIDBuffer = ParticleIDBuffer.UAV;
		PassParameters.ParticleAttributeBuffer = ParticleAttributeBuffer.UAV;
		PassParameters.SolverBaseParameter = Context->SolverParameter->FluidParameter.SolverBaseParameter;
		PassParameters.RasterizeTexture = RasterizeTexture;
		FComputeShaderUtils::Dispatch(RHICmdList,
			ComputeShader,
			PassParameters,
			DispatchCount);
	};

	{
		//Particle To Grid
		DispatchShader(0);
		/*DispatchShader(0);
		DispatchShader(1);
		DispatchShader(2);*/
	}



	EnqueueGPUReadback(RHICmdList);

	CurrentNumParticle = NewSpawnParticle;
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
	ParticleReadback->EnqueueCopy(RHICmdList, ParticleAttributeBuffer.Buffer);

//TODO should we need copy all particle buffer?


}
