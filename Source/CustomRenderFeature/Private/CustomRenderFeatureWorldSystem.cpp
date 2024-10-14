// Copyright Natsu Neko, Inc. All Rights Reserved.


#include "CustomRenderFeatureWorldSystem.h"

#include "SceneViewExtension.h"


void UCPPWorldSystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	CustomRenderFeatureViewExtension = FSceneViewExtensions::NewExtension<FCPPViewExtension>();
}

void UCPPWorldSystem::Deinitialize()
{
	Super::Deinitialize();
	CustomRenderFeatureViewExtension = nullptr;
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
