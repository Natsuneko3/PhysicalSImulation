#pragma once
#include "Physical2DFluidSolver.h"
#include "PhysicalSolver.h"
#include "SceneViewExtension.h"

class FPhysicalSolverViewExtension : public FSceneViewExtensionBase
{
public:
	FPhysicalSolverViewExtension(const FAutoRegister& AutoRegister,FPhysicalSolverContext* InContext);

public:
	//~ Begin ISceneViewExtension Interface
    virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {}
    virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {}
    virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;
    virtual void PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView) override {}
    virtual void PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override;
    virtual int32 GetPriority() const override { return -1; }
    virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const { return true; }
    //~ End ISceneViewExtension Interface



	void Initial();
	void InitDelegate();
	void ReleaseDelegate();
	void Render_RenderThread(FPostOpaqueRenderParameters& Parameters);

	FPhysicalSolverBase* PhysicalSolver;

private:
	FPostOpaqueRenderDelegate RenderDelegate;
	//TArray<FPhysicalSolverSceneProxy*> SceneProxies;

	FPhysicalSolverContext* SolverContext;
	FDelegateHandle RenderDelegateHandle;
	TArray<UTextureRenderTarget*> OutPutTextures;

};
