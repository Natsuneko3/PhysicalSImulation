#pragma once
#include "PostProcess/PostProcessing.h"
#include "RenderAdapter.generated.h"
class FCPPSceneProxy;


class FCommonMeshVS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FCommonMeshVS);
	SHADER_USE_PARAMETER_STRUCT(FCommonMeshVS, FGlobalShader);

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER(FMatrix44f, LocalToWorld)
	END_SHADER_PARAMETER_STRUCT()
};

DEFINE_LOG_CATEGORY_STATIC(LogCustomPostProcess, Log, All);

DECLARE_STATS_GROUP(TEXT("Custom Render Feature"), STATGROUP_CRF, STATCAT_Advanced)

BEGIN_SHADER_PARAMETER_STRUCT(FSolverBaseParameter, PHYSICALSIMULATION_API)
	SHADER_PARAMETER(FVector3f, GridSize)
	SHADER_PARAMETER(int, Time)
	SHADER_PARAMETER(float, dt)
	SHADER_PARAMETER(float, dx)
	SHADER_PARAMETER_SAMPLER(SamplerState, WarpSampler)
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FFluidParameter, PHYSICALSIMULATION_API)
	SHADER_PARAMETER_STRUCT_INCLUDE(FSolverBaseParameter, SolverBaseParameter)
	SHADER_PARAMETER(float, VorticityMult)
	SHADER_PARAMETER(float, NoiseFrequency)
	SHADER_PARAMETER(float, NoiseIntensity)
	SHADER_PARAMETER(float, VelocityDissipate)
	SHADER_PARAMETER(float, DensityDissipate)
	SHADER_PARAMETER(float, GravityScale)
SHADER_PARAMETER(int, UseFFT)
SHADER_PARAMETER(FVector3f, WorldPosition)
SHADER_PARAMETER(FVector3f, WorldVelocity)
	SHADER_PARAMETER_TEXTURE(Texture2D, InTexture)
SHADER_PARAMETER_SAMPLER(SamplerState, SimSampler)
SHADER_PARAMETER_SAMPLER(SamplerState, InTextureSampler)
END_SHADER_PARAMETER_STRUCT()


UCLASS(Abstract, Blueprintable, EditInlineNew, CollapseCategories )
class URenderAdapterBase :public UObject
{
	GENERATED_BODY()
public:

	URenderAdapterBase()
	{}

	virtual void Initial_RenderThread(FRHICommandListImmediate& RHICmdList)
	{
	}

	virtual void PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView)
	{
	}

	virtual void PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily)
	{
	}

	virtual void PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs)
	{
	};
	int32 Frame;
	void AddTextureBlurPass(FRDGBuilder& GraphBuilder,const FViewInfo& View,FRDGTextureRef InTexture,FRDGTextureRef& OutTexture,float BlurSize);
};


