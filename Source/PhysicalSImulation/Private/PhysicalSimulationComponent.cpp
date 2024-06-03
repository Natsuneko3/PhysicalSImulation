// Copyright Natsu Neko, Inc. All Rights Reserved.

#include "PhysicalSimulationComponent.h"
#include "Physical2DFluidSolver.h"
#include "PhysicalSimulationSceneProxy.h"
#include "PhysicalSimulationSystem.h"

#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "SceneViewExtension.h"
#include "Engine/TextureRenderTargetVolume.h"


// Sets default values
UPhysicalSimulationComponent::UPhysicalSimulationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	bTickInEditor = true;
	if (!GetStaticMesh())
	{
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane"));
		SetStaticMesh(Mesh);
	}

}

UPhysicalSimulationComponent::~UPhysicalSimulationComponent()
{
	PhysicalSolverViewExtension = nullptr;
}


void UPhysicalSimulationComponent::BeginDestroy()
{
	Super::BeginDestroy();
	/*if (GetWorld())
	{
		UPhysicalSimulationSystem* SubSystem = GetWorld()->GetSubsystem<UPhysicalSimulationSystem>();
		if (SubSystem)
		{
			SubSystem->RemoveViewExtension(GetUniqueID());
		}
	}*/
}

void UPhysicalSimulationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UPhysicalSimulationSystem* SubSystem = GetWorld()->GetSubsystem<UPhysicalSimulationSystem>();
	if (SubSystem && SubSystem->OutParticle.Num() > 10)
	{
		for (int i = 0; i < SubSystem->OutParticle.Num(); i++)
		{
			DrawDebugPoint(GetWorld(), SubSystem->OutParticle[i], 10, FColor::Green);
		}
		//UE_LOG(LogTemp,Log,TEXT("DebugNum: %i"), SubSystem->OutParticle.Num());
	}
}

void UPhysicalSimulationComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	/*FPhysicalSimulationSceneProxy* PSSceneProxy = (FPhysicalSimulationSceneProxy*)SceneProxy;
	PSSceneProxy->UpdateParameters(this);*/
}

FPrimitiveSceneProxy* UPhysicalSimulationComponent::CreateSceneProxy()
{
	return new FPhysicalSimulationSceneProxy(this);
}

void UPhysicalSimulationComponent::SetTextureToMaterial()
{

}
