// Fill out your copyright notice in the Description page of Project Settings.


#include "PhysicalSimulationSceneProxy.h"

#include "PhysicalSimulationComponent.h"
#include "PhysicalSimulationSystem.h"

DECLARE_CYCLE_STAT(TEXT("GetDynamicMeshElements"),STAT_PS_GetDynamicMeshElements,STATGROUP_PS)

FPhysicalSimulationSceneProxy::FPhysicalSimulationSceneProxy(UPhysicalSimulationComponent* InComponent)
:FPrimitiveSceneProxy(InComponent),Component(InComponent)
{
	UPhysicalSimulationSystem* SubSystem = InComponent->GetWorld()->GetSubsystem<UPhysicalSimulationSystem>();
	if(SubSystem)
	{
		ViewExtension = SubSystem->FindOrCreateViewExtension(InComponent);
	}

	StaticMesh = InComponent->GetStaticMesh();
	Material = InComponent->Material.Get();
}

SIZE_T FPhysicalSimulationSceneProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

void FPhysicalSimulationSceneProxy::CreateRenderThreadResources(FRHICommandListBase& RHICmdList)
{
	FPrimitiveSceneProxy::CreateRenderThreadResources( RHICmdList);
	if(ViewExtension)
	{
		ViewExtension->AddProxy(this);
		ViewExtension->Initial(RHICmdList);
	}


}

void FPhysicalSimulationSceneProxy::DestroyRenderThreadResources()
{
	FPrimitiveSceneProxy::DestroyRenderThreadResources();
	if(ViewExtension)
	{
		ViewExtension->RemoveProxy(this);
	}

}
// This setups associated volume mesh for built-in Unreal passes.
// Actual rendering is FPhysicalSimulationViewExtension::PreRenderView_RenderThread.
void FPhysicalSimulationSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	/*SCOPE_CYCLE_COUNTER(STAT_PS_GetDynamicMeshElements);
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const FSceneView* View = Views[ViewIndex];

		if (IsShown(View) && VisibilityMap & 1 << ViewIndex)
		{
			FMeshBatch& Mesh = Collector.AllocateMesh();
			Mesh.bWireframe = AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;

			ViewExtension->CreateMeshBatch(Mesh, this,Material->GetRenderProxy());

			Collector.AddMesh(ViewIndex, Mesh);
		}
	}*/
	ViewExtension->GetDynamicMeshElements(Views,ViewFamily,VisibilityMap,Collector,this);
}

FPrimitiveViewRelevance FPhysicalSimulationSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = IsShown(View);
	Result.bShadowRelevance = IsShadowCast(View) && ShouldRenderInMainPass();
	Result.bDynamicRelevance = true;
	Result.bStaticRelevance = false;
	Result.bRenderInMainPass = ShouldRenderInMainPass();
	Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
	Result.bRenderCustomDepth = ShouldRenderCustomDepth();
	return Result;
}
