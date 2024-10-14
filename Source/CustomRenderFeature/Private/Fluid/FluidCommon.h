#pragma once
#include "Engine/Texture.h"
#include "ShaderParameterMacros.h"
#include "SceneView.h"
#include "FluidCommon.generated.h"

#define COMPILEFLUID 0
DECLARE_STATS_GROUP(TEXT("CustomRenderFeature"), STATGROUP_CustomRenderFeature, STATCAT_Advanced);

enum EShadertype
{
	PreVel,
	Advection,
	IteratePressure,
	PoissonFilter,
	ComputeDivergence
};

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
	float dx = 1.f;

	UPROPERTY(EditAnywhere,Category = "PlandFluidParameters")
	FVector3f GridSize = FVector3f(512.f);

	UPROPERTY(EditAnywhere,Category = "PlandFluidParameters")
	TObjectPtr<UTexture> InTexture1;
};

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

BEGIN_SHADER_PARAMETER_STRUCT(FSolverBaseParameter,)
	SHADER_PARAMETER(FVector3f, GridSize)
	SHADER_PARAMETER(int, Time)
	SHADER_PARAMETER(float, dt)
	SHADER_PARAMETER(float, dx)
	SHADER_PARAMETER_SAMPLER(SamplerState, WarpSampler)
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FFluidParameter, )
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