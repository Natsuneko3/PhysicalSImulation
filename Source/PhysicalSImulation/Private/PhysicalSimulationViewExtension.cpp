#include "PhysicalSimulationViewExtension.h"

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
	PhysicalSolver->Initial(&Component->PhysicalSolverContext);
	VertexFactory = MakeUnique<FInstancedStaticMeshVertexFactory>(InComponent->GetWorld()->FeatureLevel);
	VertexFactory->InitResource();
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

	PhysicalSolver->SetParameter(SolverContext->SolverParameter);
	PhysicalSolver->Update_RenderThread(GraphBuilder, SolverContext, InView);
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

void FPhysicalSimulationViewExtension::CreateMeshBatch(FMeshBatch& MeshBatch, const FPhysicalSimulationSceneProxy* PrimitiveProxy, const FMaterialRenderProxy* MaterialProxy) const
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

}

void FPhysicalSimulationViewExtension::AddProxy(FPhysicalSimulationSceneProxy* Proxy)
{
	ENQUEUE_RENDER_COMMAND(FAddPhysicalSimulationProxyCommand)(
		[this, Proxy](FRHICommandListImmediate& RHICmdList)
		{
			check(SceneProxies.Find(Proxy) == INDEX_NONE);
			SceneProxies.Emplace(Proxy);
		});
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


void FPhysicalSimulationViewExtension::Initial(FPhysicalSolverContext* InContext)
{
	/*if(IsInRenderingThread())
	{
		//SolverContext = InContext;
		switch (SolverContext->SimulatorType)
		{
		case ESimulatorType::PlaneSmokeFluid:
			PhysicalSolver = MakeShareable(new FPhysical2DFluidSolver).Object;
			break;
		case ESimulatorType::CubeSmokeFluid:
			break;
		case ESimulatorType::Water:
			break;
		}
		PhysicalSolver->Initial(SolverContext);
		bInitialed = true;
		//InitDelegate();
	}
	else
	{
		ENQUEUE_RENDER_COMMAND(ActorPhysicalSimulation)([this,InContext](FRHICommandListImmediate& RHICmdList)
		{

			Initial(InContext);
		});
	}*/
	/*switch (SolverContext->SimulatorType)
	{
	case ESimulatorType::PlaneSmokeFluid:
		PhysicalSolver = MakeShareable(new FPhysical2DFluidSolver).Object;
		break;
	case ESimulatorType::CubeSmokeFluid:
		break;
	case ESimulatorType::Water:
		break;
	}
	PhysicalSolver->Initial(SolverContext);*/

	//SolverContext = InContext;
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
