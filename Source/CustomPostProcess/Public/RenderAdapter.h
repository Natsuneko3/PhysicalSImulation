#pragma once
#include "PostProcess/PostProcessInputs.h"
#include "RenderAdapter.generated.h"

class FCPPSceneProxy;



UCLASS(BlueprintType)
class URenderAdapterBase :public UObject
{
	GENERATED_BODY()
public:

	URenderAdapterBase();
	URenderAdapterBase(FCPPSceneProxy* InSceneProxy)
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

USTRUCT()
struct FTestStruct
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere)
	int Frame = 0;

	UPROPERTY()
	URenderAdapterBase* RenderAdapter;
};


UCLASS(BlueprintType)
class UTestRenderAdapter :public URenderAdapterBase
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere)
	int testFrame = 0;
	UTestRenderAdapter();
};