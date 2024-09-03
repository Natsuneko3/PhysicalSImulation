// Fill out your copyright notice in the Description page of Project Settings.
#include "CustomPostProcessSceneProxy.h"
#include "CustomPostProcessViewExtension.h"
#include "CustomPostProcessVolume.h"
#include "CustomPostProcessWorldSystem.h"
#include "TranslucentPP/TranslucentPostprocess.h"
DECLARE_STATS_GROUP(TEXT("Custom PostProcess"), STATGROUP_CPP, STATCAT_Advanced)

FCPPSceneProxy::FCPPSceneProxy(UCustomPostProcessComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent), Component(InComponent)
{
	UCPPWorldSystem* SubSystem = InComponent->GetWorld()->GetSubsystem<UCPPWorldSystem>();
	/*if (SubSystem)
	{
		ViewExtension = SubSystem->CustomPostProcessViewExtension;
		for(FCustomPostProcessSet* CustomPostProcess : &Component->GetCPPVolume()->CustomPostProcessSet)
		{
			switch (CustomPostProcess->Feature)
			{
			case 0:
				RenderAdapters.Add(new FTranslucentPostProcess(this,&CustomPostProcess->PostProcessSetMaterial));
				break;
			case 1:
			case 2:
			case 3:
			case 4:
				break;
			}
		}
	}*/
	ENQUEUE_RENDER_COMMAND(InitPhysicalSolver)(
		[this](FRHICommandListImmediate& RHICmdList)
		{
			for(FRenderAdapterBase* RednerAdapter:RenderAdapters)
			{
				RednerAdapter->Initial_RenderThread(RHICmdList);
			}
		});
}

FCPPSceneProxy::~FCPPSceneProxy()
{
	ViewExtension.Reset();
	RenderAdapters.Empty();
}


SIZE_T FCPPSceneProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

void FCPPSceneProxy::CreateRenderThreadResources()
{
	FPrimitiveSceneProxy::CreateRenderThreadResources();
	if (ViewExtension)
	{
		ViewExtension->AddProxy(this);
	}
}

void FCPPSceneProxy::DestroyRenderThreadResources()
{
	if (ViewExtension)
		ViewExtension->RemoveProxy(this);
}

// These setups associated volume mesh for built-in Unreal passes.
// Actual rendering is FPhysicalSimulationViewExtension::PreRenderView_RenderThread.
void FCPPSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{


}

FPrimitiveViewRelevance FCPPSceneProxy::GetViewRelevance(const FSceneView* View) const
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
