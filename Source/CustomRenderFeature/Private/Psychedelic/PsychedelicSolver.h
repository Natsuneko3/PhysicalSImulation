#pragma once
#include "RenderAdapter.h"
#include "Engine/Texture.h"
#include "Fluid/FluidCommon.h"
#include "PsychedelicSolver.generated.h"



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
