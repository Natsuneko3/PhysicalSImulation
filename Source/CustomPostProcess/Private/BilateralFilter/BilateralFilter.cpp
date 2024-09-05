#include "BilateralFilter.h"

#include "RenderGraphUtils.h"
#include "SceneTexturesConfig.h"

class FViewInfo;

USceneBlur::USceneBlur()
{
}

void USceneBlur::PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs)
{
	const FViewInfo& ViewInfo = static_cast<const FViewInfo&>(View);
	FRDGTextureRef SceneColor = (*Inputs.SceneTextures)->SceneColorTexture;
	FRDGTextureRef CopySceneColor = GraphBuilder.CreateTexture(SceneColor->Desc,TEXT("CopySceneColor"));
	FBlurParameter BilateralParameter;
	BilateralParameter.BlurSize = BlurSize;
	BilateralParameter.Sigma = Sigma;
	BilateralParameter.Step = Step;
	BilateralParameter.BlurMethod = BlurType;
	BilateralParameter.ScreenPercent = ScreenPercent;
	//AddCopyTexturePass(GraphBuilder,SceneColor,CopySceneColor);
	AddTextureBlurPass(GraphBuilder,ViewInfo,SceneColor,CopySceneColor,BilateralParameter);
	AddTextureCombinePass(GraphBuilder,ViewInfo,CopySceneColor,SceneColor,false,Weigth);
}

void USceneBlur::Initial_RenderThread(FRHICommandListImmediate& RHICmdList)
{
}
