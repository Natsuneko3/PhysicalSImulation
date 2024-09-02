#pragma once
#include "SceneViewExtension.h"

class FCPPSceneProxy;

class FCPPViewExtension : public FSceneViewExtensionBase
{
public:
	FCPPViewExtension(const FAutoRegister& AutoRegister);
	~FCPPViewExtension();

	//~ Begin ISceneViewExtension Interface
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override
	{
	}

	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override
	{
	}

	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual void PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override;
	virtual int32 GetPriority() const override { return -1; }
	virtual void PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView) override;
	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const;
	virtual void PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs) override;
	//~ End ISceneViewExtension Interface

	void AddProxy(FCPPSceneProxy* Proxy,FRHICommandListBase& RHICmdList);
	void RemoveProxy(FCPPSceneProxy* Proxy);
	void Render_RenderThread(FPostOpaqueRenderParameters& Parameters);
	void Initial(FRHICommandListBase& RHICmdList);

private:

	FPostOpaqueRenderDelegate RenderDelegate;
	TArray<FCPPSceneProxy*> SceneProxies;
	FDelegateHandle RenderDelegateHandle;
};
