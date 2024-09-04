#pragma once

#include "RenderAdapter.h"

#include "BilateralFilter.generated.h"

UCLASS(NotBlueprintable, MinimalAPI)
class UBilateralFilter: public URenderAdapterBase
{
public:
	GENERATED_BODY()
	UBilateralFilter();


	UPROPERTY(Category = CustomPostProcess,EditAnywhere,meta=(ClampMin=0.0001,DisplayName="Blur Intensity"))
	float BlurSize = 1.0;

	UPROPERTY(Category = CustomPostProcess,EditAnywhere)
	float Sigma = 10.0;

	UPROPERTY(Category = CustomPostProcess,EditAnywhere)
	int Step = 3;
	virtual void PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs) override;

	virtual void Initial_RenderThread(FRHICommandListImmediate& RHICmdList) override;

	bool bIsInitial = false;

};