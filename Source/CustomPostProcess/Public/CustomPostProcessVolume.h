// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CustomPostProcessComponent.h"
#include "CustomPostProcessViewExtension.h"
#include "GameFramework/Volume.h"
#include "CustomPostProcessVolume.generated.h"

UENUM(BlueprintType)
enum class ECustomPostProcessType : uint8
{
	PostProcessMaterial = 0,
	SNNFilter = 1,
	StylizationLine = 2,
	BilateralBlur = 3,
	Bloom = 4,
	SceneSpaceFluid = 5,
};

UCLASS(autoexpandcategories=ACustomPostProcessVolume, hidecategories=(Advanced, Collision, Volume, Brush, Attachment))
class CUSTOMPOSTPROCESS_API ACustomPostProcessVolume : public AVolume
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ACustomPostProcessVolume(const FObjectInitializer& ObjectInitializer);
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = CustomPostProcess)
	TArray<TObjectPtr<URenderAdapterBase>> RenderFeatures;
protected:
	/** The component that defines the transform (location, rotation, scale) of this Actor in the world, all other components must be attached to this one somehow */
	UPROPERTY(Category = Collision, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCustomPostProcessComponent> CPPComponent;

	virtual void BeginPlay() override;

private:
	TSharedPtr<FCPPViewExtension> ViewExtension;
};
