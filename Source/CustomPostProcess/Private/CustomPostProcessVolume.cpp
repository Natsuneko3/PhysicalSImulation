// Fill out your copyright notice in the Description page of Project Settings.
#include "CustomPostProcessVolume.h"

#include "CustomPostProcessWorldSystem.h"
//#include UE_INLINE_GENERATED_CPP_BY_NAME(CustomPostProcess)
ACustomPostProcessVolume::ACustomPostProcessVolume(const FObjectInitializer& ObjectInitializer): Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	CPPComponent = CreateDefaultSubobject<UCustomPostProcessComponent>(TEXT("CustomPostProcessComponent"));
	CPPComponent->RenderFeatures = RenderFeatures;
	SetRootComponent(CPPComponent);
}

// Called when the game starts or when spawned
void ACustomPostProcessVolume::BeginPlay()
{

}

#if WITH_EDITOR
void ACustomPostProcessVolume::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	CPPComponent->RenderFeatures = RenderFeatures;
}
#endif

// Called every frame
void ACustomPostProcessVolume::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

