// Fill out your copyright notice in the Description page of Project Settings.


#include "CustomRenderFeatureComponent.h"
#include "CustomRenderFeatureSceneProxy.h"

UCustomRenderFeatureComponent::UCustomRenderFeatureComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// Called when the game starts
void UCustomRenderFeatureComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}

FPrimitiveSceneProxy* UCustomRenderFeatureComponent::CreateSceneProxy()
{
	return new FCPPSceneProxy(this);
}


// Called every frame
void UCustomRenderFeatureComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

