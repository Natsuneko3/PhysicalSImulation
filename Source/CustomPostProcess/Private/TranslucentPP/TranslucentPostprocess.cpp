#include "TranslucentPostprocess.h"

#include "CustomPostProcessSceneProxy.h"
#include "PostProcess/PostProcessMaterial.h"


UTranslucentPostProcess::UTranslucentPostProcess()
{
}

void UTranslucentPostProcess::PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs)
{
	if(Materials.Num() < 1)
	{
		return;
	}
	const FViewInfo& ViewInfo = static_cast<const FViewInfo&>(View);
	FRDGTextureRef TranslucencyTexture = Inputs.TranslucencyViewResourcesMap.Get(ETranslucencyPass::TPT_TranslucencyAfterDOF).ColorTexture.Target;

	for(const UMaterialInterface* Material : Materials)
	{
		FScreenPassTexture OutTranslucent(TranslucencyTexture);
		FPostProcessMaterialInputs PassInputs;
		PassInputs.SetInput(EPostProcessMaterialInput::SceneColor,FScreenPassTexture((*Inputs.SceneTextures)->SceneColorTexture));
		PassInputs.SetInput(EPostProcessMaterialInput::SeparateTranslucency,FScreenPassTexture(TranslucencyTexture));
		PassInputs.SceneTextures = GetSceneTextureShaderParameters(Inputs.SceneTextures);
		//OutTranslucent = AddPostProcessMaterialPass(GraphBuilder,ViewInfo,PassInputs,Material);
		AddCopyTexturePass(GraphBuilder,OutTranslucent.Texture,TranslucencyTexture);
	}

}

void UTranslucentPostProcess::Initial_RenderThread(FRHICommandListImmediate& RHICmdList)
{

}
