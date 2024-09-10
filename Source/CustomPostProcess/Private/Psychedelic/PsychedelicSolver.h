#pragma once
#include "RenderAdapter.h"
#include "Engine/Texture.h"
#include "PsychedelicSolver.generated.h"

BEGIN_SHADER_PARAMETER_STRUCT(FSolverBaseParameter, CUSTOMRENDERFEATURE_API)
	SHADER_PARAMETER(FVector3f, GridSize)
	SHADER_PARAMETER(int, Time)
	SHADER_PARAMETER(float, dt)
	SHADER_PARAMETER(float, dx)
	SHADER_PARAMETER_SAMPLER(SamplerState, WarpSampler)
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FFluidParameter, CUSTOMRENDERFEATURE_API)
	SHADER_PARAMETER_STRUCT_INCLUDE(FSolverBaseParameter, SolverBaseParameter)
	SHADER_PARAMETER(float, VorticityMult)
	SHADER_PARAMETER(float, NoiseFrequency)
	SHADER_PARAMETER(float, NoiseIntensity)
	SHADER_PARAMETER(float, VelocityDissipate)
	SHADER_PARAMETER(float, DensityDissipate)
	SHADER_PARAMETER(float, GravityScale)
SHADER_PARAMETER(int, UseFFT)
SHADER_PARAMETER(FVector3f, WorldPosition)
SHADER_PARAMETER(FVector3f, WorldVelocity)
	SHADER_PARAMETER_TEXTURE(Texture2D, InTexture)
SHADER_PARAMETER_SAMPLER(SamplerState, SimSampler)
SHADER_PARAMETER_SAMPLER(SamplerState, InTextureSampler)
END_SHADER_PARAMETER_STRUCT()


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
	float DensityDissipate = 0.1;

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


	UPROPERTY(EditAnywhere,Category="Psychedelic")
	FPlandFluidParameters PlandFluidParameters;

private:
	TRefCountPtr<IPooledRenderTarget> SimulationTexturePool;
	TRefCountPtr<IPooledRenderTarget> PressureTexturePool;
	FMatrix PreViewProject;
};
