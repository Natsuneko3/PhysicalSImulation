#pragma once
#include "PostProcess/PostProcessInputs.h"
#include "RenderAdapter.generated.h"
class FCPPSceneProxy;

UCLASS(Abstract,hidecategories=(Object), MinimalAPI)
class URenderAdapterBase :public UObject
{
	GENERATED_BODY()
public:

	URenderAdapterBase()
	{SceneProxy = nullptr;}

	virtual void Initial_RenderThread(FRHICommandListImmediate& RHICmdList)
	{
	}

	virtual void PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView)
	{
	}

	virtual void PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily)
	{
	}

	virtual void PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs)
	{
	};
	FCPPSceneProxy* SceneProxy;
};


