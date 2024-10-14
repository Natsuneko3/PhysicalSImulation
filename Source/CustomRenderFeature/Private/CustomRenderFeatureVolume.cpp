// Fill out your copyright notice in the Description page of Project Settings.
#include "CustomRenderFeatureVolume.h"

#include "CustomRenderFeatureWorldSystem.h"
//#include UE_INLINE_GENERATED_CPP_BY_NAME(CustomRenderFeature)
ACustomRenderFeatureVolume::ACustomRenderFeatureVolume(const FObjectInitializer& ObjectInitializer): Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	CPPComponent = CreateDefaultSubobject<UCustomRenderFeatureComponent>(TEXT("CustomRenderFeatureComponent"));
	CPPComponent->RenderFeatures = RenderFeatures;
	SetRootComponent(CPPComponent);
}

// Called when the game starts or when spawned
void ACustomRenderFeatureVolume::BeginPlay()
{

}

#if WITH_EDITOR
void ACustomRenderFeatureVolume::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	CPPComponent->RenderFeatures = RenderFeatures;
}
#endif

// Called every frame
void ACustomRenderFeatureVolume::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

