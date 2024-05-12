#pragma once
#include "PhysicalSolver.h"
#include "Physical2DFluidSolver.generated.h"

enum EShadertype
{
	PreVel,
	Advection,
	IteratePressure,
	ComputeDivergence
};



USTRUCT(BlueprintType)
struct FPlandFluidParameters
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere,Category = "PlandFluidParameters")
	float VorticityMult = 10;

	UPROPERTY(EditAnywhere,Category = "PlandFluidParameters")
	float NoiseFrequency = 4;

	UPROPERTY(EditAnywhere,Category = "PlandFluidParameters")
	float NoiseIntensity = 50;

	UPROPERTY(EditAnywhere,Category = "PlandFluidParameters")
	float DensityDissipate = 0.05;

	UPROPERTY(EditAnywhere,Category = "PlandFluidParameters")
	float VelocityDissipate = 0.05;

	UPROPERTY(EditAnywhere,Category = "PlandFluidParameters")
	float GravityScale = 10;
};

class FPhysical2DFluidSolver : public FPhysicalSolverBase
{
public:
	FPhysical2DFluidSolver(FPhysicalSimulationSceneProxy* InSceneProxy);
	~FPhysical2DFluidSolver();


	virtual void PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView) override;

	virtual void Initial(FRHICommandListImmediate& RHICmdList) override;
	virtual void Render_RenderThread(FPostOpaqueRenderParameters& Parameters) override;
	virtual void PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs) override;
	virtual void Release() override;
	bool bIsInitial = false;


	FIntPoint GridSize;
	FPhysicalSimulationSceneProxy* SceneProxy;

private:
	void SetSolverParameter(FFluidParameter& SolverParameter, FSceneView& InView);
	TRefCountPtr<IPooledRenderTarget> SimulationTexturePool;
	TRefCountPtr<IPooledRenderTarget> PressureTexturePool;
	/*FRDGTextureRef SimulationTexture;
	FRDGTextureRef PressureTexture;*/
};
