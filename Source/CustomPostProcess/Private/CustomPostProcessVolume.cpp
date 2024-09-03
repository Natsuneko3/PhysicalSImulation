// Fill out your copyright notice in the Description page of Project Settings.
#include "CustomPostProcessVolume.h"

// Sets default values
ACustomPostProcessVolume::ACustomPostProcessVolume()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
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

