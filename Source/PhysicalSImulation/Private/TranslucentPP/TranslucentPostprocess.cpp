#include "TranslucentPostprocess.h"

#include "PhysicalSimulationSceneProxy.h"
#include "PixelShaderUtils.h"
#include "PostProcess/PostProcessMaterialInputs.h"

FTranslucentPostProcess::FTranslucentPostProcess(FPhysicalSimulationSceneProxy* InSceneProxy): FPhysicalSolverBase(InSceneProxy)
{
	SceneProxy = InSceneProxy;
}

void FTranslucentPostProcess::PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs)
{
	if(SceneProxy->GetMaterials().Num() < 1)
	{
		return;
	}
	const FViewInfo& ViewInfo = static_cast<const FViewInfo&>(View);
	FRDGTextureRef TranslucencyTexture = Inputs.TranslucencyViewResourcesMap.Get(ETranslucencyPass::TPT_TranslucencyAfterDOF).ColorTexture.Target;

	for(const UMaterialInterface* Material : SceneProxy->GetMaterials())
	{
		FScreenPassTexture OutTranslucent(TranslucencyTexture);
		FPostProcessMaterialInputs PassInputs;
		PassInputs.SetInput(EPostProcessMaterialInput::SceneColor,FScreenPassTexture((*Inputs.SceneTextures)->SceneColorTexture));
		PassInputs.SetInput(EPostProcessMaterialInput::SeparateTranslucency,FScreenPassTexture(TranslucencyTexture));
		PassInputs.SceneTextures = GetSceneTextureShaderParameters(Inputs.SceneTextures);
		OutTranslucent = AddPostProcessMaterialPass(GraphBuilder,View,PassInputs,Material);
		AddCopyTexturePass(GraphBuilder,OutTranslucent.Texture,TranslucencyTexture);
	}

}

void FTranslucentPostProcess::Initial_RenderThread(FRHICommandListImmediate& RHICmdList)
{

}
