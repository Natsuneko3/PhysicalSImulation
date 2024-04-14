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

TSharedPtr<FPhysicalSolverViewExtension> UPhysicalSimulationSystem::FindOrCreateViewExtension(FString InName, UPhysicalSimulationComponent* InComponent)
{

	/*if(PhysicalSolverViewExtensions.IsEmpty())
	{
		TSharedPtr<FPhysicalSolverViewExtension> PhysicalSolverViewExtension = FSceneViewExtensions::NewExtension<FPhysicalSolverViewExtension>(InComponent);
		PhysicalSolverViewExtensions.Add(InName,PhysicalSolverViewExtension);
		return PhysicalSolverViewExtension;
	}*/

	TSharedPtr<FPhysicalSolverViewExtension>* PhysicalSolverViewExtension = PhysicalSolverViewExtensions.Find(InName);
	if(!PhysicalSolverViewExtension)
	{
		UE_LOG(LogSimulation,Log,TEXT("%s:Create SceneViewExtensions"),*InName)
		TSharedPtr<FPhysicalSolverViewExtension> NewViewExtension = FSceneViewExtensions::NewExtension<FPhysicalSolverViewExtension>(InComponent);
		PhysicalSolverViewExtensions.Add(InName,NewViewExtension);
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
