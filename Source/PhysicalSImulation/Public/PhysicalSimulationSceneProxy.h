// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"


/**
 * 
 */

class UPhysicalSimulationComponent;

class PHYSICALSIMULATION_API FPhysicalSimulationSceneProxy : public FPrimitiveSceneProxy
{
public:
	FPhysicalSimulationSceneProxy( UPhysicalSimulationComponent* InComponent);
	 UStaticMesh* GetStaticMesh() const {return StaticMesh;}
	 UMaterialInterface* GetMeterial() const{return  Material;}

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
	TSharedPtr<class FPhysicalSimulationViewExtension>  ViewExtension;
	const UPhysicalSimulationComponent* Component = nullptr;
	UStaticMesh* StaticMesh;
	UMaterialInterface* Material;

};
