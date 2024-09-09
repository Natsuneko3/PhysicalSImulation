#pragma once
#include "RenderAdapter.h"
#include "SceneSpaceLine.generated.h"
class SceneSpaceLine
{
public:

};
UCLASS(NotBlueprintable, MinimalAPI)
class USceneSpaceLine: public URenderAdapterBase
{
public:
	GENERATED_BODY()
	USceneSpaceLine();

	UPROPERTY(Category = CustomPostProcess,EditAnywhere)
	float LineWeight = 1.0;
	UPROPERTY(Category = CustomPostProcess,EditAnywhere)
	float LineThreshold = 0.0;
	UPROPERTY(Category = CustomPostProcess,EditAnywhere)
	FLinearColor LineColor;
	virtual void PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs) override;

};