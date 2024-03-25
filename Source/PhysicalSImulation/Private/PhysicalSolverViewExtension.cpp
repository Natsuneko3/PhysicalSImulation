#include "PhysicalSolverViewExtension.h"

#include "RenderGraphBuilder.h"
#include "Renderer/Private/SceneRendering.h"

void FPhysicalSolverViewExtension::PostRenderBasePassDeferred_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView, const FRenderTargetBindingSlots& RenderTargets,
                                                                           TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTextures)
{
	GraphBuilder.AddPass(RDG_EVENT_NAME("Niagara::ExecuteTicksPost"),
		ERDGPassFlags::None,
		[this,GraphBuilder,InView,SceneTextures](FRHICommandListImmediate& RHICmdList)
		{
			TArray<FRDGTextureRef> OutputArray;
			/*TRefCountPtr<IPooledRenderTarget> PreViewTextureRT = CreateRenderTarget(SimulationTexture->GetResource()->GetTextureRHI(), TEXT("PreViewTexture"));
			FRDGTextureRef OutRT = GraphBuilder.RegisterExternalTexture(PreViewTextureRT);

			TRefCountPtr<IPooledRenderTarget> PressureTextureRT = CreateRenderTarget(PressureTexture->GetResource()->GetTextureRHI(), TEXT("PressureTexture"));
			FRDGTextureRef OutPreRT = GraphBuilder.RegisterExternalTexture(PressureTextureRT);*/

			OutputArray.Add(SceneTextures->GetContents()->SceneColorTexture);
			OutputArray.Add(SceneTextures->GetContents()->SceneDepthTexture);

			PhysicalSolver->SetParameter(SolverContext.SolverParameter);
			PhysicalSolver->Update_RenderThread(GraphBuilder,OutputArray,SolverContext);
		});
}
