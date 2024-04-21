#pragma once
#include "PhysicalSolver.h"

class FPhysical3DFluidSolver: public FPhysicalSolverBase
{
public:
	FPhysical3DFluidSolver();

	virtual void SetParameter(FPhysicalSolverContext* Context) override;

	virtual void Update_RenderThread(FRDGBuilder& GraphBuilder,FSceneView& InView) override;

	virtual void Initial(FRHICommandListBase& RHICmdList) override;


	bool bIsInitial = false;

	FFluidParameter* SolverParameter;
	FIntPoint GridSize;
	int32 Frame;

};
