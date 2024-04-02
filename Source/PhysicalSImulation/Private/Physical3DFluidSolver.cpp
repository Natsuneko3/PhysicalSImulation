#include "Physical3DFluidSolver.h"

FPhysical3DFluidSolver::FPhysical3DFluidSolver()
{
}

void FPhysical3DFluidSolver::SetParameter(FSolverParameter* InParameter)
{
	FPhysicalSolverBase::SetParameter(InParameter);
}

void FPhysical3DFluidSolver::Update_RenderThread(FRDGBuilder& GraphBuilder,FPhysicalSolverContext* Context,FSceneView& InView)
{
	FPhysicalSolverBase::Update_RenderThread(GraphBuilder, Context,InView);
}

void FPhysical3DFluidSolver::Initial(FPhysicalSolverContext* Context)
{
	FPhysicalSolverBase::Initial(Context);
}
