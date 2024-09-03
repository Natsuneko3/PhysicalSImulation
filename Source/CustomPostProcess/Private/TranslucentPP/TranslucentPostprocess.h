#pragma once
#include "RenderAdapter.h"
#include "TranslucentPostprocess.generated.h"

USTRUCT(BlueprintType)
struct FTranslucentParameterSet
{
	GENERATED_BODY();

	UPROPERTY(Category = CustomPostProcessVolume,EditAnywhere)
	TArray<UMaterial*> Materials;
};

//UCLASS()
class FTranslucentPostProcess: public FRenderAdapterBase
{
public:
	//GENERATED_BODY();
	FTranslucentPostProcess(FCPPSceneProxy* InSceneProxy,FTranslucentParameterSet* InParameterSet);

	FTranslucentParameterSet* Parameters;
	virtual void PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs) override;

	virtual void Initial_RenderThread(FRHICommandListImmediate& RHICmdList) override;

	bool bIsInitial = false;

	FIntPoint GridSize;
	int32 Frame;

};
