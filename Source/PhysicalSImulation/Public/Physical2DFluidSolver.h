#pragma once
#include "PhysicalSolver.h"
#include "RenderGraphResources.h"
//#include "Physical2DFluid.generated.h"


class FPhysical2DFluidSolver :public FPhysicalSolverBase
{
public:
	FPhysical2DFluidSolver();

	virtual void SetParameter(FSolverParameter* InParameter) override;

	virtual void Update_RenderThread(FRDGBuilder& GraphBuilder,FPhysicalSolverContext* Context) override;

	virtual void Initial(FPhysicalSolverContext* Context) override;


	bool bIsInitial = false;

	FFluidParameter* SolverParameter;
	FIntPoint GridSize;
	int32 Frame;
private:
	TRefCountPtr<IPooledRenderTarget> SimulatorTextureRT;
	TRefCountPtr<IPooledRenderTarget> PressureTextureRT;


};
