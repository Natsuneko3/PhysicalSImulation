#pragma once
#include "RenderAdapter.h"
#include "PsychedelicSolver.generated.h"



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

UCLASS(NotBlueprintable, MinimalAPI)
class UPsychedelicSolver :public URenderAdapterBase
{
public:
	GENERATED_BODY()
	UPsychedelicSolver();
	~UPsychedelicSolver();
	virtual void Initial_RenderThread(FRHICommandListImmediate& RHICmdList) override;
	virtual void PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs) override;
	//void SolverPreesure(FRDGTextureRef InPressure);
	UPROPERTY(EditAnywhere)
	FIntPoint GridSize;

	UPROPERTY(EditAnywhere)
	FPlandFluidParameters PlandFluidParameters;

private:
	TRefCountPtr<IPooledRenderTarget> SimulationTexturePool;
	TRefCountPtr<IPooledRenderTarget> PressureTexturePool;
	FMatrix PreViewProject;
};
