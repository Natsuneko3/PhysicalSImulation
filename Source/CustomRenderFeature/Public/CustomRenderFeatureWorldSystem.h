// Copyright Natsu Neko, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CustomRenderFeatureSceneProxy.h"
#include "Subsystems/WorldSubsystem.h"
#include "CustomRenderFeatureWorldSystem.generated.h"

/**
 *
 */
UCLASS()
class CUSTOMRENDERFEATURE_API UCPPWorldSystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	// USubsystem implementation End

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	/** Called once all UWorldSubsystems have been initialized */
	virtual void PostInitialize() override;
	void AddSceneProxyToViewExtension(FCPPSceneProxy* InSceneProxy);
	TSharedPtr<FCPPViewExtension> CustomRenderFeatureViewExtension;
	TArray<FVector> OutParticle;
};
