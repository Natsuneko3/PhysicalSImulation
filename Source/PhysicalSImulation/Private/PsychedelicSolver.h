#pragma once
#include "PhysicalSolver.h"

class FPsychedelicSolver :public FPhysicalSolverBase
{
public:
	FPsychedelicSolver(FPhysicalSimulationSceneProxy* InSceneProxy);
	~FPsychedelicSolver();
	virtual void Initial(FRHICommandListImmediate& RHICmdList) override;
	virtual void PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs) override;
	//void SolverPreesure(FRDGTextureRef InPressure);
	FIntPoint GridSize;
	FPhysicalSimulationSceneProxy* SceneProxy;
	TRefCountPtr<IPooledRenderTarget> SimulationTexturePool;
	TRefCountPtr<IPooledRenderTarget> PressureTexturePool;
};
