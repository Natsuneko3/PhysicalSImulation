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
	FRDGTextureRef SceneColor = GetSceneTexture(Inputs);
	TShaderMapRef<FStylizationCS> ComputeShader(ShaderMap);
	FStylizationCS::FParameters* Parameters = GraphBuilder.AllocParameters<FStylizationCS::FParameters>();
	FVector2f ViewSize = FVector2f((*Inputs.SceneTextures)->SceneColorTexture->Desc.Extent) * (ScreenPercent /100.f);

	FRDGTextureDesc OutTextureDesc(FRDGTextureDesc::Create2D(FIntPoint(ViewSize.X,ViewSize.Y), SceneColor->Desc.Format, FClearValueBinding::None, TexCreate_RenderTargetable | TexCreate_UAV));
	FRDGTextureRef OutTexture = GraphBuilder.CreateTexture(OutTextureDesc,TEXT("StylizationOutTexture"));
	Parameters->Resolution = ViewSize;
	Parameters->Step =Step;
	Parameters->StylizationIntensity = StylizationIntensity;
	Parameters->StylizationType = int(StylizationType);
	Parameters->SceneColor = SceneColor;
	Parameters->SceneColorSampler = TStaticSamplerState<>::GetRHI();
	Parameters->OutStylization = GraphBuilder.CreateUAV(OutTexture);

	FComputeShaderUtils::AddPass(GraphBuilder,
									 RDG_EVENT_NAME("Stylization %ix%i",int(ViewSize.X),int(ViewSize.Y)),
									 ERDGPassFlags::Compute,
									 ComputeShader,
									 Parameters,
									 FComputeShaderUtils::GetGroupCount(FIntVector(ViewSize.X,ViewSize.Y, 1), 8));
									
	AddTextureCombinePass(GraphBuilder,ViewInfo,Inputs,OutTexture,SceneColor,&TextureBlendDesc);
	//AddCopyTexturePass(GraphBuilder,OutTexture,SceneColor);

}
