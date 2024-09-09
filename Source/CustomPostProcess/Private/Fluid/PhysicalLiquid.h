#pragma once
#include "PhysicalSolver.h"
#include "RHIGPUReadback.h"
#include "PhysicalLiquidSolver.generated.h"

BEGIN_SHADER_PARAMETER_STRUCT(FLiuquidParameter, PHYSICALSIMULATION_API)
	SHADER_PARAMETER_STRUCT_INCLUDE(FSolverBaseParameter, SolverBaseParameter)
	SHADER_PARAMETER(float, GravityScale)
	SHADER_PARAMETER(float, LifeTime)
END_SHADER_PARAMETER_STRUCT()


USTRUCT(BlueprintType)
struct FLiquidSolverParameter
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "LiquidSolverParameter")
	float SpawnRate = 60;

	UPROPERTY(EditAnywhere, Category = "LiquidSolverParameter")
	float LifeTime = 2;

	UPROPERTY(EditAnywhere, Category = "LiquidSolverParameter")
	float GravityScale = 20;
};

//class FPhysicalSimulationSceneProxy;
class FPhysicalLiquidSolver : public FPhysicalSolverBase
{
public:
	FPhysicalLiquidSolver(FPhysicalSimulationSceneProxy* InSceneProxy);
	~FPhysicalLiquidSolver();

	void SetLiuquidParameter(FLiuquidParameter& Parameter, FSceneView& InView,FPhysicalSimulationSceneProxy* InSceneProxy);

	virtual void PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView) override;
virtual void PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs) override;
	virtual void Initial_RenderThread(FRHICommandListImmediate& RHICmdList) override;
	virtual void Release() override;

	bool bIsInitial = false;

	FIntVector GridSize;
	int LastNumParticle;
	uint32 DeadParticle;

private:
	int32 AllocatedInstanceCounts = 0;

	TRefCountPtr<FRDGPooledBuffer> ParticleAttributeBufferPool;
	TRefCountPtr<IPooledRenderTarget> RasterizeTexturePool;
	FRHIGPUBufferReadback* ParticleReadback = nullptr;
	FRHIGPUBufferReadback* ParticleIDReadback = nullptr;

};
