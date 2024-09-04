// Copyright 2023 Natsu Neko, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RenderAdapter.h"
#include "SceneViewExtension.h"
#include "DualKawaseBlur.generated.h"

UCLASS(NotBlueprintable, MinimalAPI,DisplayName="Blur-DualKawase")

class  UDualKawaseBlur : public URenderAdapterBase
{
	GENERATED_BODY()
public:
	UDualKawaseBlur();

	virtual void PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs) override;

	UPROPERTY(EditAnywhere,meta=(DisplayName="Blur Size"))
	float Offset = 1.0;

	UPROPERTY(EditAnywhere)
	bool bUseGaussianPass;

	UPROPERTY(EditAnywhere)
	int PassNum = 4;

};
