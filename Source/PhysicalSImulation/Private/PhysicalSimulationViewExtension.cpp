﻿#include "PhysicalSimulationViewExtension.h"

#include "PhysicalSimulationComponent.h"
#include "PhysicalSimulationSceneProxy.h"
#include "ShaderPrintParameters.h"

FPhysicalSimulationViewExtension::FPhysicalSimulationViewExtension(const FAutoRegister& AutoRegister, UPhysicalSimulationComponent* InComponent):
	FSceneViewExtensionBase(AutoRegister), Component(InComponent)
{
	switch (Component->SimulatorType)
	{
	case ESimulatorType::PlaneSmokeFluid:
		PhysicalSolver = MakeShareable(new FPhysical2DFluidSolver);
		break;
	case ESimulatorType::CubeSmokeFluid:
		PhysicalSolver = MakeShareable(new FPhysical3DFluidSolver);
		break;
	case ESimulatorType::Liquid:
		PhysicalSolver = MakeShareable(new FPhysicalLiquidSolver); //new FPhysicalLiquidSolver;
		break;
	}
	LastType = Component->SimulatorType;
	FeatureLevel = InComponent->GetWorld()->FeatureLevel;


}

FPhysicalSimulationViewExtension::~FPhysicalSimulationViewExtension()
{
	Release();
}

void FPhysicalSimulationViewExtension::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
}

void FPhysicalSimulationViewExtension::PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView)
{

	Component->UpdateSolverContext();
	SolverContext = &Component->PhysicalSolverContext;
	SolverContext->SolverParameter->FluidParameter.SolverBaseParameter.View = InView.ViewUniformBuffer;
	SolverContext->SolverParameter->LiuquidParameter.SolverBaseParameter.View = InView.ViewUniformBuffer;
	SolverContext->FeatureLevel = InView.FeatureLevel;

	PhysicalSolver->SetParameter(SolverContext);
	PhysicalSolver->Update_RenderThread(GraphBuilder, InView);
	if(Component->SimulatorType == ESimulatorType::Liquid)
	{

	}

}

bool FPhysicalSimulationViewExtension::IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const
{
	if(Component)
	{
		return Component->bSimulation;
	}
	return false;

}

void FPhysicalSimulationViewExtension::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector,const FPhysicalSimulationSceneProxy* SceneProxy) const
{
	//PhysicalSolver->GetDynamicMeshElements(Views,ViewFamily,VisibilityMap,Collector,SceneProxy);
}

/*void FPhysicalSimulationViewExtension::CreateMeshBatch(FMeshBatch& MeshBatch, const FPhysicalSimulationSceneProxy* PrimitiveProxy, const FMaterialRenderProxy* MaterialProxy) const
{
	MeshBatch.bUseWireframeSelectionColoring = PrimitiveProxy->IsSelected();
	MeshBatch.VertexFactory = VertexFactory.Get();
	MeshBatch.MaterialRenderProxy = MaterialProxy;
	MeshBatch.ReverseCulling = false;
	MeshBatch.Type = PT_TriangleList;
	MeshBatch.DepthPriorityGroup = SDPG_World;
	MeshBatch.bCanApplyViewModeOverrides = true;
	MeshBatch.bUseForMaterial = true;
	MeshBatch.CastShadow = false;
	MeshBatch.bUseForDepthPass = false;

	const FStaticMeshLODResources& LODModel = PrimitiveProxy->GetStaticMesh()->GetRenderData()->LODResources[0];
	FMeshBatchElement& BatchElement = MeshBatch.Elements[0];
	BatchElement.PrimitiveUniformBuffer = PrimitiveProxy->GetUniformBuffer();
	BatchElement.IndexBuffer = &LODModel.IndexBuffer;
	BatchElement.FirstIndex = 0;
	BatchElement.MinVertexIndex = 0;
	BatchElement.NumInstances = Component->SimulatorType == ESimulatorType::Liquid? 10 : 1;
	BatchElement.MaxVertexIndex = LODModel.GetNumVertices() - 1;
	BatchElement.NumPrimitives = LODModel.GetNumTriangles();
	BatchElement.VertexFactoryUserData = VertexFactory->GetUniformBuffer();

}*/

void FPhysicalSimulationViewExtension::AddProxy(FPhysicalSimulationSceneProxy* Proxy)
{
	/*ENQUEUE_RENDER_COMMAND(FAddPhysicalSimulationProxyCommand)(
		[this, Proxy](FRHICommandListImmediate& RHICmdList)
		{
			check(SceneProxies.Find(Proxy) == INDEX_NONE);
			SceneProxies.Emplace(Proxy);
		});*/
	check(SceneProxies.Find(Proxy) == INDEX_NONE);
	SceneProxies.Emplace(Proxy);
}

void FPhysicalSimulationViewExtension::RemoveProxy(FPhysicalSimulationSceneProxy* Proxy)
{
	ENQUEUE_RENDER_COMMAND(FRemovePhysicalSimulationProxyCommand)(
		[this, Proxy](FRHICommandListImmediate& RHICmdList)
		{
			auto Idx = SceneProxies.Find(Proxy);
			if (Idx != INDEX_NONE)
			{
				SceneProxies.Remove(Proxy);
			}
		});
}

void FPhysicalSimulationViewExtension::PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily)
{
	FSceneViewExtensionBase::PreRenderViewFamily_RenderThread(GraphBuilder, InViewFamily);
}


void FPhysicalSimulationViewExtension::Initial(FRHICommandListBase& RHICmdList)
{
	PhysicalSolver->Initial(RHICmdList);
}


void FPhysicalSimulationViewExtension::UpdateParameters(UPhysicalSimulationComponent* InComponent)
{
	Component = InComponent;
	if(LastType != Component->SimulatorType)
	{
		PhysicalSolver->Release();
		switch (Component->SimulatorType)
		{
		case ESimulatorType::PlaneSmokeFluid:
			PhysicalSolver = MakeShareable(new FPhysical2DFluidSolver);
			break;
		case ESimulatorType::CubeSmokeFluid:
			PhysicalSolver = MakeShareable(new FPhysical3DFluidSolver);
			break;
		case ESimulatorType::Liquid:
			PhysicalSolver = MakeShareable(new FPhysicalLiquidSolver); //new FPhysicalLiquidSolver;
			break;
		}
		LastType = Component->SimulatorType;
	}
}

void FPhysicalSimulationViewExtension::Release()
{
	PhysicalSolver->Release();
}