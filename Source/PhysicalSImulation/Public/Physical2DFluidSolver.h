#pragma once
#include "PhysicalSolver.h"
#include "RenderGraphResources.h"
//#include "Physical2DFluid.generated.h"

//USTRUCT(BlueprintType)
struct FPlandFluidParameters
{
	//GENERATED_BODY()
	float VorticityMult = 100;
	float NoiseFrequency = 5;
	float NoiseIntensity = 50;
	float DensityDissipate = 0.2;
	float VelocityDissipate = 0.05;
	float GravityScale = 20;
};

class FPhysical2DFluidSolver :public FPhysicalSolverBase
{
public:
	FPhysical2DFluidSolver();


	virtual void SetParameter(FPhysicalSimulationSceneProxy* InSceneProxy) override;

	virtual void Update_RenderThread(FRDGBuilder& GraphBuilder,FSceneView& InView) override;

	virtual void Initial(FRHICommandListBase& RHICmdList) override;

	virtual void Release() override;
	bool bIsInitial = false;

	FFluidParameter* SolverParameter;
	FIntPoint GridSize;
	FPhysicalSimulationSceneProxy* SceneProxy;
	//FPhysicalSolverContext* Context;

};
