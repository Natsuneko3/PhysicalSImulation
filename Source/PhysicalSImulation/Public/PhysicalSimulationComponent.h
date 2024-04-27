// Copyright Natsu Neko, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/Actor.h"

#include "PhysicalSimulationViewExtension.h"
#include "Engine/TextureRenderTargetVolume.h"
#include "PhysicalSimulationComponent.generated.h"



UCLASS(Blueprintable, meta=(BlueprintSpawnableComponent))
class PHYSICALSIMULATION_API UPhysicalSimulationComponent : public UStaticMeshComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	UPhysicalSimulationComponent();
	~UPhysicalSimulationComponent();

	UPROPERTY(EditAnywhere,Category = "PhysicalSimulation",meta=(ClampMin = "8.0",UIMin = "8.0"))
	FIntVector GridSize = FIntVector(512,512,1);
	
	UPROPERTY(EditAnywhere,Category = "PhysicalSimulation",meta=(DisplayName="Simualtion"))
	bool bSimulation;

	UPROPERTY(EditAnywhere,Category = "PhysicalSimulation ")
	TSoftObjectPtr<UMaterialInterface> Material;

	UPROPERTY(EditAnywhere,Category = "PhysicalSimulation",meta = (DisplayName = "Use Physical Solver"))
	bool bUsePhysicalSolver;

	UPROPERTY(EditAnywhere,Category = "PhysicalSimulation", meta = (EditCondition = "bUsePhysicalSolver"))
	ESimulatorType SimulatorType = ESimulatorType::PlaneSmokeFluid;

	UPROPERTY(EditAnywhere,Category = "PhysicalSimulation")
	float Dx = 1;

	UPROPERTY(EditAnywhere,Category = "PhysicalSimulation|2D Smoke Fluid",meta = (EditCondition = "SimulatorType==ESimulatorType::PlaneSmokeFluid"))
	FPlandFluidParameters PlandFluidParameters;

	UPROPERTY(EditAnywhere,Category = "PhysicalSimulation|Liquid",meta = (EditCondition = "SimulatorType==ESimulatorType::Liquid"))
	FLiquidSolverParameter LiquidSolverParameter;
	UFUNCTION(BlueprintCallable,Category = "PhysicalSimulation")
	void Initial();

	virtual void BeginDestroy() override;

	UTextureRenderTargetVolume* TestTexture;
protected:
	

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;

private:

	void SetTextureToMaterial();

	TSharedPtr<FPhysicalSimulationViewExtension,ESPMode::ThreadSafe> PhysicalSolverViewExtension;

	//TArray<UTextureRenderTarget*> OutputTextures;


	
};
