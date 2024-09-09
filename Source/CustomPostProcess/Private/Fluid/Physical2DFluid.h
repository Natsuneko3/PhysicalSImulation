#pragma once
#include "RenderAdapter.h"


USTRUCT(BlueprintType)
struct FPlandFluidParameters
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere,Category = "PlandFluidParameters")
	float VorticityMult = 10;

	UPROPERTY(EditAnywhere,Category = "PlandFluidParameters")
	float NoiseFrequency = 3;

	UPROPERTY(EditAnywhere,Category = "PlandFluidParameters")
	float NoiseIntensity = 100;

	UPROPERTY(EditAnywhere,Category = "PlandFluidParameters")
	float DensityDissipate = 0.5;

	UPROPERTY(EditAnywhere,Category = "PlandFluidParameters")
	float VelocityDissipate = 0.05;

	UPROPERTY(EditAnywhere,Category = "PlandFluidParameters")
	float GravityScale = 10;

	UPROPERTY(EditAnywhere,Category = "PlandFluidParameters")
	TObjectPtr<UTexture> InTexture1;
};

class FPhysical2DFluidSolver : public URenderAdapterBase
{
public:
	FPhysical2DFluidSolver();
	~FPhysical2DFluidSolver();


	virtual void PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView) override;

	virtual void Initial_RenderThread(FRHICommandListImmediate& RHICmdList) override;

	virtual void PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs) override;
	virtual void Release() override;
	bool bIsInitial = false;


	FIntPoint GridSize;

private:
	TRefCountPtr<IPooledRenderTarget> SimulationTexturePool;
	TRefCountPtr<IPooledRenderTarget> PressureTexturePool;

};
