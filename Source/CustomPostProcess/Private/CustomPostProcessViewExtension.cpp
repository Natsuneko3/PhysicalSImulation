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
	DECLARE_GPU_STAT(CustomRenderFeature)
	RDG_EVENT_SCOPE(GraphBuilder, "CustomRenderFeature");
	RDG_GPU_STAT_SCOPE(GraphBuilder, CustomRenderFeature);
	for (FCPPSceneProxy* SceneProxy : SceneProxies)
	{
		//SceneProxy->PhysicalSolver->SetParameter(SceneProxy);
		if (SceneProxy && SceneProxy->Component !=nullptr && SceneProxy->Component->RenderFeatures.Num())
		{
			for(URenderAdapterBase* RenderAdapter : SceneProxy->Component->RenderFeatures)
			{
				if(RenderAdapter&&RenderAdapter->bEnable&&RenderAdapter->TextureBlendDesc.Weight > 0.f)
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
	DECLARE_GPU_STAT(CustomRenderFeature)
	RDG_EVENT_SCOPE(GraphBuilder, "CustomRenderFeature");
	RDG_GPU_STAT_SCOPE(GraphBuilder, CustomRenderFeature);
	for (FCPPSceneProxy* SceneProxy : SceneProxies)
	{
		//SceneProxy->PhysicalSolver->SetParameter(SceneProxy);
		if (SceneProxy && SceneProxy->Component != nullptr && SceneProxy->Component->RenderFeatures.Num())
		{
			for(URenderAdapterBase* RenderAdapter : SceneProxy->Component->RenderFeatures)
			{
				if(RenderAdapter&&RenderAdapter->bEnable&&RenderAdapter->TextureBlendDesc.Weight > 0.f)
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
