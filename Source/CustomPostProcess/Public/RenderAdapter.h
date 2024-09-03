#pragma once
//#include "RenderAdapter.generated.h"

#include "PostProcess/PostProcessing.h"
class FCPPSceneProxy;

//UCLASS(BlueprintType)
class FRenderAdapterBase //:public UObject
{
	//GENERATED_BODY()
public:

	//RenderAdapterBase();
	FRenderAdapterBase(FCPPSceneProxy* InSceneProxy)
	{
		SceneProxy = InSceneProxy;
	}

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


