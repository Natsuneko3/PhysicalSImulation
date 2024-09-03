// Fill out your copyright notice in the Description page of Project Settings.
#include "CustomPostProcessVolume.h"
//#include UE_INLINE_GENERATED_CPP_BY_NAME(CustomPostProcess)
ACustomPostProcessVolume::ACustomPostProcessVolume(const FObjectInitializer& ObjectInitializer): Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	CPPComponent = CreateDefaultSubobject<UCustomPostProcessComponent>(TEXT("CustomPostProcessComponent"));
	SetRootComponent(CPPComponent);
	CPPComponent->SetPostProcessVolume(this);
}

// Called when the game starts or when spawned
void ACustomPostProcessVolume::BeginPlay()
{

}

// Called every frame
void ACustomPostProcessVolume::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

