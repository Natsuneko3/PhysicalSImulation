#include "PhysicalSolverViewExtension.h"

#include "ShaderPrintParameters.h"

FPhysicalSolverViewExtension::FPhysicalSolverViewExtension(const FAutoRegister& AutoRegister,FPhysicalSolverContext* InContext):
	FSceneViewExtensionBase(AutoRegister),SolverContext(InContext)
{
	switch (SolverContext->SimulatorType)
	{
	case ESimulatorType::PlaneSmokeFluid:
		PhysicalSolver = MakeShareable(new FPhysical2DFluidSolver);
		break;
	case ESimulatorType::CubeSmokeFluid:
		PhysicalSolver = MakeShareable(new FPhysical3DFluidSolver);
		break;
	case ESimulatorType::Liquid:
		PhysicalSolver = MakeShareable(new FPhysicalLiquidSolver);//new FPhysicalLiquidSolver;
		break;
	}

	PhysicalSolver->Initial(SolverContext);
}

FPhysicalSolverViewExtension::~FPhysicalSolverViewExtension()
{
	PhysicalSolver.Reset();
}

void FPhysicalSolverViewExtension::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
}

void FPhysicalSolverViewExtension::PreRenderView_RenderThread(FRDGBuilder& GraphBuilder,FSceneView& InView)
{
	if (SolverContext != nullptr)
	{
		if(SolverContext->bSimulation)
		{

			SolverContext->SolverParameter->FluidParameter.SolverBaseParameter.View = InView.ViewUniformBuffer;
			SolverContext->FeatureLevel = InView.FeatureLevel;
			PhysicalSolver->SetParameter(SolverContext->SolverParameter);
			PhysicalSolver->Update_RenderThread(GraphBuilder, SolverContext,InView);

		}
	}
}

void FPhysicalSolverViewExtension::PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily)
{
	FSceneViewExtensionBase::PreRenderViewFamily_RenderThread(GraphBuilder, InViewFamily);

}


void FPhysicalSolverViewExtension::Initial(FPhysicalSolverContext* InContext)
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


/*
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
	/*if(SolverComponent != nullptr)
	{
		SolverContext = SolverComponent->PhysicalSolverContext;
		FRDGBuilder& GraphBuilder = *Parameters.GraphBuilder;
		const FSceneView* View = static_cast<FSceneView*>(Parameters.Uid);

		SolverContext->SolverParameter->FluidParameter.SolverBaseParameter.View = View->ViewUniformBuffer;
		SolverContext->FeatureLevel = View->FeatureLevel;

		PhysicalSolver->SetParameter(SolverContext->SolverParameter);
		PhysicalSolver->Update_RenderThread(GraphBuilder, SolverContext);
	}#1#
	/*if (SolverContext != nullptr)
	{
		if(SolverContext->bSimulation)
		{
			FRDGBuilder& GraphBuilder = *Parameters.GraphBuilder;
			const FSceneView* View = static_cast<FSceneView*>(Parameters.Uid);

			SolverContext->SolverParameter->FluidParameter.SolverBaseParameter.View = View->ViewUniformBuffer;
			SolverContext->FeatureLevel = View->FeatureLevel;

			PhysicalSolver->SetParameter(SolverContext->SolverParameter);
			PhysicalSolver->Update_RenderThread(GraphBuilder, SolverContext);
		}
	}#1#
}
*/

void FPhysicalSolverViewExtension::UpdateParameters(FPhysicalSolverContext* Context)
{
	SolverContext = Context;
}
