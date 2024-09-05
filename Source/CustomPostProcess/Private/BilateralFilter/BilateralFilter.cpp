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
	FRDGTextureRef SceneColor = GetSceneTexture(Inputs);
	FVector2f ViewSize = FVector2f((*Inputs.SceneTextures)->SceneColorTexture->Desc.Extent) * (ScreenPercent /100.f);
	FRDGTextureDesc OutTextureDesc(FRDGTextureDesc::Create2D(FIntPoint(ViewSize.X,ViewSize.Y), SceneColor->Desc.Format, FClearValueBinding::None, TexCreate_RenderTargetable | TexCreate_UAV));
	FRDGTextureRef CopySceneColor = GraphBuilder.CreateTexture(OutTextureDesc,TEXT("CopySceneColor"));
	FBlurParameter BilateralParameter;
	BilateralParameter.BlurSize = BlurSize;
	BilateralParameter.Sigma = Sigma;
	BilateralParameter.Step = Step;
	BilateralParameter.BlurMethod = BlurType;
	BilateralParameter.ScreenPercent = ScreenPercent;
	AddTextureBlurPass(GraphBuilder,ViewInfo,SceneColor,CopySceneColor,BilateralParameter);
	AddTextureCombinePass(GraphBuilder,ViewInfo,Inputs,CopySceneColor,SceneColor,&TextureBlendDesc);
}

void USceneBlur::Initial_RenderThread(FRHICommandListImmediate& RHICmdList)
{
}
