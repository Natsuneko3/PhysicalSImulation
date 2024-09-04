#include "CustomPostProcessViewExtension.h"

#include "CustomPostProcessSceneProxy.h"
#include "RenderAdapter.h"


FCPPViewExtension::FCPPViewExtension(const FAutoRegister& AutoRegister):
	FSceneViewExtensionBase(AutoRegister)
{
}

FCPPViewExtension::~FCPPViewExtension()
{
	RenderDelegateHandle.Reset();
	SceneProxies.Empty();
}

void FCPPViewExtension::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
}

void FCPPViewExtension::PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView)
{
	for (FCPPSceneProxy* SceneProxy : SceneProxies)
	{
		//SceneProxy->PhysicalSolver->SetParameter(SceneProxy);
		if (SceneProxy && SceneProxy->RenderAdapters->Num() > 0)
		{
			for(URenderAdapterBase* RenderAdapter: SceneProxy->RenderAdapters)
			{
				if(RenderAdapter)
				{
					RenderAdapter->PreRenderView_RenderThread(GraphBuilder, InView);
				}
			}
		}
	}
}

bool FCPPViewExtension::IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const
{
	for (FCPPSceneProxy* SceneProxy : SceneProxies)
	{
		if (SceneProxy )
		{
			return true;
		}
	}

	return false;
}

void FCPPViewExtension::PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs)
{

	for (FCPPSceneProxy* SceneProxy : SceneProxies)
	{
		//SceneProxy->PhysicalSolver->SetParameter(SceneProxy);
		if (SceneProxy && SceneProxy->RenderAdapters->Num() > 0)
		{
			for(URenderAdapterBase* RenderAdapter: SceneProxy->RenderAdapters)
			{
				if(RenderAdapter)
				{
					RenderAdapter->PrePostProcessPass_RenderThread(GraphBuilder, View,Inputs);
				}
			}
		}
	}
}

void FCPPViewExtension::AddProxy(FCPPSceneProxy* Proxy)
{
	if (SceneProxies.Find(Proxy) == INDEX_NONE)
	{
		SceneProxies.Add(Proxy);
	}
	if (!RenderDelegateHandle.IsValid())
	{
		const FName RendererModuleName("Renderer");
		IRendererModule* RendererModule = FModuleManager::GetModulePtr<IRendererModule>(RendererModuleName);
		if (RendererModule)
		{
			RenderDelegate.BindRaw(this, &FCPPViewExtension::Render_RenderThread);
			RenderDelegateHandle = RendererModule->RegisterPostOpaqueRenderDelegate(RenderDelegate);
		}
	}
}

void FCPPViewExtension::RemoveProxy(FCPPSceneProxy* Proxy)
{
	auto Idx = SceneProxies.Find(Proxy);
	if (Idx != INDEX_NONE)
	{
		SceneProxies.Remove(Proxy);
		if(SceneProxies.IsEmpty())
		{
			RenderDelegate.Unbind();
			RenderDelegateHandle.Reset();
		}
	}
}

void FCPPViewExtension::Render_RenderThread(FPostOpaqueRenderParameters& Parameters)
{

}

void FCPPViewExtension::Initial(FRHICommandListBase& RHICmdList)
{
}

void FCPPViewExtension::PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily)
{
	FSceneViewExtensionBase::PreRenderViewFamily_RenderThread(GraphBuilder, InViewFamily);
}
