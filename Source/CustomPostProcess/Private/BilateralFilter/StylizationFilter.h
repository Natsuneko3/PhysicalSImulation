#pragma once
#include "RenderAdapter.h"
#include "StylizationFilter.generated.h"

UENUM()
enum class EStylizationFilterType:uint8
{
	SNNFilter = 0,
	KuwaharaFilter = 1
};
UCLASS(NotBlueprintable, MinimalAPI)
class UStylizationFilter: public URenderAdapterBase
{
public:
	GENERATED_BODY()
	UStylizationFilter();


	UPROPERTY(Category = CustomPostProcess,EditAnywhere)
	EStylizationFilterType StylizationType;

	UPROPERTY(Category = CustomPostProcess,EditAnywhere)
	int Step = 5;

	UPROPERTY(Category = CustomPostProcess,EditAnywhere)
	float StylizationIntensity = 1.f;

	virtual void PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs) override;

};