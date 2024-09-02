// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CustomPostProcessComponent.h"
#include "RenderAdapter.h"
#include "GameFramework/Volume.h"
#include "CustomPostProcessVolume.generated.h"

UCLASS(autoexpandcategories=ACustomPostProcessVolume, hidecategories=(Advanced, Collision, Volume, Brush, Attachment))
class CUSTOMPOSTPROCESS_API ACustomPostProcessVolume : public AVolume
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ACustomPostProcessVolume();

	UPROPERTY(EditAnywhere)
	TArray<TSubclassOf<URenderAdapterBase>> Test;
protected:
	/** The component that defines the transform (location, rotation, scale) of this Actor in the world, all other components must be attached to this one somehow */
	UPROPERTY(Category = Collision, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCustomPostProcessComponent> CPPComponent;

	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};
