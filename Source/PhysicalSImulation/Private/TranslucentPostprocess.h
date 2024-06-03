#pragma once
#include "PhysicalSolver.h"

class FTranslucentPostProcess: public FPhysicalSolverBase
{
public:
	FTranslucentPostProcess(FPhysicalSimulationSceneProxy* InSceneProxy);

	virtual void PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs) override;

	virtual void Initial_RenderThread(FRHICommandListImmediate& RHICmdList) override;


	bool bIsInitial = false;

	FFluidParameter* SolverParameter;
	FIntPoint GridSize;
	int32 Frame;

};
