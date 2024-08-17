// Copyright Natsu Neko, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Physical2DFluidSolver.h"
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
	TArray<TSoftObjectPtr<UMaterialInterface>> Materials;

	UPROPERTY(EditAnywhere,Category = "PhysicalSimulation")
	ESimulatorType SimulatorType = ESimulatorType::PlaneSmokeFluid;

	UPROPERTY(EditAnywhere,Category = "PhysicalSimulation")
	float Dx = 1;

	UPROPERTY(EditAnywhere,Category = "PhysicalSimulation|2D Smoke Fluid")
	FPlandFluidParameters PlandFluidParameters;

	UPROPERTY(EditAnywhere,Category = "PhysicalSimulation|Liquid")
	FLiquidSolverParameter LiquidSolverParameter;

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
