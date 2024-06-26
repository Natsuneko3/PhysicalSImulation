#pragma once
#include "PhysicalSolver.h"

class SNNSolver : public FPhysicalSolverBase
{
public:
	/*SNNSolver(FPhysicalSimulationSceneProxy* InSceneProxy);
	~SNNSolver();
	virtual void Initial_RenderThread(FRHICommandListImmediate& RHICmdList) override;
	virtual void PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs) override;
	//void SolverPreesure(FRDGTextureRef InPressure);
	FIntPoint GridSize;
	FPhysicalSimulationSceneProxy* SceneProxy;
	TRefCountPtr<IPooledRenderTarget> SimulationTexturePool;
	TRefCountPtr<IPooledRenderTarget> PressureTexturePool;
	FMatrix PreViewProject;*/
};
