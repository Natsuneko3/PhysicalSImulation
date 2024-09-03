#include "TranslucentPostprocess.h"

#include "CustomPostProcessSceneProxy.h"
#include "Renderer/Private/ScreenPass.h"
#include "Renderer/Private/PostProcess/PostProcessing.h"
#include "Renderer/Private/PostProcess/PostProcessMaterial.h"

FTranslucentPostProcess::FTranslucentPostProcess(FCPPSceneProxy* InSceneProxy,FTranslucentParameterSet* InParameterSet): FRenderAdapterBase(InSceneProxy),Parameters(InParameterSet)
{
	SceneProxy = InSceneProxy;
}

void FTranslucentPostProcess::PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs)
{
	if(Parameters->Materials.Num() < 1)
	{
		return;
	}
	const FViewInfo& ViewInfo = static_cast<const FViewInfo&>(View);
	FRDGTextureRef TranslucencyTexture = Inputs.TranslucencyViewResourcesMap.Get(ETranslucencyPass::TPT_TranslucencyAfterDOF).ColorTexture.Target;

	for(const UMaterialInterface* Material : Parameters->Materials)
	{
		FScreenPassTexture OutTranslucent(TranslucencyTexture);
		FPostProcessMaterialInputs PassInputs;
		PassInputs.SetInput( EPostProcessMaterialInput::SceneColor,FScreenPassTexture((*Inputs.SceneTextures)->SceneColorTexture));
		PassInputs.SetInput(EPostProcessMaterialInput::SeparateTranslucency,FScreenPassTexture(TranslucencyTexture));
		PassInputs.SceneTextures = GetSceneTextureShaderParameters(Inputs.SceneTextures);
		//OutTranslucent = AddPostProcessMaterialPass(GraphBuilder,ViewInfo,PassInputs,Material);
		AddCopyTexturePass(GraphBuilder,OutTranslucent.Texture,TranslucencyTexture);
	}

}

void FTranslucentPostProcess::Initial_RenderThread(FRHICommandListImmediate& RHICmdList)
{

}
