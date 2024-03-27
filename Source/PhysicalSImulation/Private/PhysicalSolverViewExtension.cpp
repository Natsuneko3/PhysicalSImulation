#include "PhysicalSolverViewExtension.h"

#include "CommonRenderResources.h"
#include "Physical2DFluidSolver.h"
#include "RenderGraphBuilder.h"
#include "Renderer/Private/SceneRendering.h"

FPhysicalSolverViewExtension::FPhysicalSolverViewExtension(const FAutoRegister& AutoRegister, FPhysicalSolverContext* InContext):
	FSceneViewExtensionBase(AutoRegister),
	SolverContext(InContext)
{
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
}

void FPhysicalSolverViewExtension::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
}

void FPhysicalSolverViewExtension::PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily)
{
	FSceneViewExtensionBase::PreRenderViewFamily_RenderThread(GraphBuilder, InViewFamily);
}

void FPhysicalSolverViewExtension::AddSceneProxy(FPhysicalSolverSceneProxy* Proxy)
{
	ENQUEUE_RENDER_COMMAND(FAddVdbProxyCommand)(
		[this, Proxy](FRHICommandListImmediate& RHICmdList)
		{
			check(SceneProxies.Find(Proxy) == INDEX_NONE);
			SceneProxies.Emplace(Proxy);
		});

}

void FPhysicalSolverViewExtension::RemoveSceneProxy(FPhysicalSolverSceneProxy* Proxy)
{
	ENQUEUE_RENDER_COMMAND(FRemoveVdbProxyCommand)(
		[this, Proxy](FRHICommandListImmediate& RHICmdList)
		{
			auto Idx = SceneProxies.Find(Proxy);
			if (Idx != INDEX_NONE)
			{
				SceneProxies.Remove(Proxy);
			}
		});
}

void FPhysicalSolverViewExtension::Initial()
{
	InitDelegate();
	PhysicalSolver->Initial();
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
	FRDGBuilder& GraphBuilder = *Parameters.GraphBuilder;
	const FSceneView* View = static_cast<FSceneView*>(Parameters.Uid);
	SolverContext->SolverParameter.FluidParameter.SolverBaseParameter.View = View->ViewUniformBuffer;
	PhysicalSolver->SetParameter(SolverContext->SolverParameter);
	PhysicalSolver->Update_RenderThread(GraphBuilder,SolverContext);
}

TArray<UTextureRenderTarget*> FPhysicalSolverViewExtension::GetOutPutTextures()
{
	return PhysicalSolver->GetOutputTextures();
}

