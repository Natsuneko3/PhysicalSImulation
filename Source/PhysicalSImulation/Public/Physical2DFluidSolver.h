#pragma once
#include "PhysicalSolver.h"
#include "RenderGraphResources.h"
//#include "Physical2DFluid.generated.h"




class FPhysical2DFluidSolver :public FPhysicalSolverBase
{
public:
	FPhysical2DFluidSolver();

	virtual void SetParameter(FSolverParameter InParameter) override;

	virtual void Update_RenderThread(FRDGBuilder& GraphBuilder, TArray<FRDGTextureRef>& OutTextureArray,FPhysicalSolverContext Context) override;

	virtual void Initial(FRDGBuilder& GraphBuilder,FSolverParameter InParameter) override;
	bool bIsInitial = false;

	FFluidParameter SolverParameter;

	FIntPoint GridSize;
	int32 Frame;

};
