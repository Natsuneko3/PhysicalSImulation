// Copyright Natsu Neko, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/Actor.h"

#include "PhysicalSolverViewExtension.h"
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

	/*UPROPERTY(EditAnywhere,Category = "SimulatorMesh",meta=(DisplayName="Mesh"))
	TSoftObjectPtr<UStaticMesh> StaticMesh;*/

	UPROPERTY(EditAnywhere,Category = "PhysicalSimulation ")
	TSoftObjectPtr<UMaterialInterface> Material;

	UPROPERTY(EditAnywhere,Category = "PhysicalSimulation")
	ESimulatorType SimulatorType = ESimulatorType::PlaneSmokeFluid;

	UPROPERTY(EditAnywhere,Category = "PhysicalSimulation|2D Smoke Fluid",meta = (EditCondition = "SimulatorType==ESimulatorType::PlaneSmokeFluid"))
	float VorticityMult = 100;

	UPROPERTY(EditAnywhere,Category = "PhysicalSimulation|2D Smoke Fluid",meta = (EditCondition = "SimulatorType==ESimulatorType::PlaneSmokeFluid"))
	float NoiseFrequency = 5;

	UPROPERTY(EditAnywhere,Category = "PhysicalSimulation|2D Smoke Fluid",meta = (EditCondition = "SimulatorType==ESimulatorType::PlaneSmokeFluid"))
	float NoiseIntensity = 50;

	UPROPERTY(EditAnywhere,Category = "PhysicalSimulation|2D Smoke Fluid",meta = (EditCondition = "SimulatorType==ESimulatorType::PlaneSmokeFluid"))
	float DensityDissipate = 0.2;

	UPROPERTY(EditAnywhere,Category = "PhysicalSimulation|2D Smoke Fluid",meta = (EditCondition = "SimulatorType==ESimulatorType::PlaneSmokeFluid"))
	float VelocityDissipate = 0.05;

	UPROPERTY(EditAnywhere,Category = "PhysicalSimulation|2D Smoke Fluid",meta = (EditCondition = "SimulatorType==ESimulatorType::PlaneSmokeFluid"))
	float GravityScale = 20;

	UPROPERTY(EditAnywhere,Category = "PhysicalSimulation|Liquid",meta = (EditCondition = "SimulatorType==ESimulatorType::Liquid"))
	float SpawnRate = 60;

	UPROPERTY(EditAnywhere,Category = "PhysicalSimulation|Liquid",meta = (EditCondition = "SimulatorType==ESimulatorType::Liquid"))
	float LifeTime = 2;

	UFUNCTION(BlueprintCallable,Category = "PhysicalSimulation")
	void Initial();

	void InitializeComponent() override;
	virtual void BeginDestroy() override;
	/*UPROPERTY(BlueprintReadOnly,Category = "PhysicalSimulation")
	UTextureRenderTarget* SimulationTexture;

	UPROPERTY(BlueprintReadOnly,Category = "PhysicalSimulation")
	UTextureRenderTarget* PressureTexture;*/

	FSolverParameter SolverParameter;
	FPhysicalSolverContext PhysicalSolverContext;
	void UpdateSolverContext();
protected:
	
	virtual void BeginPlay() override;
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;


	//virtual FPrimitiveSceneProxy* CreateSceneProxy() override;

	void SetupSolverParameter();


	/*FVector3f LastOnwerPosition;
	FVector3f CenterOnwerPosition;*/
private:


	void CreateSolverTextures();
	void Create3DRenderTarget();
	void Create2DRenderTarget();
	TSharedPtr<FPhysicalSolverViewExtension,ESPMode::ThreadSafe> PhysicalSolverViewExtension;

	TArray<UTextureRenderTarget*> OutputTextures;
	bool bInitialed;

	
};
