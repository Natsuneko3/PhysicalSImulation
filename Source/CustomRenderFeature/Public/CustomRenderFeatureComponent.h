// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RenderAdapter.h"
#include "Components/PrimitiveComponent.h"
#include "CustomRenderFeatureComponent.generated.h"


class ACustomRenderFeatureVolume;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
//UCLASS(Abstract, Blueprintable, EditInlineNew, CollapseCategories, Config = Input, defaultconfig, configdonotcheckdefaults)
class CUSTOMRENDERFEATURE_API UCustomRenderFeatureComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UCustomRenderFeatureComponent();

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = CustomRenderFeature)
	TArray<TObjectPtr<URenderAdapterBase>> RenderFeatures;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;


public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
};
