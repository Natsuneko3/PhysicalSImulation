// Copyright Natsu Neko, Inc. All Rights Reserved.


#include "PhysicalSimulationSystem.h"

void UPhysicalSimulationSystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	PhysicalSolverViewExtension = FSceneViewExtensions::NewExtension<FPhysicalSimulationViewExtension>();
}

void UPhysicalSimulationSystem::Deinitialize()
{
	Super::Deinitialize();
	PhysicalSolverViewExtension = nullptr;
}

bool UPhysicalSimulationSystem::ShouldCreateSubsystem(UObject* Outer) const
{
	bool bShouldCreateSubsystem = Super::ShouldCreateSubsystem(Outer);

	if (Outer)
	{
		if (UWorld* World = Outer->GetWorld())
		{
			bShouldCreateSubsystem = DoesSupportWorldType(World->WorldType) && bShouldCreateSubsystem;
		}
	}

	return bShouldCreateSubsystem;
}

void UPhysicalSimulationSystem::PostInitialize()
{
	Super::PostInitialize();
}

void UPhysicalSimulationSystem::AddSceneProxyToViewExtension(FPhysicalSimulationSceneProxy* InSceneProxy)
{

}
