// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Physical2DFluidSolver.h"
#include "PhysicalLiquidSolver.h"
#include "PhysicalSolver.h"
#include "UObject/Object.h"


/**
 * 
 */

class UPhysicalSimulationComponent;

class PHYSICALSIMULATION_API FPhysicalSimulationSceneProxy : public FPrimitiveSceneProxy
{
public:
	FPhysicalSimulationSceneProxy( UPhysicalSimulationComponent* InComponent);
	~FPhysicalSimulationSceneProxy();
	 UStaticMesh* GetStaticMesh() const {return StaticMesh;}
	 UMaterialInterface* GetMeterial() const{return  Material;}
	bool bSimulation = false;
	TSharedPtr<FPhysicalSolverBase> PhysicalSolver;
	FIntVector GridSize;
	TArray<UTextureRenderTarget*> OutputTextures;
	ERHIFeatureLevel::Type FeatureLevel;
	const FPlandFluidParameters* PlandFluidParameters;
	const FLiquidSolverParameter* LiquidSolverParameter;
	UWorld* World;
	float Dx;
	//const UPhysicalSimulationComponent* GetComponent() {return Component;}
protected:
	//FPrimitiveSceneProxy Interface
	virtual SIZE_T GetTypeHash() const override;

	virtual void CreateRenderThreadResources(FRHICommandListBase& RHICmdList) override;
	virtual void DestroyRenderThreadResources() override;
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
	virtual uint32 GetMemoryFootprint() const override { return(sizeof(*this) + GetAllocatedSize()); }
	//End FPrimitiveSceneProxy Interface

private:
	void Create3DRenderTarget();
	void Create2DRenderTarget();
	TSharedPtr<class FPhysicalSimulationViewExtension>  ViewExtension;
	const UPhysicalSimulationComponent* Component = nullptr;
	UStaticMesh* StaticMesh;
	UMaterialInterface* Material;

};
