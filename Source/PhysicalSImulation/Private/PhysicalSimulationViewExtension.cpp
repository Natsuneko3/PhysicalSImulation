#include "PhysicalSimulationViewExtension.h"

#include "PhysicalSimulationComponent.h"
#include "PhysicalSimulationSceneProxy.h"
#include "ShaderPrintParameters.h"

FPhysicalSimulationViewExtension::FPhysicalSimulationViewExtension(const FAutoRegister& AutoRegister):
	FSceneViewExtensionBase(AutoRegister)
{

	if (!RenderDelegateHandle.IsValid())
	{
		const FName RendererModuleName("Renderer");
		IRendererModule* RendererModule = FModuleManager::GetModulePtr<IRendererModule>(RendererModuleName);
		if (RendererModule)
		{
			RenderDelegate.BindRaw(this, &FPhysicalSimulationViewExtension::Render_RenderThread);
			RenderDelegateHandle = RendererModule->RegisterPostOpaqueRenderDelegate(RenderDelegate);
		}
	}
}

FPhysicalSimulationViewExtension::~FPhysicalSimulationViewExtension()
{
	RenderDelegateHandle.Reset();
}

void FPhysicalSimulationViewExtension::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
}

void FPhysicalSimulationViewExtension::PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView)
{
	for (FPhysicalSimulationSceneProxy* SceneProxy : SceneProxies)
	{
		//SceneProxy->PhysicalSolver->SetParameter(SceneProxy);
		if (SceneProxy)
		{
			SceneProxy->PhysicalSolver->PreRenderView_RenderThread(GraphBuilder, InView);
		}
	}
}

bool FPhysicalSimulationViewExtension::IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const
{
	for (FPhysicalSimulationSceneProxy* SceneProxy : SceneProxies)
	{
		if (SceneProxy && SceneProxy->bSimulation)
		{
			return true;
		}
	}

	return false;
}

void FPhysicalSimulationViewExtension::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector, const FPhysicalSimulationSceneProxy* SceneProxy) const
{
	//PhysicalSolver->GetDynamicMeshElements(Views,ViewFamily,VisibilityMap,Collector,SceneProxy);
}

void FPhysicalSimulationViewExtension::AddProxy(FPhysicalSimulationSceneProxy* Proxy, FRHICommandListBase& RHICmdList)
{
	if(SceneProxies.Find(Proxy) == INDEX_NONE)
	{
		SceneProxies.Add(Proxy);
	}
}

void FPhysicalSimulationViewExtension::RemoveProxy(FPhysicalSimulationSceneProxy* Proxy)
{
	auto Idx = SceneProxies.Find(Proxy);
	if(Idx != INDEX_NONE)
		SceneProxies.Remove(Proxy);
}

void FPhysicalSimulationViewExtension::Render_RenderThread(FPostOpaqueRenderParameters& Parameters)
{

	if (!SceneProxies.IsEmpty() && SceneProxies.Num() < 128)
	{
		for(int i = 0;i<SceneProxies.Num();i++)
		{
			if(SceneProxies.IsValidIndex(i))
			{
				FPhysicalSimulationSceneProxy* SceneProxy = SceneProxies[i];
				if(SceneProxy && SceneProxy->bSimulation)
				{
					SceneProxy->PhysicalSolver->Render_RenderThread(Parameters);
				}
			}
		}

	}
}

void FPhysicalSimulationViewExtension::Initial(FRHICommandListBase& RHICmdList)
{
}

void FPhysicalSimulationViewExtension::PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily)
{
	FSceneViewExtensionBase::PreRenderViewFamily_RenderThread(GraphBuilder, InViewFamily);
}
