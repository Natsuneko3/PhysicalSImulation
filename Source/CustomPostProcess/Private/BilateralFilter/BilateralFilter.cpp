#include "BilateralFilter.h"

#include "RenderGraphUtils.h"
#include "SceneTexturesConfig.h"

class FViewInfo;

UBilateralFilter::UBilateralFilter()
{
}

void UBilateralFilter::PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs)
{
	const FViewInfo& ViewInfo = static_cast<const FViewInfo&>(View);
	FRDGTextureRef SceneColor = (*Inputs.SceneTextures)->SceneColorTexture;
	FRDGTextureRef CopySceneColor = GraphBuilder.CreateTexture(SceneColor->Desc,TEXT("CopySceneColor"));
	FBilateralParameter BilateralParameter;
	BilateralParameter.BlurSize = BlurSize;
	BilateralParameter.Sigma = Sigma;
	BilateralParameter.Step = Step;

	AddCopyTexturePass(GraphBuilder,SceneColor,CopySceneColor);
	AddTextureBlurPass(GraphBuilder,ViewInfo,CopySceneColor,SceneColor,BilateralParameter);

}

void UBilateralFilter::Initial_RenderThread(FRHICommandListImmediate& RHICmdList)
{
}
