// Copyright Natsu Neko, Inc. All Rights Reserved.


#include "CustomPostProcessWorldSystem.h"

#include "SceneViewExtension.h"


void UCPPWorldSystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	CustomPostProcessViewExtension = FSceneViewExtensions::NewExtension<FCPPViewExtension>();
}

void UCPPWorldSystem::Deinitialize()
{
	Super::Deinitialize();
	CustomPostProcessViewExtension = nullptr;
}

bool UCPPWorldSystem::ShouldCreateSubsystem(UObject* Outer) const
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

void UCPPWorldSystem::PostInitialize()
{
	Super::PostInitialize();
}

void UCPPWorldSystem::AddSceneProxyToViewExtension(FCPPSceneProxy* InSceneProxy)
{

}
