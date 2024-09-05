#include "StylizationFilter.h"

class FStylizationCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FStylizationCS);
	SHADER_USE_PARAMETER_STRUCT(FStylizationCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
	    //SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D,SceneColor)
		SHADER_PARAMETER_SAMPLER(SamplerState,SceneColorSampler)
		SHADER_PARAMETER(int,Step)
	SHADER_PARAMETER(int,StylizationType)
	SHADER_PARAMETER(float,DownScale)
	SHADER_PARAMETER(float,StylizationIntensity)
	SHADER_PARAMETER(FVector2f,Resolution)
	    SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutStylization)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), 8);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), 8);
		OutEnvironment.SetDefine(TEXT("SNNFILTER"), 0);
		OutEnvironment.SetDefine(TEXT("KUWAHARAFILTER"), 1);
	}
	/*class FSNNFilter	  : SHADER_PERMUTATION_BOOL("SNNFilter");
	class FKuwaharaFilter	  : SHADER_PERMUTATION_BOOL("KuwaharaFilter");
  using FPermutationDomain = TShaderPermutationDomain<FSNNFilter, FKuwaharaFilter>;*/
};
IMPLEMENT_GLOBAL_SHADER(FStylizationCS, "/Plugin/PhysicalSimulation/TextureBlur/StylizationFilter.usf", "MainCS", SF_Compute);

UStylizationFilter::UStylizationFilter()
{
}

void UStylizationFilter::PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs)
{
	 const FViewInfo& ViewInfo = static_cast<const FViewInfo&>(View);
	auto ShaderMap = GetGlobalShaderMap(View.FeatureLevel);

	FRDGTextureRef SceneColor = (*Inputs.SceneTextures)->SceneColorTexture;
	TShaderMapRef<FStylizationCS> ComputeShader(ShaderMap);
	FStylizationCS::FParameters* Parameters = GraphBuilder.AllocParameters<FStylizationCS::FParameters>();
	FVector2f ViewSize = FVector2f((*Inputs.SceneTextures)->SceneColorTexture->Desc.Extent) * (ScreenPercent /100.f);
	FRDGTextureRef OutTexture = GraphBuilder.CreateTexture(SceneColor->Desc,TEXT("StylizationOutTexture"));
	Parameters->Resolution = ViewSize;
	Parameters->Step =Step;
	Parameters->StylizationIntensity = StylizationIntensity;
	Parameters->StylizationType = int(StylizationType);
	Parameters->SceneColor = SceneColor;
	Parameters->SceneColorSampler = TStaticSamplerState<>::GetRHI();
	Parameters->OutStylization = GraphBuilder.CreateUAV(OutTexture);
	Parameters->DownScale = 1/ (ScreenPercent / 100.f);
	FComputeShaderUtils::AddPass(GraphBuilder,
									 RDG_EVENT_NAME("Stylization %ix%i",ViewSize.X,ViewSize.Y),
									 ERDGPassFlags::Compute,
									 ComputeShader,
									 Parameters,
									 FComputeShaderUtils::GetGroupCount(FIntVector(ViewSize.X,ViewSize.Y, 1), 8));
									
	AddTextureCombinePass(GraphBuilder,ViewInfo,OutTexture,SceneColor,false,Weigth);
	//AddCopyTexturePass(GraphBuilder,OutTexture,SceneColor);

}
