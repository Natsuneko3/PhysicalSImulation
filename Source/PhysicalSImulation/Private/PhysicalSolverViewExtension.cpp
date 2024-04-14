#include "PhysicalSolverViewExtension.h"

#include "PhysicalSimulationComponent.h"
#include "ShaderPrintParameters.h"

FPhysicalSolverViewExtension::FPhysicalSolverViewExtension(const FAutoRegister& AutoRegister, UPhysicalSimulationComponent* InComponent):
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
}

FPhysicalSolverViewExtension::~FPhysicalSolverViewExtension()
{
	Release();
}

void FPhysicalSolverViewExtension::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
}

void FPhysicalSolverViewExtension::PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView)
{

	Component->UpdateSolverContext();
	SolverContext = &Component->PhysicalSolverContext;
	SolverContext->SolverParameter->FluidParameter.SolverBaseParameter.View = InView.ViewUniformBuffer;
	SolverContext->SolverParameter->LiuquidParameter.SolverBaseParameter.View = InView.ViewUniformBuffer;
	SolverContext->FeatureLevel = InView.FeatureLevel;

	PhysicalSolver->SetParameter(SolverContext->SolverParameter);
	PhysicalSolver->Update_RenderThread(GraphBuilder, SolverContext, InView);

}

bool FPhysicalSolverViewExtension::IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const
{
	if(Component)
	{
		return Component->bSimulation;
	}
	return false;

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


void FPhysicalSolverViewExtension::UpdateParameters(UPhysicalSimulationComponent* InComponent)
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

void FPhysicalSolverViewExtension::Release()
{
	PhysicalSolver->Release();
}
