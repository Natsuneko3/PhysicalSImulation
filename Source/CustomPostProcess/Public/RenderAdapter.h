#pragma once
#include "PostProcess/PostProcessing.h"
#include "RenderGraphBuilder.h"
#include "PostProcess/PostProcessDownsample.h"
#include "RenderAdapter.generated.h"
class FCPPSceneProxy;


class FCommonMeshVS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FCommonMeshVS);
	SHADER_USE_PARAMETER_STRUCT(FCommonMeshVS, FGlobalShader);

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER(FMatrix44f, LocalToWorld)
	END_SHADER_PARAMETER_STRUCT()
};

DEFINE_LOG_CATEGORY_STATIC(LogCustomPostProcess, Log, All);

DECLARE_STATS_GROUP(TEXT("Custom Render Feature"), STATGROUP_CRF, STATCAT_Advanced)

UENUM()
enum class EBlurMethod:uint8
{
	BilateralFilter ,
	DualKawase ,
	Gaussian
};

UENUM()
enum class EBlendMethod:uint8
{
	Addition,
	Multiply,
	Interpolation,
	DepthInterpolation,
	InvDepthInterpolation,
	MAX
};

USTRUCT()
struct FTextureBlendDesc
{
	GENERATED_BODY()
	UPROPERTY(Category = "BaseParameter|RenderAdapter",EditAnywhere)
	float Weight = 1.0;
	UPROPERTY(Category = "BaseParameter|RenderAdapter",EditAnywhere)
	EBlendMethod BlendMethod = EBlendMethod::Interpolation;
};

struct FBlurParameter
{
	EBlurMethod BlurMethod = EBlurMethod::BilateralFilter;
	float BlurSize = 1.0;
	float Sigma = 10;
	int Step = 3;
	float ScreenPercent = 100.0;
};

struct FDownSampleParameter
{
	EDownsampleQuality Quality;
	bool bUseComputeShader;
	bool bNeedClampLuminance;
	float BloomThreshold;
	float UVScale;
};

UCLASS(Abstract, Blueprintable, EditInlineNew, CollapseCategories )
class URenderAdapterBase :public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(Category = "BaseParameter|RenderAdapter",EditAnywhere)
	bool bEnable = true;

	UPROPERTY(Category = "BaseParameter|RenderAdapter",EditAnywhere)
	bool bTranslucentOnly ;

	UPROPERTY(Category = "BaseParameter|RenderAdapter",EditAnywhere,meta=(ClampMin=10,ClampMax=100,UIMin=10,UIMax=100))
	float ScreenPercent = 100.0;

	UPROPERTY(Category = "BaseParameter|RenderAdapter",EditAnywhere,meta=(DisplayName="Texture Blend"))
	FTextureBlendDesc TextureBlendDesc;


	URenderAdapterBase(){}

	virtual void Initial_RenderThread(FRHICommandListImmediate& RHICmdList){}

	virtual void PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView){}

	virtual void PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily){}

	virtual void PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs){};

	void AddTextureBlurPass(FRDGBuilder& GraphBuilder,const FViewInfo& View,FRDGTextureRef InTexture,FRDGTextureRef& OutTexture,FBlurParameter BlurParameter);
	void InitialPlaneMesh(FRHICommandList& RHICmdList);
	void InitialCubeMesh(FRHICommandList& RHICmdList);
	void AddTextureCombinePass(FRDGBuilder& GraphBuilder,const FViewInfo& View, const FPostProcessingInputs& Inputs,FRDGTextureRef InTexture,FRDGTextureRef& OutTexture,FTextureBlendDesc* InTextureBlendDesc);
	void AddDownsamplePass(FRDGBuilder& GraphBuilder, const FViewInfo& View, FScreenPassTexture Input, FScreenPassTexture Output, FDownSampleParameter DownSampleParameter);
	virtual void Release(){}

	FBufferRHIRef VertexBufferRHI;
	FBufferRHIRef IndexBufferRHI;
	int32 Frame;
	template <typename TShaderClassPS>
	void DrawMesh(const FViewInfo& View,
	FMatrix44f Transform,
		const TShaderRef<TShaderClassPS>& PixelShader,
	              typename TShaderClassPS::FParameters* InPSParameters,
	              FRDGBuilder& GraphBuilder, const FIntRect& ViewportRect, uint32 NumInstance)
	{
		if (NumPrimitives == 0)
		{
			return;
		}
		FCommonMeshVS::FParameters* InVSParameters = GraphBuilder.AllocParameters<FCommonMeshVS::FParameters>();
		InVSParameters->View = View.ViewUniformBuffer;
		InVSParameters->LocalToWorld = Transform;

		FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
		TShaderMapRef<FCommonMeshVS> VertexShader(GlobalShaderMap);
		FString Name = NumPrimitives==2?"DrawPlaneMesh":"DrawCubeMesh";
		GraphBuilder.AddPass(
			RDG_EVENT_NAME("%s",*Name),
			InPSParameters,
			ERDGPassFlags::Raster,
			[VertexShader,InVSParameters,PixelShader,InPSParameters,ViewportRect,this,NumInstance](FRHICommandList& RHICmdList)
			{
				FGraphicsPipelineStateInitializer GraphicsPSOInit;
				RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
				GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero>::GetRHI();
				GraphicsPSOInit.RasterizerState = GetStaticRasterizerState<true>(FM_Solid, CM_CW);
				GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Less>::GetRHI();
				GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
				GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
				GraphicsPSOInit.PrimitiveType = PT_TriangleList;
				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

				SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), *InVSParameters);
				SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), *InPSParameters);

				RHICmdList.SetViewport(ViewportRect.Min.X, ViewportRect.Min.Y, 0.0f, ViewportRect.Max.X, ViewportRect.Max.Y, 1.0f);
				RHICmdList.SetStreamSource(0, VertexBufferRHI, 0);

				RHICmdList.DrawIndexedPrimitive(IndexBufferRHI, 0, 0, NumVertices, 0, NumPrimitives, NumInstance);
			});
	}
	uint32 NumPrimitives;
	uint32 NumVertices;
	/*This function will blend InTexture into OutTexture*/
protected:
	FRDGTextureRef GetSceneTexture(const FPostProcessingInputs& Inputs);
private:

	void DrawDualKawaseBlur(FRDGBuilder& GraphBuilder,const FViewInfo& View,FRDGTextureRef InTexture,FRDGTextureRef& OutTexture,FBlurParameter* BilateralParameter);
	void DrawBilateralFilter(FRDGBuilder& GraphBuilder,const FViewInfo& View,FRDGTextureRef InTexture,FRDGTextureRef& OutTexture,FBlurParameter* BilateralParameter);
	void DrawGaussianBlur(FRDGBuilder& GraphBuilder,const FViewInfo& View,FRDGTextureRef InTexture,FRDGTextureRef& OutTexture,FBlurParameter* BilateralParameter);

};

