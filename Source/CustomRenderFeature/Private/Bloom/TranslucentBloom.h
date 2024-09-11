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
	UPROPERTY(Category = "RenderFeature",EditAnywhere,meta=(ClampMax=1))
	bool bDisableOriginBloom = true;

	UPROPERTY(Category = "RenderFeature",EditAnywhere,meta=(ClampMax=1))
	float Size = 1.0f;

	UPROPERTY(Category = "RenderFeature",EditAnywhere)
	float Intensity = 0.03f;

	UPROPERTY(Category = "RenderFeature",EditAnywhere)
	float Falloff = 0.1f;

	UPROPERTY(Category = "RenderFeature",EditAnywhere)
	float BloomThreshold = 1.0f;

	UPROPERTY(Category = "RenderFeature",EditAnywhere,meta=(ClampMin=1,ClampMax=10,UIMin=1,UIMax=10))
	float BloomGlow = 10.0f;

	UPROPERTY(Category = "RenderFeature",EditAnywhere,meta=(ClampMin=3,ClampMax=6,UIMin=3,UIMax=6))
	uint32 BloomQuality = 6;

	UPROPERTY(Category = "RenderFeature",EditAnywhere)
	FLinearColor Color = FLinearColor::White;

	virtual void PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs) override;

};
/*BloomFalloff:1.000000
Wed Sep 11 21:52:23 CST 2024  Log          LogTemp                   BloomFalloff:1.105171
Wed Sep 11 21:52:23 CST 2024  Log          LogTemp                   BloomFalloff:1.221403
Wed Sep 11 21:52:23 CST 2024  Log          LogTemp                   BloomFalloff:1.349859
Wed Sep 11 21:52:23 CST 2024  Log          LogTemp                   BloomFalloff:1.491825
Wed Sep 11 21:52:23 CST 2024  Log          LogTemp                   BloomFalloff:1.648721*/