#pragma once
#include "RenderAdapter.h"
#include "TranslucentBloom.generated.h"
class TranslucentBloom
{
public:

};

UCLASS(NotBlueprintable, MinimalAPI,DisplayName="Bloom")
class UTranslucentBloom: public URenderAdapterBase
{
public:
	GENERATED_BODY()
	UTranslucentBloom();

	UPROPERTY(Category = "RenderFeature",EditAnywhere)
	float Size = 50.f;

	UPROPERTY(Category = "RenderFeature",EditAnywhere)
	float Intensity = 0.3f;

	UPROPERTY(Category = "RenderFeature",EditAnywhere)
	float Falloff = 1.0f;

	UPROPERTY(Category = "RenderFeature",EditAnywhere)
	float BloomThreshold = 1.0f;

	UPROPERTY(Category = "RenderFeature",EditAnywhere,meta=(ClampMin=3,ClampMax=6,UIMin=3,UIMax=6))
	uint32 BloomQuality = 4;

	UPROPERTY(Category = "RenderFeature",EditAnywhere)
	FLinearColor Color = FLinearColor::White;

	virtual void PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs) override;

};