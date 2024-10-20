
#pragma once
#include "FluidCommon.h"
#include "RenderAdapter.h"
#include "Physical2DFluid.generated.h"
UCLASS(NotBlueprintable, MinimalAPI,DisplayName="2dFluid")
class UPhysical2DFluidSolver : public URenderAdapterBase
{
public:
	GENERATED_BODY()
	UPhysical2DFluidSolver();
	~UPhysical2DFluidSolver();

	UPROPERTY(EditAnywhere,Category="Psychedelic")
	FPlandFluidParameters PlandFluidParameters;

	virtual void PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView) override;

	virtual void Initial_RenderThread(FRHICommandListImmediate& RHICmdList) override;

	virtual void PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs) override;
	virtual void Release() override;
	bool bIsInitial = false;


	FIntPoint GridSize;

private:
	void SetSolverParameter(FFluidParameter& SolverParameter, FSceneView& InView);
	TRefCountPtr<IPooledRenderTarget> SimulationTexturePool;
	TRefCountPtr<IPooledRenderTarget> PressureTexturePool;

};

