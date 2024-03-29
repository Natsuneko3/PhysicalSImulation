#include "PhysicalSolverViewExtension.h"

#include "CommonRenderResources.h"
#include "Physical2DFluidSolver.h"
#include "RenderGraphBuilder.h"

FPhysicalSolverViewExtension::FPhysicalSolverViewExtension(const FAutoRegister& AutoRegister):
	FSceneViewExtensionBase(AutoRegister)
{

}

void FPhysicalSolverViewExtension::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
}

void FPhysicalSolverViewExtension::PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily)
{
	FSceneViewExtensionBase::PreRenderViewFamily_RenderThread(GraphBuilder, InViewFamily);
}


void FPhysicalSolverViewExtension::Initial(FPhysicalSolverContext* InContext)
{
	SolverContext = InContext;
	switch (InContext->SimulatorType)
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
	InitDelegate();

}

void FPhysicalSolverViewExtension::InitDelegate()
{
	if (!RenderDelegateHandle.IsValid())
	{
		const FName RendererModuleName("Renderer");
		IRendererModule* RendererModule = FModuleManager::GetModulePtr<IRendererModule>(RendererModuleName);
		if (RendererModule)
		{
			RenderDelegate.BindRaw(this, &FPhysicalSolverViewExtension::Render_RenderThread);
			RenderDelegateHandle = RendererModule->RegisterPostOpaqueRenderDelegate(RenderDelegate);
		}
	}
}

void FPhysicalSolverViewExtension::ReleaseDelegate()
{
	if (RenderDelegateHandle.IsValid())
	{
		const FName RendererModuleName("Renderer");
		IRendererModule* RendererModule = FModuleManager::GetModulePtr<IRendererModule>(RendererModuleName);
		if (RendererModule)
		{
			RendererModule->RemovePostOpaqueRenderDelegate(RenderDelegateHandle);
		}

		RenderDelegateHandle.Reset();
	}
}

void FPhysicalSolverViewExtension::Render_RenderThread(FPostOpaqueRenderParameters& Parameters)
{
	if (SolverContext != nullptr && SolverContext->bSimulation)
	{
		FRDGBuilder& GraphBuilder = *Parameters.GraphBuilder;
		const FSceneView* View = static_cast<FSceneView*>(Parameters.Uid);

		SolverContext->SolverParameter->FluidParameter.SolverBaseParameter.View = View->ViewUniformBuffer;
		SolverContext->FeatureLevel = View->FeatureLevel;

		PhysicalSolver->SetParameter(SolverContext->SolverParameter);
		PhysicalSolver->Update_RenderThread(GraphBuilder, SolverContext);
	}
}
