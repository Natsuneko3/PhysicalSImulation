// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CustomRenderFeatureComponent.h"
#include "PrimitiveSceneProxy.h"
#include "CustomRenderFeatureViewExtension.h"
#include "UObject/Object.h"

/**
 * 
 */

class CUSTOMRENDERFEATURE_API FCPPSceneProxy : public FPrimitiveSceneProxy
{
public:
	FCPPSceneProxy( UCustomRenderFeatureComponent* InComponent);
	~FCPPSceneProxy();
	//TArray<TObjectPtr<URenderAdapterBase>> RenderAdapters;
	UCustomRenderFeatureComponent* Component = nullptr;
protected:

	virtual SIZE_T GetTypeHash() const override;
	virtual void CreateRenderThreadResources(FRHICommandListBase& RHICmdList) override;


	virtual void DestroyRenderThreadResources() override;
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
	virtual uint32 GetMemoryFootprint() const override { return(sizeof(*this) + GetAllocatedSize()); }
	//End FPrimitiveSceneProxy Interface

private:
	TSharedPtr<FCPPViewExtension>  ViewExtension;

};
