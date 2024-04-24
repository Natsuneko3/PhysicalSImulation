#pragma once


#include "SceneViewExtension.h"
#include "Physical2DFluidSolver.h"
#include "Physical3DFluidSolver.h"
#include "PhysicalLiquidSolver.h"
#include "Engine/InstancedStaticMesh.h"

class FPhysicalSimulationSceneProxy;
class UPhysicalSimulationComponent;

class FPhysicalSimulationViewExtension : public FSceneViewExtensionBase
{
public:
	FPhysicalSimulationViewExtension(const FAutoRegister& AutoRegister);
	~FPhysicalSimulationViewExtension();


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


	void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector,const FPhysicalSimulationSceneProxy* SceneProxy) const;
	void AddProxy(FPhysicalSimulationSceneProxy* Proxy);
	void RemoveProxy(FPhysicalSimulationSceneProxy* Proxy);

	void Initial(FRHICommandListBase& RHICmdList);
	/*void InitDelegate();
	void ReleaseDelegate();
	void Render_RenderThread(FPostOpaqueRenderParameters& Parameters);*/

	void UpdateParameters(UPhysicalSimulationComponent* InComponent);
	void Release();


private:
	//FPhysicalSolverContext* SolverContext;
	TEnumAsByte<ERHIFeatureLevel::Type> FeatureLevel;
	FPhysicalSolverContext* SolverContext;
	FPostOpaqueRenderDelegate RenderDelegate;
	//UPhysicalSimulationComponent* Component;
	ESimulatorType LastType;
	TArray<FPhysicalSimulationSceneProxy*> SceneProxies;

	TSharedPtr<FPhysicalSolverBase> PhysicalSolver;
	FDelegateHandle RenderDelegateHandle;
	TArray<UTextureRenderTarget*> OutPutTextures;

	TUniquePtr<FInstancedStaticMeshVertexFactory> VertexFactory;
};
