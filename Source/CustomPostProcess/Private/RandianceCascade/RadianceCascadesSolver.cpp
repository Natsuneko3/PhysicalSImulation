#include "RadianceCascadesSolver.h"
#if ENGINE_MINOR_VERSION >1
#include "DataDrivenShaderPlatformInfo.h"
#endif
#include "PixelShaderUtils.h"


BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FProbeInfoParamater, )
	SHADER_PARAMETER(FVector4f,RayNum)
	SHADER_PARAMETER_ARRAY(FVector4f,ProbeIntervel,[4])
	SHADER_PARAMETER(FVector4f,Diagonal)
	SHADER_PARAMETER(FVector4f,Step)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FProbeInfoParamater, "ProbeInfo");
class FRadianceCascadeGIPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FRadianceCascadeGIPS);
	SHADER_USE_PARAMETER_STRUCT(FRadianceCascadeGIPS, FGlobalShader);

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2DArray, RCTexture)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)

		SHADER_PARAMETER_SAMPLER(SamplerState, RCTextureSampler)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};

class FGenerateRadianceCascadeCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FGenerateRadianceCascadeCS);
	SHADER_USE_PARAMETER_STRUCT(FGenerateRadianceCascadeCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
		SHADER_PARAMETER_STRUCT_INCLUDE(ShaderPrint::FShaderParameters, ShaderPrintParameters)
		SHADER_PARAMETER_STRUCT_REF(FProbeInfoParamater,ProbeInfo)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray, GenerateRadianceCaseadeUAV)
		SHADER_PARAMETER(int, CascadeLevel)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), 8);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), 8);
	}
};

IMPLEMENT_GLOBAL_SHADER(FRadianceCascadeGIPS, "/Plugin/PhysicalSimulation/RadianceCaseade/RadianceCascadeGI.usf", "MainPS", SF_Pixel)
IMPLEMENT_GLOBAL_SHADER(FGenerateRadianceCascadeCS, "/Plugin/PhysicalSimulation/RadianceCaseade/GenerateRadianceCaseade.usf", "MainCS", SF_Compute);

FRadianceCascadesSolver::FRadianceCascadesSolver(FPhysicalSimulationSceneProxy* InScaneProxy): FPhysicalSolverBase(InScaneProxy)
{
	SceneProxy = InScaneProxy;
}

FRadianceCascadesSolver::~FRadianceCascadesSolver()
{
}

void FRadianceCascadesSolver::Initial_RenderThread(FRHICommandListImmediate& RHICmdList)
{
	FPhysicalSolverBase::Initial_RenderThread(RHICmdList);
}

void FRadianceCascadesSolver::PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs)
{
	FPhysicalSolverBase::PrePostProcessPass_RenderThread(GraphBuilder, View, Inputs);

	RDG_EVENT_SCOPE(GraphBuilder, "RadianceCascades");

	FRDGTextureDesc RDDesc = FRDGTextureDesc::Create2DArray(FIntPoint(128), PF_FloatRGBA, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV, 4);
	FRDGTextureRef RadianceCascadesTextures = GraphBuilder.CreateTexture(RDDesc, TEXT("RadianceCascadesTexture"));

	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(View.FeatureLevel);
	TShaderMapRef<FGenerateRadianceCascadeCS> ComputeShader(ShaderMap);
	const FViewInfo& ViewInfo = static_cast<const FViewInfo&>(View);

	ShaderPrint::SetEnabled(true);
	//int i = 0;
	for (int i = 0; i < 4; i++)
	{
		FGenerateRadianceCascadeCS::FParameters* Parameters = GraphBuilder.AllocParameters<FGenerateRadianceCascadeCS::FParameters>();
		Parameters->View = View.ViewUniformBuffer;
		Parameters->CascadeLevel = i;
		Parameters->SceneTexturesStruct = Inputs.SceneTextures;
		Parameters->GenerateRadianceCaseadeUAV = GraphBuilder.CreateUAV(RadianceCascadesTextures);

		FProbeInfoParamater ProbeInfoParamater;
		ProbeInfoParamater.Diagonal[i] = 1;
		ProbeInfoParamater.Step[i] = 0 ;
		ProbeInfoParamater.ProbeIntervel[i] = FVector4f(1);
		ProbeInfoParamater.RayNum[i] = (i + 1) * 8;
		Parameters->ProbeInfo = CreateUniformBufferImmediate(ProbeInfoParamater,UniformBuffer_SingleFrame);

		ShaderPrint::SetParameters(GraphBuilder, ViewInfo.ShaderPrintData, Parameters->ShaderPrintParameters);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("GenerateRadianceCaseade DestMipLevel=%d", i),
			ERDGPassFlags::Compute,
			ComputeShader,
			Parameters, FIntVector(16, 16, 1));
	}

	TShaderMapRef<FRadianceCascadeGIPS> PixelShader(ShaderMap);
	FRadianceCascadeGIPS::FParameters* PixelShaderParameters = GraphBuilder.AllocParameters<FRadianceCascadeGIPS::FParameters>();
	PixelShaderParameters->View = View.ViewUniformBuffer;
	PixelShaderParameters->RenderTargets[0] = FRenderTargetBinding((*Inputs.SceneTextures)->SceneColorTexture, ERenderTargetLoadAction::ENoAction);
	PixelShaderParameters->SceneTexturesStruct = Inputs.SceneTextures;
	PixelShaderParameters->RCTexture = RadianceCascadesTextures;
	PixelShaderParameters->RCTextureSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
	FPixelShaderUtils::AddFullscreenPass(
		GraphBuilder,
		ShaderMap,
		RDG_EVENT_NAME("RadianceCascadeMerge"),
		PixelShader,
		PixelShaderParameters,
		ViewInfo.ViewRect);
}
