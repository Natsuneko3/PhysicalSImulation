#pragma once


#include "SceneViewExtension.h"
#include "Physical2DFluidSolver.h"
#include "Physical3DFluidSolver.h"
#include "PhysicalLiquidSolver.h"

class UPhysicalSimulationComponent;

class FPhysicalSolverViewExtension : public FSceneViewExtensionBase
{
public:
	FPhysicalSolverViewExtension(const FAutoRegister& AutoRegister, UPhysicalSimulationComponent* InComponent);
	~FPhysicalSolverViewExtension();

public:
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


	//~ End ISceneViewExtension Interface


	void Initial(FPhysicalSolverContext* InContext);
	/*void InitDelegate();
	void ReleaseDelegate();
	void Render_RenderThread(FPostOpaqueRenderParameters& Parameters);*/

	void UpdateParameters(UPhysicalSimulationComponent* InComponent);
	void Release();


private:
	//FPhysicalSolverContext* SolverContext;
	FPhysicalSolverContext* SolverContext;
	FPostOpaqueRenderDelegate RenderDelegate;
	UPhysicalSimulationComponent* Component;
	ESimulatorType LastType;
	//TArray<FPhysicalSolverSceneProxy*> SceneProxies;
	//UPhysicalSimulationComponent* SolverComponent;
	//FPhysicalSolverBase* PhysicalSolver;
	TSharedPtr<FPhysicalSolverBase> PhysicalSolver;
	FDelegateHandle RenderDelegateHandle;
	TArray<UTextureRenderTarget*> OutPutTextures;
};
