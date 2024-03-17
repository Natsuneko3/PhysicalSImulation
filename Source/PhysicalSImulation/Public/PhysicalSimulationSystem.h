// Copyright Natsu Neko, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/Actor.h"
#include "Physical2DFluidSolver.h"

#include "PhysicalSimulationSystem.generated.h"



DEFINE_LOG_CATEGORY_STATIC(LogSimulation, Log, All);
UCLASS(Blueprintable, meta=(BlueprintSpawnableComponent))
class PHYSICALSIMULATION_API UPhysicalSimulationComponent : public UStaticMeshComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	UPhysicalSimulationComponent();
	~UPhysicalSimulationComponent();

	UPROPERTY(EditAnywhere,meta=(ClampMin = "8.0",UIMin = "8.0"))
	FIntVector GridSize = FIntVector(512,512,1);
	
	UPROPERTY(EditAnywhere,meta=(DisplayName="Simualtion"))
	bool bSimulation;
	
	UPROPERTY(EditAnywhere,meta=(DisplayName="Vorticity Scale"))
	float VorticityMult = 500;

	UPROPERTY(EditAnywhere)
	float NoiseFrequency = 250;

	UPROPERTY(EditAnywhere)
	float NoiseIntensity = 1;

	UPROPERTY(EditAnywhere)
	float DensityDissipate = 1;

	UPROPERTY(EditAnywhere)
	float VelocityDissipate = 1;

	UPROPERTY(EditAnywhere)
	float GravityScale = 1;
	UFUNCTION(BlueprintCallable)
	void DoSimulation();
	
	UFUNCTION(BlueprintCallable)
	void Initial();

	UFUNCTION(BlueprintCallable)
	void InitialRenderTaget();

	UPROPERTY(BlueprintReadOnly)
	UTextureRenderTarget* SimulationTexture;

	UPROPERTY(BlueprintReadOnly)
	UTextureRenderTarget* PressureTexture;
	
	
protected:
	
	virtual void BeginPlay() override;
	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	void SetupSolverParameter();
	FSolverParameter SolverParameter;
	FPhysicalSolver* PhysicalSolver;
	FVector3f LastOnwerPosition;
	FVector3f CenterOnwerPosition;
	
	
};
