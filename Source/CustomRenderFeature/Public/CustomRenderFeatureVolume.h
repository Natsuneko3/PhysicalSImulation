// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CustomRenderFeatureComponent.h"
#include "CustomRenderFeatureViewExtension.h"
#include "GameFramework/Volume.h"
#include "CustomRenderFeatureVolume.generated.h"

UENUM(BlueprintType)
enum class ECustomRenderFeatureType : uint8
{
	PostProcessMaterial = 0,
	SNNFilter = 1,
	StylizationLine = 2,
	BilateralBlur = 3,
	Bloom = 4,
	SceneSpaceFluid = 5,
};

UCLASS(autoexpandcategories=ACustomRenderFeatureVolume, hidecategories=(Advanced, Collision, Volume, Brush, Attachment))
class CUSTOMRENDERFEATURE_API ACustomRenderFeatureVolume : public AVolume
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ACustomRenderFeatureVolume(const FObjectInitializer& ObjectInitializer);
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = CustomRenderFeature)
	TArray<TObjectPtr<URenderAdapterBase>> RenderFeatures;
protected:
	/** The component that defines the transform (location, rotation, scale) of this Actor in the world, all other components must be attached to this one somehow */
	UPROPERTY(Category = Collision, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCustomRenderFeatureComponent> CPPComponent;

	virtual void BeginPlay() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
private:
	TSharedPtr<FCPPViewExtension> ViewExtension;
};
