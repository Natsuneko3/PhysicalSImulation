#pragma once
#include "PhysicalSolver.h"
#include "RHIGPUReadback.h"


DEFINE_LOG_CATEGORY_STATIC(LogSimulation, Log, All);
class FPhysicalLiquidSolver:public FPhysicalSolverBase
{
	public:
	FPhysicalLiquidSolver();

	virtual void SetParameter(FSolverParameter* InParameter) override;

	virtual void Update_RenderThread(FRDGBuilder& GraphBuilder,FPhysicalSolverContext* Context,FSceneView& InView) override;

	virtual void Initial(FPhysicalSolverContext* Context) override;
	virtual void Release() override;
	void PostSimulation();

	bool bIsInitial = false;

	FFluidParameter* SolverParameter;
	FIntPoint GridSize;
	int32 Frame;
	uint32 CurrentNumParticle;
	uint32 DeadParticle;
private:
	int32 AllocatedInstanceCounts = 0;
	FRWBuffer ParticleIDBuffer;
	FRWBuffer ParticleAttributeBuffer;
	FRHIGPUMemoryReadback* ParticleReadback = nullptr;
	void EnqueueGPUReadback(FRHICommandListImmediate& RHICmdList);
};
