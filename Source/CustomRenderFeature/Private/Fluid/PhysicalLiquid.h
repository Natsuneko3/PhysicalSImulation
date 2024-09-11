
#pragma once

#include "FluidCommon.h"
#if COMPILEFLUID
#include "CustomRenderFeatureSceneProxy.h"
#include "RHIGPUReadback.h"
#include "RenderAdapter.h"
#include "Psychedelic/PsychedelicSolver.h"
//#include "PhysicalLiquidSolver.generated.h"

BEGIN_SHADER_PARAMETER_STRUCT(FLiuquidParameter, PHYSICALSIMULATION_API)
	SHADER_PARAMETER_STRUCT_INCLUDE(FSolverBaseParameter, SolverBaseParameter)
	SHADER_PARAMETER(float, GravityScale)
	SHADER_PARAMETER(float, LifeTime)
END_SHADER_PARAMETER_STRUCT()



//class FPhysicalSimulationSceneProxy;
class FPhysicalLiquidSolver : public URenderAdapterBase
{
public:
	FPhysicalLiquidSolver();
	~FPhysicalLiquidSolver();

	void SetLiuquidParameter(FLiuquidParameter& Parameter, FSceneView& InView);

	virtual void PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView) override;
	virtual void PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs) override;
	virtual void Initial_RenderThread(FRHICommandListImmediate& RHICmdList) override;
	virtual void Release() override;

	UPROPERTY(EditAnywhere,Category="PhysicalLiquidSolver")
	FLiquidSolverParameter LiquidSolverParameter;
	bool bIsInitial = false;

	FIntVector GridSize;
	int LastNumParticle;
	uint32 DeadParticle;
	FCPPSceneProxy* InSceneProxy;
private:
	int32 AllocatedInstanceCounts = 0;

	TRefCountPtr<FRDGPooledBuffer> ParticleAttributeBufferPool;
	TRefCountPtr<IPooledRenderTarget> RasterizeTexturePool;
	FRHIGPUBufferReadback* ParticleReadback = nullptr;
	FRHIGPUBufferReadback* ParticleIDReadback = nullptr;

};
#endif