#pragma once
#include "PhysicalSolver.h"

class FPhysical3DFluidSolver: public FPhysicalSolverBase
{
public:
	FPhysical3DFluidSolver();

	virtual void SetParameter(FSolverParameter* InParameter) override;

	virtual void Update_RenderThread(FRDGBuilder& GraphBuilder,FPhysicalSolverContext* Context,FSceneView& InView) override;

	virtual void Initial(FPhysicalSolverContext* Context) override;


	bool bIsInitial = false;

	FFluidParameter* SolverParameter;
	FIntPoint GridSize;
	int32 Frame;

};
