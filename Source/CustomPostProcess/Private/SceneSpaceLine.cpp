#include "SceneSpaceLine.h"

class FSceneSpaceLineCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FSceneSpaceLineCS);
	SHADER_USE_PARAMETER_STRUCT(FSceneSpaceLineCS, FGlobalShader);
    
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
	    SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTextures)

	    SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutTexture)
		SHADER_PARAMETER(float,LineWeight)
		SHADER_PARAMETER(float,LineThreshold)
		SHADER_PARAMETER(FLinearColor,LineColor)
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
IMPLEMENT_GLOBAL_SHADER(FSceneSpaceLineCS, "/Plugin/PhysicalSimulation/SceneSpaceLine.usf", "MainCS", SF_Compute);

USceneSpaceLine::USceneSpaceLine()
{
}

void USceneSpaceLine::PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs)
{
	auto ShaderMap = GetGlobalShaderMap(View.FeatureLevel);
	const FViewInfo& ViewInfo = static_cast<const FViewInfo&>(View);

	FVector2f ViewSize = FVector2f((*Inputs.SceneTextures)->SceneColorTexture->Desc.Extent) * (ScreenPercent /100.f);
	FRDGTextureRef SceneColor = GetSceneTexture(Inputs);
	FRDGTextureDesc OutTextureDesc(FRDGTextureDesc::Create2D(FIntPoint(ViewSize.X, ViewSize.Y), (*Inputs.SceneTextures)->SceneColorTexture->Desc.Format, FClearValueBinding::None, TexCreate_RenderTargetable | TexCreate_UAV));
	FRDGTextureRef SceneSpaceLineTexture = GraphBuilder.CreateTexture(OutTextureDesc,TEXT("SceneSpaceLine"));

	TShaderMapRef<FSceneSpaceLineCS> ComputeShader(ShaderMap);
	FSceneSpaceLineCS::FParameters* Parameters = GraphBuilder.AllocParameters<FSceneSpaceLineCS::FParameters>();

	Parameters->View = View.ViewUniformBuffer;
	Parameters->SceneTextures =  Inputs.SceneTextures;

	Parameters->OutTexture = GraphBuilder.CreateUAV(SceneSpaceLineTexture);
	Parameters->LineWeight = LineWeight;
	Parameters->LineThreshold = LineThreshold;
	Parameters->LineColor = LineColor;
	FComputeShaderUtils::AddPass(GraphBuilder,
									 RDG_EVENT_NAME("SceneSpaceLine"),
									 ERDGPassFlags::Compute,
									 ComputeShader,
									 Parameters,
									 FComputeShaderUtils::GetGroupCount(FIntVector(ViewSize.X,ViewSize.Y, 1), 8));
	AddTextureCombinePass(GraphBuilder,ViewInfo,Inputs,SceneSpaceLineTexture,SceneColor,&TextureBlendDesc);
}
