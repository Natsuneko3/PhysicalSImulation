#pragma once

#include "RenderAdapter.h"

#include "BilateralFilter.generated.h"

UCLASS(NotBlueprintable, MinimalAPI,DisplayName="Scene Blur")
class USceneBlur: public URenderAdapterBase
{
public:
	GENERATED_BODY()
	USceneBlur();


	UPROPERTY(Category = CustomRenderFeature,EditAnywhere,meta=(DisplayName="BlurMethod"))
	EBlurMethod BlurType;

	UPROPERTY(Category = CustomRenderFeature,EditAnywhere,meta=(ClampMin=0.0001,DisplayName="Blur Intensity"))
	float BlurSize = 1.0;

	UPROPERTY(Category = CustomRenderFeature,EditAnywhere,meta=(EditCondition="BlurType==EBlurMethod::BilateralFilter",EditConditionHides))
	float Sigma = 10.0;

	UPROPERTY(Category = CustomRenderFeature,EditAnywhere,meta=(EditCondition="BlurType!=EBlurMethod::Gaussian",EditConditionHides))
	int Step = 3;

	virtual void PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs) override;

	virtual void Initial_RenderThread(FRHICommandListImmediate& RHICmdList) override;

};