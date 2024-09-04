// Copyright 2023 Natsu Neko, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RenderAdapter.h"
#include "SceneViewExtension.h"
#include "DualKawaseBlur.generated.h"

UCLASS(NotBlueprintable, MinimalAPI)

class  UDualKawaseBlur : public URenderAdapterBase
{
	GENERATED_BODY()
public:
	UDualKawaseBlur();

	virtual void PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs) override;

	float Offset;
	bool bUseGaussianPass;
	int PassNum = 1;

};
