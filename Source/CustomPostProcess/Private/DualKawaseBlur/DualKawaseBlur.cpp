// Copyright 2023 Natsu Neko, Inc. All Rights Reserved.


#include "DualKawaseBlur.h"

#include "PixelShaderUtils.h"
#include "RenderGraphEvent.h"
#include "ScreenPass.h"
#include "PostProcess/PostProcessing.h"
#include "PostProcess/PostProcessMaterial.h"

class FDualKawaseBlurDCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FDualKawaseBlurDCS);
	SHADER_USE_PARAMETER_STRUCT(FDualKawaseBlurDCS, FGlobalShader);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters,
	                                         FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("NUMTHREADS_X"), 32);
		OutEnvironment.SetDefine(TEXT("NUMTHREADS_Y"), 32);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
		SHADER_PARAMETER(FVector4f, ViewSize)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_SAMPLER(SamplerState, Sampler)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, Intexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutTexture)

		END_SHADER_PARAMETER_STRUCT()
};

class FDualKawaseBlurUCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FDualKawaseBlurUCS);
	SHADER_USE_PARAMETER_STRUCT(FDualKawaseBlurUCS, FGlobalShader);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters,
	                                         FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("NUMTHREADS_X"), 32);
		OutEnvironment.SetDefine(TEXT("NUMTHREADS_Y"), 32);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
		SHADER_PARAMETER(FVector4f, ViewSize)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, Intexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, Sampler)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutTexture)

		END_SHADER_PARAMETER_STRUCT()
};

class FGaussianBlurCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FGaussianBlurCS);
	SHADER_USE_PARAMETER_STRUCT(FGaussianBlurCS, FGlobalShader);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters,
	                                         FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("NUMTHREADS_X"), 32);
		OutEnvironment.SetDefine(TEXT("NUMTHREADS_Y"), 32);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
		SHADER_PARAMETER(FVector4f, ViewSize)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, Intexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, Sampler)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutTexture)

		END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FDualKawaseBlurDCS, "/ProjectShader/DualKawaseBlur.usf", "DownSampleCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FDualKawaseBlurUCS, "/ProjectShader/DualKawaseBlur.usf", "UpSampleCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FGaussianBlurCS, "/ProjectShader/DualKawaseBlur.usf", "GaussianBlur", SF_Compute);


UDualKawaseBlur::UDualKawaseBlur()
{
}

void UDualKawaseBlur::PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs)
{

	//const FScreenPassTexture SceneColor = (*Inputs.SceneTextures)->SceneColorTexture;
	const FViewInfo& ViewInfo = static_cast<const FViewInfo&>(View);
	FRDGTextureDesc TextureDesc = (*Inputs.SceneTextures)->SceneColorTexture->Desc;
	FRDGTextureRef TempTexture = GraphBuilder.CreateTexture(TextureDesc,TEXT("OutSceneColor"));
	FRDGTextureRef SceneColor = (*Inputs.SceneTextures)->SceneColorTexture;
	AddCopyTexturePass(GraphBuilder,SceneColor,TempTexture);

	TextureDesc.Flags = ETextureCreateFlags::UAV;

	if (!bUseGaussianPass)
	{
		RDG_EVENT_SCOPE(GraphBuilder, "DualKawaseBlurCompute");
		for (int i = 0; i < PassNum; ++i)
		{
			FRDGTextureRef OutPutTexture =  GraphBuilder.CreateTexture(TextureDesc, TEXT("DluaKawaseDownPass"));

			FDualKawaseBlurDCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FDualKawaseBlurDCS::FParameters>();
			PassParameters->Intexture = TempTexture;
			PassParameters->ViewSize = FVector4f(TextureDesc.Extent.X, TextureDesc.Extent.Y, Offset, 0);
			PassParameters->View = View.ViewUniformBuffer;
			PassParameters->Sampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			PassParameters->OutTexture = GraphBuilder.CreateUAV(OutPutTexture);;

			TShaderMapRef<FDualKawaseBlurDCS> ComputeShader(ViewInfo.ShaderMap);
			FComputeShaderUtils::AddPass(GraphBuilder,
			                             RDG_EVENT_NAME("DualKawaseBlurDownSample %dx%d (CS)", TextureDesc.Extent.X, TextureDesc.Extent.Y),
			                             ComputeShader,
			                             PassParameters,
			                             FComputeShaderUtils::GetGroupCount(TextureDesc.Extent, FIntPoint(32, 32)));
			//OutSceneRenderTarget = nullptr;
			TextureDesc.Extent.X = TextureDesc.Extent.X / 2;
			TextureDesc.Extent.Y = TextureDesc.Extent.Y / 2;

			TempTexture = OutPutTexture;
		}

		for (int i = 0; i < PassNum; ++i)
		{
			TextureDesc.Extent.X = TextureDesc.Extent.X * 2;
			TextureDesc.Extent.Y = TextureDesc.Extent.Y * 2;

			FRDGTextureRef OutPutTexture = GraphBuilder.CreateTexture(TextureDesc, TEXT("DluaKawaseDownPass"));;

			FDualKawaseBlurUCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FDualKawaseBlurUCS::FParameters>();
			PassParameters->Intexture = TempTexture;
			PassParameters->ViewSize = FVector4f(TextureDesc.Extent.X, TextureDesc.Extent.Y, Offset, 0);
			PassParameters->View = View.ViewUniformBuffer;
			PassParameters->Sampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			PassParameters->OutTexture = GraphBuilder.CreateUAV(OutPutTexture);;

			TShaderMapRef<FDualKawaseBlurUCS> ComputeShader(ViewInfo.ShaderMap);
			FComputeShaderUtils::AddPass(GraphBuilder,
			                             RDG_EVENT_NAME("DualKawaseBlurUpSample %dx%d (CS)", TextureDesc.Extent.X, TextureDesc.Extent.Y),
			                             ComputeShader,
			                             PassParameters,
			                             FComputeShaderUtils::GetGroupCount(TextureDesc.Extent, FIntPoint(32, 32)));
			TempTexture = OutPutTexture;

		}
		AddCopyTexturePass(GraphBuilder,TempTexture,SceneColor);
		//SceneColor = MoveTemp(TempTexture);
	}
	else
	{
		RDG_EVENT_SCOPE(GraphBuilder, "GaussainBlur");
		//TextureDesc.Flags = ETextureCreateFlags::UAV;
		//TextureDesc.Extent *= 1/0.77;
		FGaussianBlurCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FGaussianBlurCS::FParameters>();
		PassParameters->Intexture = TempTexture;
		PassParameters->ViewSize = FVector4f(TextureDesc.Extent.X, TextureDesc.Extent.Y, Offset, 0);
		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->Sampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		PassParameters->OutTexture = GraphBuilder.CreateUAV(SceneColor);
		TShaderMapRef<FGaussianBlurCS> ComputeShader(ViewInfo.ShaderMap);
		FComputeShaderUtils::AddPass(GraphBuilder,
									 RDG_EVENT_NAME("GaussianBlur %dx%d (CS)", TextureDesc.Extent.X, TextureDesc.Extent.Y),
									 ComputeShader,
									 PassParameters,
									 FComputeShaderUtils::GetGroupCount(TextureDesc.Extent, FIntPoint(32, 32)));

	}

}


