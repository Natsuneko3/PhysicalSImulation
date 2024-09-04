#pragma once
#include "RenderAdapter.h"
#include "TranslucentPostprocess.generated.h"


UCLASS(NotBlueprintable, MinimalAPI)
class UTranslucentPostProcess: public URenderAdapterBase
{
public:
	GENERATED_BODY()
	UTranslucentPostProcess();

	UPROPERTY(Category = CustomPostProcess,EditAnywhere)
	TArray<UMaterial*> Materials;

	UPROPERTY(Category = CustomPostProcess,EditAnywhere)
	bool bOnlyTranslucentPass;
	virtual void PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs) override;

	virtual void Initial_RenderThread(FRHICommandListImmediate& RHICmdList) override;

	bool bIsInitial = false;

	FIntPoint GridSize;


};
