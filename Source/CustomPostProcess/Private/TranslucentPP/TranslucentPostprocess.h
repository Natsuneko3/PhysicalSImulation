#pragma once
#include "RenderAdapter.h"
#include "TranslucentPostprocess.generated.h"


UCLASS(MinimalAPI)
class UTranslucentPostProcess: public URenderAdapterBase
{
public:
	GENERATED_BODY()
	UTranslucentPostProcess();

	UPROPERTY(Category = CustomPostProcessVolume,EditAnywhere)
	TArray<UMaterial*> Materials;

	virtual void PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs) override;

	virtual void Initial_RenderThread(FRHICommandListImmediate& RHICmdList) override;

	bool bIsInitial = false;

	FIntPoint GridSize;
	int32 Frame;

};
