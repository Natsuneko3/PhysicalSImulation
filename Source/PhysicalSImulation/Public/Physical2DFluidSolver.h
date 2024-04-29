#pragma once
#include "PhysicalSolver.h"
#include "RenderGraphResources.h"
#include "Physical2DFluidSolver.generated.h"

USTRUCT(BlueprintType)
struct FPlandFluidParameters
{
	GENERATED_BODY()
	float VorticityMult = 100;
	float NoiseFrequency = 5;
	float NoiseIntensity = 50;
	float DensityDissipate = 0.2;
	float VelocityDissipate = 0.05;
	float GravityScale = 20;
};

class FPhysical2DFluidSolver : public FPhysicalSolverBase
{
public:
	FPhysical2DFluidSolver(FPhysicalSimulationSceneProxy* InSceneProxy);


	virtual void PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView) override;

	virtual void Initial(FRHICommandListImmediate& RHICmdList) override;
	virtual void Render_RenderThread(FPostOpaqueRenderParameters& Parameters) override;
	virtual void Release() override;
	bool bIsInitial = false;


	FIntPoint GridSize;
	FPhysicalSimulationSceneProxy* SceneProxy;

private:
	void SetSolverParameter(FFluidParameter& SolverParameter, FSceneView& InView);
	FRDGTextureRef SimulationTexture;
	FRDGTextureRef PressureTexture;
};
