#include "Fluid/FluidCommon.h"
#if COMPILEFLUID
#pragma once
#include "RenderAdapter.h"

class UPhysical2DFluidSolver : public URenderAdapterBase
{
public:
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
#endif
