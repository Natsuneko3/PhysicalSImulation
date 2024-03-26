// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
//#include "PhysicalSolver.generated.h"

/**
 * 
 */
BEGIN_SHADER_PARAMETER_STRUCT(FSolverBaseParameter, PHYSICALSIMULATION_API)
SHADER_PARAMETER(FVector3f,GridSize)
SHADER_PARAMETER(int32,Time)
SHADER_PARAMETER(float,dt)
SHADER_PARAMETER(float,dx)


SHADER_PARAMETER_SAMPLER(SamplerState, WarpSampler)
SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)

END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FFluidParameter, PHYSICALSIMULATION_API)
	SHADER_PARAMETER_STRUCT_INCLUDE(FSolverBaseParameter, SolverBaseParameter)
	SHADER_PARAMETER(float,VorticityMult)
	SHADER_PARAMETER(float,NoiseFrequency)
	SHADER_PARAMETER(float,NoiseIntensity)
	SHADER_PARAMETER(float,VelocityDissipate)
	SHADER_PARAMETER(float,DensityDissipate)
	SHADER_PARAMETER(float,GravityScale)
SHADER_PARAMETER(int,UseFFT)
END_SHADER_PARAMETER_STRUCT()

UENUM()
enum class ESimulatorType : uint8
{
	PlaneSmokeFluid = 0,
	CubeSmokeFluid = 1,
	Water = 2
};

struct FSolverParameter
{
	
	FFluidParameter FluidParameter;
	
};

struct FPhysicalSolverContext
{
	FPhysicalSolverContext()
	{
		WorldVelocity = FVector3f(0);
		WorldPosition = FVector3f(0);
	};

	ERHIFeatureLevel::Type FeatureLevel;
	FVector3f WorldVelocity;
	FVector3f WorldPosition;
	FSolverParameter SolverParameter;
	UStaticMesh* BoundingMesh;
	UMaterialInterface* MeshMaterial;
	ESimulatorType SimulatorType;
};
class FPhysicalSolverBase
{

public:
	virtual void SetParameter(FSolverParameter InParameter){}
	virtual void Initial(FRDGBuilder& GraphBuilder,FSolverParameter InParameter){}
	virtual void Update_RenderThread(FRDGBuilder& GraphBuilder,TArray<FRDGTextureRef>& OutTextureArray,FPhysicalSolverContext Context){}
	
};
