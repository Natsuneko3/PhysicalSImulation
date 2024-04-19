// Copyright Natsu Neko, Inc. All Rights Reserved.


#include "PhysicalSimulationSystem.h"

void UPhysicalSimulationSystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	PhysicalSolverViewExtensions.Empty();
}

void UPhysicalSimulationSystem::Deinitialize()
{
	Super::Deinitialize();
	PhysicalSolverViewExtensions.Empty();
	//UE_LOG(LogSimulation,Log,TEXT("222"))
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

TSharedPtr<FPhysicalSimulationViewExtension> UPhysicalSimulationSystem::FindOrCreateViewExtension(UPhysicalSimulationComponent* InComponent)
{

	/*if(PhysicalSolverViewExtensions.IsEmpty())
	{
		TSharedPtr<FPhysicalSimulationViewExtension> PhysicalSolverViewExtension = FSceneViewExtensions::NewExtension<FPhysicalSimulationViewExtension>(InComponent);
		PhysicalSolverViewExtensions.Add(InName,PhysicalSolverViewExtension);
		return PhysicalSolverViewExtension;
	}*/
	FString ComponentName = InComponent->GetName();
	TSharedPtr<FPhysicalSimulationViewExtension>* PhysicalSolverViewExtension = PhysicalSolverViewExtensions.Find(ComponentName);
	if(!PhysicalSolverViewExtension)
	{
		UE_LOG(LogSimulation,Log,TEXT("%s:Create SceneViewExtensions"),*ComponentName)
		TSharedPtr<FPhysicalSimulationViewExtension> NewViewExtension = FSceneViewExtensions::NewExtension<FPhysicalSimulationViewExtension>(InComponent);
		PhysicalSolverViewExtensions.Add(ComponentName,NewViewExtension);
		return NewViewExtension;
	}
	else
	{
		ENQUEUE_RENDER_COMMAND(UpdateViewScene)(
			[PhysicalSolverViewExtension,InComponent](FRHICommandListImmediate& RHICmdList)
				{
					PhysicalSolverViewExtension->Get()->UpdateParameters(InComponent);
				});

	}
	return *PhysicalSolverViewExtension;

}

void UPhysicalSimulationSystem::RemoveViewExtension(FString InName)
{
	PhysicalSolverViewExtensions.Remove(InName);
}
