// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RenderAdapter.h"
#include "Components/PrimitiveComponent.h"
#include "CustomPostProcessComponent.generated.h"


class ACustomPostProcessVolume;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
//UCLASS(Abstract, Blueprintable, EditInlineNew, CollapseCategories, Config = Input, defaultconfig, configdonotcheckdefaults)
class CUSTOMPOSTPROCESS_API UCustomPostProcessComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UCustomPostProcessComponent();

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = CustomPostProcess)
	TArray<TObjectPtr<URenderAdapterBase>> RenderFeatures;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;


public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
};
