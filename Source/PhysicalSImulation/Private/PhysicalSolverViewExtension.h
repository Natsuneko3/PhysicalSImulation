#pragma once
#include "PhysicalSolver.h"
#include "SceneViewExtension.h"

class FPhysicalSolverViewExtension : public FSceneViewExtensionBase
{
public:
	FPhysicalSolverViewExtension(const FAutoRegister& AutoRegister,FPhysicalSolverBase* InPhysicalSolver,FPhysicalSolverContext InContext) :
	FSceneViewExtensionBase(AutoRegister),
	PhysicalSolver(InPhysicalSolver),
	SolverContext(InContext)
	{}

public:
	//~ ISceneViewExtension interface
	// FSceneViewExtensionBase implementation :
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override{};
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override{};
	virtual void PostRenderBasePassDeferred_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView, const FRenderTargetBindingSlots& RenderTargets, TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTextures) override;
private:
	FPhysicalSolverBase* PhysicalSolver;
	FPhysicalSolverContext SolverContext;

	TRefCountPtr<IPooledRenderTarget> TextureRT1;
	TRefCountPtr<IPooledRenderTarget> TextureRT2;
	
};
