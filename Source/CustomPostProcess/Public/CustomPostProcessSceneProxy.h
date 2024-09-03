// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CustomPostProcessComponent.h"
#include "CustomPostProcessViewExtension.h"
#include "RenderAdapter.h"
#include "UObject/Object.h"

/**
 * 
 */

class CUSTOMPOSTPROCESS_API FCPPSceneProxy : public FPrimitiveSceneProxy
{
public:
	FCPPSceneProxy( UCustomPostProcessComponent* InComponent);
	~FCPPSceneProxy();
	TArray<FRenderAdapterBase*> RenderAdapters;
protected:

	virtual SIZE_T GetTypeHash() const override;

	virtual void CreateRenderThreadResources() override;
	virtual void DestroyRenderThreadResources() override;
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
	virtual uint32 GetMemoryFootprint() const override { return(sizeof(*this) + GetAllocatedSize()); }
	//End FPrimitiveSceneProxy Interface

private:
	TSharedPtr<FCPPViewExtension>  ViewExtension;
	UCustomPostProcessComponent* Component = nullptr;
};
