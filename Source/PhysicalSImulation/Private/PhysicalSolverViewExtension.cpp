#include "PhysicalSolverViewExtension.h"

#include "CommonRenderResources.h"
#include "Physical2DFluidSolver.h"
#include "RenderGraphBuilder.h"
#include "Renderer/Private/SceneRendering.h"
/*class FPhysicalMeshVS : public FMaterialShader
{
	DECLARE_SHADER_TYPE(FPhysicalMeshVS, Material);
public:
	FPhysicalMeshVS() {}
	FPhysicalMeshVS(const FMaterialShaderType::CompiledShaderInitializerType& Initializer);
	void SetParameters(FRHICommandList& RHICmdList, const FSceneView View)
	{
		FRHIVertexShader* ShaderRHI = RHICmdList.GetBoundVertexShader();
		FMaterialShader::SetViewParameters(RHICmdList, ShaderRHI, View, View.ViewUniformBuffer);
	}
};
IMPLEMENT_GLOBAL_SHADER(FPhysicalMeshVS,"/Engine/Private/BasePassVertexShader.usf","Main",SF_Vertex);

class FPhysicalMeshPS : public FMaterialShader
{
	DECLARE_SHADER_TYPE(FPhysicalMeshPS, Material);
public:
	FPhysicalMeshPS() {}
	FPhysicalMeshPS(const FMaterialShaderType::CompiledShaderInitializerType& Initializer);
	void SetParameters(FRHICommandList& RHICmdList, const FSceneView View, const FMaterialRenderProxy* MaterialProxy, const FMaterial& Material)
	{
		FRHIPixelShader* ShaderRHI = RHICmdList.GetBoundPixelShader();
		FMaterialShader::SetViewParameters(RHICmdList, ShaderRHI, View, View.ViewUniformBuffer);
		FMaterialShader::SetParameters(RHICmdList, ShaderRHI, MaterialProxy, Material, View);
	}
};
IMPLEMENT_GLOBAL_SHADER(FPhysicalMeshPS,"/Engine/Private/BasePassPixelShader.usf","MainPS",SF_Vertex);

void FPhysicalSolverViewExtension::SetupViewFamily(FSceneViewFamily& InViewFamily)
{
	PhysicalSolver = MakeShareable(new FPhysical2DFluidSolver).Object;
}*/

void FPhysicalSolverViewExtension::SetupViewFamily(FSceneViewFamily& InViewFamily)
{
	PhysicalSolver = MakeShareable(new FPhysical2DFluidSolver).Object;


}

void FPhysicalSolverViewExtension::PostRenderBasePassDeferred_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView, const FRenderTargetBindingSlots& RenderTargets,
                                                                           TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTextures)
{


	/*TArray<FRDGTextureRef> OutputArray;


	PhysicalSolver->SetParameter(SolverContext.SolverParameter);
	PhysicalSolver->Update_RenderThread(GraphBuilder,OutputArray,SolverContext);
	GraphBuilder.AddPass(RDG_EVENT_NAME("Niagara::ExecuteTicksPost"),
		ERDGPassFlags::Raster,
		[this,&GraphBuilder,InView,SceneTextures](FRHICommandListImmediate& RHICmdList)
		{



			TRefCountPtr<IPooledRenderTarget> PreViewTextureRT = CreateRenderTarget(SimulationTexture->GetResource()->GetTextureRHI(), TEXT("PreViewTexture"));
			FRDGTextureRef OutRT = GraphBuilder.RegisterExternalTexture(PreViewTextureRT);

			TRefCountPtr<IPooledRenderTarget> PressureTextureRT = CreateRenderTarget(PressureTexture->GetResource()->GetTextureRHI(), TEXT("PressureTexture"));
			FRDGTextureRef OutPreRT = GraphBuilder.RegisterExternalTexture(PressureTextureRT);*/


			/*UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic> (SolverContext.MeshMaterial);
			//MID->SetTextureParameterValue(TEXT("RT"),OutputArray[1]);
			const FMaterialRenderProxy* MaterialProxy = SolverContext.MeshMaterial->GetRenderProxy();
			const FMaterial& MeshMaterial = MaterialProxy->GetMaterialWithFallback(InView.GetFeatureLevel(), MaterialProxy);
			const FMaterialShaderMap* MaterialShaderMap = MeshMaterial.GetRenderingThreadShaderMap();

			TShaderRef<FPhysicalMeshVS> VertexShader = MaterialShaderMap->GetShader<FPhysicalMeshVS>();
			TShaderRef<FPhysicalMeshPS>  PixelShader = MaterialShaderMap->GetShader<FPhysicalMeshPS>();

			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
			GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;

			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);
			VertexShader->SetParameters(RHICmdList, InView);
			PixelShader->SetParameters(RHICmdList, InView, MaterialProxy, MeshMaterial);


			//SetUniformBufferParameterImmediate(RHICmdList, VertexShader.GetVertexShader(), VertexShader->GetUniformBufferParameter<FViewUniformShaderParameters>(), InView.ViewUniformBuffer);
			SolverContext.BoundingMesh->GetRenderData()->LODResources[0].VertexBuffers.StaticMeshVertexBuffer
			RHICmdList.SetStreamSource(0, , 0);
			RHICmdList.DrawIndexedPrimitive(
				OverlayIndexBufferRHI,
				/*BaseVertexIndex=#1# 0,
				/*MinIndex=#1# 0,
				/*NumVertices=#1# 4,
				/*StartIndex=#1# 0,
				/*NumPrimitives=#1# 2,
				/*NumInstances=#1# 1
			);
		});*/
}
