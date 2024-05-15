#pragma once
#include "PhysicalSolver.h"

class FRadianceCascadesSolver : public FPhysicalSolverBase
{
public:
	FRadianceCascadesSolver(FPhysicalSimulationSceneProxy* InScaneProxy);
	~FRadianceCascadesSolver();
	virtual void Initial_RenderThread(FRHICommandListImmediate& RHICmdList) override;
	virtual void PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs) override;
};
