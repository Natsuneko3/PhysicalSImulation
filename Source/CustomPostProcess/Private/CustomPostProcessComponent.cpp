// Fill out your copyright notice in the Description page of Project Settings.


#include "CustomPostProcessComponent.h"
#include "CustomPostProcessSceneProxy.h"

UCustomPostProcessComponent::UCustomPostProcessComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// Called when the game starts
void UCustomPostProcessComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}

FPrimitiveSceneProxy* UCustomPostProcessComponent::CreateSceneProxy()
{
	return new FCPPSceneProxy(this);
}


// Called every frame
void UCustomPostProcessComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

