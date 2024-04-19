// Copyright Natsu Neko, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PhysicalSimulationComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "PhysicalSimulationSystem.generated.h"

/**
 *
 */
UCLASS()
class PHYSICALSIMULATION_API UPhysicalSimulationSystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	// USubsystem implementation End

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	/** Called once all UWorldSubsystems have been initialized */
	virtual void PostInitialize() override;

	TSharedPtr<FPhysicalSimulationViewExtension> FindOrCreateViewExtension(UPhysicalSimulationComponent* InComponent);
	void RemoveViewExtension(FString InName);
	TMap<FString,TSharedPtr<FPhysicalSimulationViewExtension>> PhysicalSolverViewExtensions;
};
