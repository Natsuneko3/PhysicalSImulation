#pragma once
#include "RenderAdapter.h"
#include "TranslucentBloom.generated.h"
class TranslucentBloom
{
public:

};

UENUM()
enum class EBloomQuailtyLevel : uint8
{
	Low = 0,
	Medium = 1,
	High = 2

};
UCLASS(NotBlueprintable, MinimalAPI,DisplayName="Bloom")
class UTranslucentBloom: public URenderAdapterBase
{
public:
	GENERATED_BODY()
	UTranslucentBloom();
	UPROPERTY(Category = "RenderFeature",EditAnywhere,meta=(ClampMax=1))
	bool bDisableOriginBloom = true;

	UPROPERTY(Category = "RenderFeature",EditAnywhere)
	float Size = 5.0f;

	UPROPERTY(Category = "RenderFeature",EditAnywhere)
	float Intensity = 0.04f;

	UPROPERTY(Category = "RenderFeature",EditAnywhere)
	float Falloff = 0.5f;

	UPROPERTY(Category = "RenderFeature",EditAnywhere)
	float BloomThreshold = 1.0f;

	UPROPERTY(Category = "RenderFeature",EditAnywhere,meta=(ClampMin=1,ClampMax=10,UIMin=1,UIMax=10))
	float BloomGlow = 2.0f;

	UPROPERTY(Category = "RenderFeature",EditAnywhere)
	EBloomQuailtyLevel BloomQualityLevel = EBloomQuailtyLevel::High;

	UPROPERTY(Category = "RenderFeature",EditAnywhere)
	FLinearColor Color = FLinearColor::White;

	virtual void PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs) override;

};
