#pragma once
#include "PhysicalSolver.h"
#include "RenderGraphResources.h"
#include "Physical2DFluidSolver.generated.h"

USTRUCT(BlueprintType)
struct FPlandFluidParameters
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere,Category = "PlandFluidParameters")
	float VorticityMult = 100;

	UPROPERTY(EditAnywhere,Category = "PlandFluidParameters")
	float NoiseFrequency = 5;

	UPROPERTY(EditAnywhere,Category = "PlandFluidParameters")
	float NoiseIntensity = 50;

	UPROPERTY(EditAnywhere,Category = "PlandFluidParameters")
	float DensityDissipate = 0.2;

	UPROPERTY(EditAnywhere,Category = "PlandFluidParameters")
	float VelocityDissipate = 0.05;

	UPROPERTY(EditAnywhere,Category = "PlandFluidParameters")
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
