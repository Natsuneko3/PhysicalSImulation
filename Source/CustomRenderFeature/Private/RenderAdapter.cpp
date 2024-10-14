#include "RenderAdapter.h"

#include "PixelShaderUtils.h"
#include "PostProcess/PostProcessDownsample.h"

const int32 GDownsampleTileSizeX = 8;
const int32 GDownsampleTileSizeY = 8;

BEGIN_SHADER_PARAMETER_STRUCT(FDownsampleParameters, )
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
	SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, Input)
	SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, Output)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)

END_SHADER_PARAMETER_STRUCT()

class FBilateralFilterCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FBilateralFilterCS);
	SHADER_USE_PARAMETER_STRUCT(FBilateralFilterCS, FGlobalShader);

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters,
	                                         FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), 8);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), 8);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, InputTextureSampler)


		SHADER_PARAMETER(FVector2f, BlurDir)
		SHADER_PARAMETER(FVector2f, TextureSize)
		SHADER_PARAMETER(float, Coefficient)
		SHADER_PARAMETER(int, Step)
		SHADER_PARAMETER(float, Sigma)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutTexture)

	END_SHADER_PARAMETER_STRUCT()
};

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

class FTextureBlendCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTextureBlendCS);
	SHADER_USE_PARAMETER_STRUCT(FTextureBlendCS, FGlobalShader);
    
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
	    SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTextures)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D,InTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D,SceneTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, ClampSampler)
	    SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutTexture)
	SHADER_PARAMETER(float, Weight)
	END_SHADER_PARAMETER_STRUCT()
	class FBlendMethod	  : SHADER_PERMUTATION_ENUM_CLASS("COMBINEMETHOD",EBlendMethod);

	using FPermutationDomain = TShaderPermutationDomain<FBlendMethod>;
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), 8);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), 8);
		OutEnvironment.SetDefine(TEXT("ADDITION"), 0);
		OutEnvironment.SetDefine(TEXT("MULTIPLY"), 1);
		OutEnvironment.SetDefine(TEXT("INTERPOLATION"), 2);
		OutEnvironment.SetDefine(TEXT("DEPTHINTERPOLATION"), 3);
		OutEnvironment.SetDefine(TEXT("INVDEPTHINTERPOLATION"), 4);
		//OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), 8);
	}
};

class FTextureBlendPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTextureBlendPS);
	SHADER_USE_PARAMETER_STRUCT(FTextureBlendPS, FGlobalShader);

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("ADDITION"), 0);
		OutEnvironment.SetDefine(TEXT("MULTIPLY"), 1);
		OutEnvironment.SetDefine(TEXT("INTERPOLATION"), 2);
		OutEnvironment.SetDefine(TEXT("DEPTHINTERPOLATION"), 3);
		OutEnvironment.SetDefine(TEXT("INVDEPTHINTERPOLATION"), 4);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTextures)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D,InTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D,SceneTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, ClampSampler)
		SHADER_PARAMETER(float, Weight)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
	class FBlendMethod	  : SHADER_PERMUTATION_ENUM_CLASS("COMBINEMETHOD",EBlendMethod);
	using FPermutationDomain = TShaderPermutationDomain<FBlendMethod>;
};

class FDownsamplePS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FDownsamplePS);
	SHADER_USE_PARAMETER_STRUCT(FDownsamplePS, FGlobalShader);


	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FDownsampleParameters, Common)
	SHADER_PARAMETER(float,UVScale)
	SHADER_PARAMETER(int,Index)
	SHADER_PARAMETER(float,BloomThreshold)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
	class FQuality	  : SHADER_PERMUTATION_ENUM_CLASS("DOWNSAMPLE_QUALITY",EDownsampleQuality);
	class FNeedClampLuminance	  : SHADER_PERMUTATION_BOOL("NEEDCLAMPLUMINANCE");
	using FPermutationDomain = TShaderPermutationDomain<FQuality,FNeedClampLuminance>;
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}
};

class FDownsampleCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FDownsampleCS);
	SHADER_USE_PARAMETER_STRUCT(FDownsampleCS, FGlobalShader);


	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FDownsampleParameters, Common)
		SHADER_PARAMETER(FScreenTransform, DispatchThreadIdToInputUV)
		SHADER_PARAMETER(float,BloomThreshold)
	SHADER_PARAMETER(int,Index)
	SHADER_PARAMETER(float,UVScale)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutComputeTexture)
	END_SHADER_PARAMETER_STRUCT()
	class FQuality	  : SHADER_PERMUTATION_ENUM_CLASS("DOWNSAMPLE_QUALITY",EDownsampleQuality);
	class FNeedClampLuminance	  : SHADER_PERMUTATION_BOOL("NEEDCLAMPLUMINANCE");
	using FPermutationDomain = TShaderPermutationDomain<FQuality,FNeedClampLuminance>;
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), GDownsampleTileSizeX);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), GDownsampleTileSizeY);
	}
};

IMPLEMENT_GLOBAL_SHADER(FDownsamplePS, "/Plugin/CustomRenderFeature/DownSample.usf", "MainPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FDownsampleCS, "/Plugin/CustomRenderFeature/DownSample.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTextureBlendPS, "/Plugin/CustomRenderFeature/TextureBlend.usf", "MainPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FTextureBlendCS, "/Plugin/CustomRenderFeature/TextureBlend.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FDualKawaseBlurDCS, "/Plugin/CustomRenderFeature/TextureBlur/DualKawaseBlur.usf", "DownSampleCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FDualKawaseBlurUCS, "/Plugin/CustomRenderFeature/TextureBlur/DualKawaseBlur.usf", "UpSampleCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FGaussianBlurCS, "/Plugin/CustomRenderFeature/TextureBlur/DualKawaseBlur.usf", "GaussianBlur", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FCommonMeshVS, "/Plugin/CustomRenderFeature/SmokePlanePass.usf", "MainVS", SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FBilateralFilterCS, "/Plugin/CustomRenderFeature/TextureBlur/TextureBlur.usf", "MainCS", SF_Compute);

///////////////////URenderAdapterBase/////////////////////

FDownsampleParameters GetDownsampleParameters(const FViewInfo& View, FScreenPassTexture Output, FScreenPassTexture Input, EDownsampleQuality DownsampleMethod)
{
	check(Output.IsValid());
	check(Input.IsValid());

	const FScreenPassTextureViewportParameters InputParameters = GetScreenPassTextureViewportParameters(FScreenPassTextureViewport(Input));
	const FScreenPassTextureViewportParameters OutputParameters = GetScreenPassTextureViewportParameters(FScreenPassTextureViewport(Output));

	FDownsampleParameters Parameters;
	Parameters.ViewUniformBuffer = View.ViewUniformBuffer;
	Parameters.Input = InputParameters;
	Parameters.Output = OutputParameters;
	Parameters.InputTexture = Input.Texture;
	Parameters.InputSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	return Parameters;
}

void URenderAdapterBase::InitialPlaneMesh(FRHICommandList& RHICmdList)
{
	TResourceArray<FFilterVertex, VERTEXBUFFER_ALIGNMENT> Vertices;
	Vertices.SetNumUninitialized(8);

	Vertices[0].Position = FVector4f(0, 0, 0, 1.f);
	Vertices[0].UV = FVector2f(0.f, 0.f);
	Vertices[1].Position = FVector4f(1, 0, 0, 1.f);
	Vertices[1].UV = FVector2f(1.f, 0.f);
	Vertices[2].Position = FVector4f(0, 1, 0, 1.f);
	Vertices[2].UV = FVector2f(0.f, 1.f);
	Vertices[3].Position = FVector4f(1, 1, 0, 1.f);
	Vertices[3].UV = FVector2f(1.f, 1.f);
	// Setup index buffer
	const uint16 SpriteIndices[] = {
		2, 3, 1, 2, 1, 0
	};


	TResourceArray<uint16, INDEXBUFFER_ALIGNMENT> IndexBuffer;
	const uint32 NumIndices = UE_ARRAY_COUNT(SpriteIndices);
	IndexBuffer.SetNumUninitialized(NumIndices);
	FMemory::Memcpy(IndexBuffer.GetData(), SpriteIndices, NumIndices * sizeof(uint16));

	FRHIResourceCreateInfo CreateInfoIB(TEXT("PSPlaneMeshIndexBuffer"), &IndexBuffer);
	FRHIResourceCreateInfo CreateInfoVB(TEXT("PSPlaneMeshVertexBuffer"), &Vertices);
	IndexBufferRHI = RHICmdList.CreateIndexBuffer( sizeof(uint16), sizeof(uint16) * NumIndices, BUF_Static, CreateInfoIB);
	VertexBufferRHI = RHICmdList.CreateVertexBuffer(Vertices.GetResourceDataSize(), BUF_Static,  CreateInfoVB);

	//IndexBufferRHI = RHICmdList.CreateIndexBuffer(sizeof(uint16), IndexBuffer.GetResourceDataSize(), BUF_Static, CreateInfoIB);
	//RHICreateIndexBuffer(sizeof(uint16), IndexBuffer.GetResourceDataSize(), static_cast<uint32>(BUF_Static), CreateInfoIB);
	NumPrimitives = 2;
	NumVertices = 4;
}

void URenderAdapterBase::InitialCubeMesh(FRHICommandList& RHICmdList)
{
	TResourceArray<FFilterVertex, VERTEXBUFFER_ALIGNMENT> Vertices;
	Vertices.SetNumUninitialized(8);

	FVector3f BboxMin(0, 0, 0);
	FVector3f BboxMax(1, 1, 1);

	// Front face
	Vertices[0].Position = FVector4f(BboxMin.X, BboxMin.Y, BboxMin.Z, 1.f);
	Vertices[0].UV = FVector2f(0.f, 0.f);
	Vertices[1].Position = FVector4f(BboxMax.X, BboxMin.Y, BboxMin.Z, 1.f);
	Vertices[1].UV = FVector2f(1.f, 0.f);
	Vertices[2].Position = FVector4f(BboxMin.X, BboxMax.Y, BboxMin.Z, 1.f);
	Vertices[2].UV = FVector2f(0.f, 1.f);
	Vertices[3].Position = FVector4f(BboxMax.X, BboxMax.Y, BboxMin.Z, 1.f);
	Vertices[3].UV = FVector2f(1.f, 1.f);
	// Back face		   FVector4f
	Vertices[4].Position = FVector4f(BboxMin.X, BboxMin.Y, BboxMax.Z, 1.f);
	Vertices[0].UV = FVector2f(1.f, 1.f);
	Vertices[5].Position = FVector4f(BboxMax.X, BboxMin.Y, BboxMax.Z, 1.f);
	Vertices[1].UV = FVector2f(1.f, 0.f);
	Vertices[6].Position = FVector4f(BboxMin.X, BboxMax.Y, BboxMax.Z, 1.f);
	Vertices[2].UV = FVector2f(0.f, 1.f);
	Vertices[7].Position = FVector4f(BboxMax.X, BboxMax.Y, BboxMax.Z, 1.f);
	Vertices[3].UV = FVector2f(0.f, 0.f);

	FRHIResourceCreateInfo CreateInfoVB(TEXT("PSCubeMeshVertexBuffer"), &Vertices);

	// Setup index buffer
	const uint16 Indices[] = {
		// bottom face
		0, 1, 2,
		1, 3, 2,
		// right face
		1, 5, 3,
		3, 5, 7,
		// front face
		3, 7, 6,
		2, 3, 6,
		// left face
		2, 4, 0,
		2, 6, 4,
		// back face
		0, 4, 5,
		1, 0, 5,
		// top face
		5, 4, 6,
		5, 6, 7
	};

	TResourceArray<uint16, INDEXBUFFER_ALIGNMENT> IndexBuffer;
	const uint32 NumIndices = UE_ARRAY_COUNT(Indices);
	IndexBuffer.AddUninitialized(NumIndices);
	FMemory::Memcpy(IndexBuffer.GetData(), Indices, NumIndices * sizeof(uint16));

	FRHIResourceCreateInfo CreateInfoIB(TEXT("PSCubeMeshIndexBuffer"), &IndexBuffer);
	//IndexBufferRHI = RHICmdList.CreateBuffer( IndexBuffer.GetResourceDataSize(), BUF_Static, 0, ERHIAccess::VertexOrIndexBuffer,CreateInfoIB);
	//IndexBufferRHI = RHICmdList.CreateIndexBuffer(sizeof(uint16), IndexBuffer.GetResourceDataSize(), BUF_Static, CreateInfoIB);

	IndexBufferRHI = RHICmdList.CreateIndexBuffer( 0, sizeof(uint16) * NumIndices, BUF_Static, ERHIAccess::VertexOrIndexBuffer,CreateInfoIB);
	VertexBufferRHI = RHICmdList.CreateVertexBuffer(Vertices.GetResourceDataSize(), BUF_Static, ERHIAccess::VertexOrIndexBuffer,CreateInfoVB) ;//RHICreateVertexBuffer();

	NumPrimitives = 12;
	NumVertices = 8;
}

FRDGTextureRef URenderAdapterBase::GetSceneTexture(const FPostProcessingInputs& Inputs)
{
	FRDGTextureRef OutTexture = nullptr;
	if(bTranslucentOnly)
	{
		OutTexture = Inputs.TranslucencyViewResourcesMap.Get(ETranslucencyPass::TPT_TranslucencyAfterDOF).ColorTexture.Target;
	}
	else
	{
		OutTexture = (*Inputs.SceneTextures)->SceneColorTexture;
	}
	return OutTexture;
}

void URenderAdapterBase::DrawDualKawaseBlur(FRDGBuilder& GraphBuilder, const FViewInfo& View, FRDGTextureRef InTexture, FRDGTextureRef& OutTexture,FBlurParameter* BilateralParameter)
{
	const FViewInfo& ViewInfo = static_cast<const FViewInfo&>(View);
	FRDGTextureDesc TextureDesc =OutTexture->Desc;
	FRDGTextureRef TempTexture = GraphBuilder.CreateTexture(TextureDesc,TEXT("OutSceneColor"));
	FRDGTextureRef SceneColor = InTexture;
	AddCopyTexturePass(GraphBuilder,SceneColor,TempTexture);

	TextureDesc.Flags = ETextureCreateFlags::UAV;

	RDG_EVENT_SCOPE(GraphBuilder, "DualKawaseBlurCompute");
	for (int i = 0; i < BilateralParameter->Step; ++i)
	{
		FRDGTextureRef OutPutTexture =  GraphBuilder.CreateTexture(TextureDesc, TEXT("DluaKawaseDownPass"));

		FDualKawaseBlurDCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FDualKawaseBlurDCS::FParameters>();
		PassParameters->Intexture = TempTexture;
		PassParameters->ViewSize = FVector4f(TextureDesc.Extent.X, TextureDesc.Extent.Y, BilateralParameter->BlurSize, 0);
		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->Sampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		PassParameters->OutTexture = GraphBuilder.CreateUAV(OutPutTexture);;

		TShaderMapRef<FDualKawaseBlurDCS> ComputeShader(ViewInfo.ShaderMap);
		FComputeShaderUtils::AddPass(GraphBuilder,
		                             RDG_EVENT_NAME("DualKawaseBlurDownSample %dx%d (CS)", TextureDesc.Extent.X, TextureDesc.Extent.Y),
		                             ComputeShader,
		                             PassParameters,
		                             FComputeShaderUtils::GetGroupCount(TextureDesc.Extent, FIntPoint(32, 32)));
		TextureDesc.Extent.X = TextureDesc.Extent.X / 2;
		TextureDesc.Extent.Y = TextureDesc.Extent.Y / 2;

		TempTexture = OutPutTexture;
	}

	for (int i = 0; i < BilateralParameter->Step; ++i)
	{
		TextureDesc.Extent.X = TextureDesc.Extent.X * 2;
		TextureDesc.Extent.Y = TextureDesc.Extent.Y * 2;

		FRDGTextureRef OutPutTexture = GraphBuilder.CreateTexture(TextureDesc, TEXT("DluaKawaseDownPass"));;

		FDualKawaseBlurUCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FDualKawaseBlurUCS::FParameters>();
		PassParameters->Intexture = TempTexture;
		PassParameters->ViewSize = FVector4f(TextureDesc.Extent.X, TextureDesc.Extent.Y, BilateralParameter->BlurSize, 0);
		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->Sampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		if(i==BilateralParameter->Step - 1)
		{
			PassParameters->OutTexture = GraphBuilder.CreateUAV(OutTexture);
		}
		else
		{
			PassParameters->OutTexture = GraphBuilder.CreateUAV(OutPutTexture);
		}


		TShaderMapRef<FDualKawaseBlurUCS> ComputeShader(ViewInfo.ShaderMap);
		FComputeShaderUtils::AddPass(GraphBuilder,
		                             RDG_EVENT_NAME("DualKawaseBlurUpSample %dx%d (CS)", TextureDesc.Extent.X, TextureDesc.Extent.Y),
		                             ComputeShader,
		                             PassParameters,
		                             FComputeShaderUtils::GetGroupCount(TextureDesc.Extent, FIntPoint(32, 32)));
		TempTexture = OutPutTexture;

	}
	//AddCopyTexturePass(GraphBuilder,TempTexture,OutTexture);
}

void URenderAdapterBase::DrawBilateralFilter(FRDGBuilder& GraphBuilder, const FViewInfo& View, FRDGTextureRef InTexture, FRDGTextureRef& OutTexture,FBlurParameter* BilateralParameter)
{
	FString IsHorizon;
	FVector2f BlurDir;

	IsHorizon = "Horizon";
	BlurDir = FVector2f(1.0f, 0.0f);

	FRDGTextureRef TempTexture = GraphBuilder.CreateTexture(OutTexture->Desc,TEXT("BilateralFilterTempTexture"));

	FIntPoint ViewSize = OutTexture->Desc.Extent;

	FBilateralFilterCS::FParameters* HorizonPassParameters = GraphBuilder.AllocParameters<FBilateralFilterCS::FParameters>();

	HorizonPassParameters->InputTexture = InTexture;
	HorizonPassParameters->InputTextureSampler = TStaticSamplerState<SF_Point>::GetRHI();
	HorizonPassParameters->Coefficient = BilateralParameter->BlurSize;
	HorizonPassParameters->Sigma = BilateralParameter->Sigma;
	HorizonPassParameters->BlurDir = BlurDir;
	HorizonPassParameters->Step = BilateralParameter->Step;
	HorizonPassParameters->OutTexture = GraphBuilder.CreateUAV(TempTexture);
	TShaderMapRef<FBilateralFilterCS> ComputeShader(View.ShaderMap);

	FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(ViewSize , FIntPoint(8, 8));
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("BilateralFilter %s %dx%d(CS)",
		               *IsHorizon, ViewSize.X, ViewSize.Y),
		ComputeShader, HorizonPassParameters,
		GroupCount);

	IsHorizon = "Vertical";
	BlurDir = FVector2f(0.0f, 1.0f);
	FBilateralFilterCS::FParameters* VerticalPassParameters = GraphBuilder.AllocParameters<FBilateralFilterCS::FParameters>();

	VerticalPassParameters->InputTexture = TempTexture;
	VerticalPassParameters->InputTextureSampler = TStaticSamplerState<SF_Point>::GetRHI();
	VerticalPassParameters->Coefficient = BilateralParameter->BlurSize;
	VerticalPassParameters->Sigma = BilateralParameter->Sigma;
	VerticalPassParameters->BlurDir = BlurDir;
	VerticalPassParameters->Step = BilateralParameter->Step;
	VerticalPassParameters->OutTexture = GraphBuilder.CreateUAV(OutTexture);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("BilateralFilter %s %dx%d(CS)",
		               *IsHorizon, ViewSize.X, ViewSize.Y),
		ComputeShader, VerticalPassParameters,
		GroupCount);
}

void URenderAdapterBase::DrawGaussianBlur(FRDGBuilder& GraphBuilder, const FViewInfo& View, FRDGTextureRef InTexture, FRDGTextureRef& OutTexture,FBlurParameter* BilateralParameter)
{
	const FViewInfo& ViewInfo = static_cast<const FViewInfo&>(View);
	FRDGTextureDesc TextureDesc = InTexture->Desc;

	RDG_EVENT_SCOPE(GraphBuilder, "GaussianBlur");
	//TextureDesc.Flags = ETextureCreateFlags::UAV;
	//TextureDesc.Extent *= 1/0.77;

	FGaussianBlurCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FGaussianBlurCS::FParameters>();
	PassParameters->Intexture = InTexture;
	PassParameters->ViewSize = FVector4f(OutTexture->Desc.Extent.X, OutTexture->Desc.Extent.Y, BilateralParameter->BlurSize, 0);
	PassParameters->View = View.ViewUniformBuffer;
	PassParameters->Sampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->OutTexture = GraphBuilder.CreateUAV(OutTexture);
	TShaderMapRef<FGaussianBlurCS> ComputeShader(ViewInfo.ShaderMap);
	FComputeShaderUtils::AddPass(GraphBuilder,
								 RDG_EVENT_NAME("GaussianBlur %dx%d (CS)", OutTexture->Desc.Extent.X, OutTexture->Desc.Extent.Y),
								 ComputeShader,
								 PassParameters,
								 FComputeShaderUtils::GetGroupCount(OutTexture->Desc.Extent, FIntPoint(32, 32)));

}

void URenderAdapterBase::AddTextureCombinePass(FRDGBuilder& GraphBuilder, const FViewInfo& View,  const FPostProcessingInputs& Inputs,FRDGTextureRef InTexture, FRDGTextureRef& OutTexture,FTextureBlendDesc* InTextureBlendDesc)
{

	if(!OutTexture->Desc.IsValid())
	{
		return;
	}
	auto ShaderMap = GetGlobalShaderMap(View.FeatureLevel);
	FRDGTextureRef CopyOutTexture =GraphBuilder.CreateTexture(OutTexture->Desc,TEXT("CopyOutTexture"));
	if(!bTranslucentOnly)
	{

		FTextureBlendCS::FPermutationDomain PermutationDomain;
		PermutationDomain.Set<FTextureBlendCS::FBlendMethod>(InTextureBlendDesc->BlendMethod == EBlendMethod::MAX?EBlendMethod::Addition: InTextureBlendDesc->BlendMethod);
		//AddCopyTexturePass(GraphBuilder,OutTexture,CopyOutTexture);
		TShaderMapRef<FTextureBlendCS> ComputeShader(ShaderMap,PermutationDomain);
		FTextureBlendCS::FParameters* Parameters = GraphBuilder.AllocParameters<FTextureBlendCS::FParameters>();
		Parameters->View = View.ViewUniformBuffer;
		Parameters->SceneTextures =  Inputs.SceneTextures;
		Parameters->ClampSampler = TStaticSamplerState<>::GetRHI();
		Parameters->InTexture = InTexture;
		Parameters->SceneTexture = OutTexture;
		Parameters->OutTexture = GraphBuilder.CreateUAV(CopyOutTexture);
		Parameters->Weight = InTextureBlendDesc->Weight;
		FComputeShaderUtils::AddPass(GraphBuilder,
										 RDG_EVENT_NAME("SceneColorCombine CS(%s -> %s)", InTexture->Name, CopyOutTexture->Name),
										 ERDGPassFlags::Compute,
										 ComputeShader,
										 Parameters,
										 FComputeShaderUtils::GetGroupCount(FIntVector(OutTexture->Desc.Extent.X,OutTexture->Desc.Extent.Y, 1), 8));
		OutTexture = MoveTemp(CopyOutTexture);
	}
	else
	{


		FTextureBlendPS::FPermutationDomain PermutationDomain;
		PermutationDomain.Set<FTextureBlendPS::FBlendMethod>(InTextureBlendDesc->BlendMethod == EBlendMethod::MAX?EBlendMethod::Addition: InTextureBlendDesc->BlendMethod);
		TShaderMapRef<FTextureBlendPS> PixelShader(ShaderMap,PermutationDomain);
		FTextureBlendPS::FParameters* Parameters = GraphBuilder.AllocParameters<FTextureBlendPS::FParameters>();
		Parameters->View = View.ViewUniformBuffer;
		Parameters->SceneTextures =  Inputs.SceneTextures;
		Parameters->ClampSampler = TStaticSamplerState<>::GetRHI();
		Parameters->InTexture = InTexture;
		Parameters->SceneTexture = OutTexture;
		Parameters->Weight = InTextureBlendDesc->Weight;
		Parameters->RenderTargets[0] = FRenderTargetBinding(CopyOutTexture, ERenderTargetLoadAction::ELoad);
		FPixelShaderUtils::AddFullscreenPass(GraphBuilder,ShaderMap,
			RDG_EVENT_NAME("SceneColorCombine PS(%s -> %s)", InTexture->Name, CopyOutTexture->Name),
		PixelShader,
		Parameters,
		FIntRect(0, 0, OutTexture->Desc.Extent.X, OutTexture->Desc.Extent.Y));
		AddDrawTexturePass(GraphBuilder,View,CopyOutTexture,OutTexture);
		//OutTexture = MoveTemp(CopyOutTexture);
	}

	//AddCopyTexturePass(GraphBuilder,OutTexture,SceneColor);
}

void URenderAdapterBase::AddDownsamplePass(FRDGBuilder& GraphBuilder, const FViewInfo& View, FScreenPassTexture Input, FScreenPassTexture Output, FDownSampleParameter DownSampleParameter)
{

	if (DownSampleParameter.bUseComputeShader)
	{
		FDownsampleCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FDownsampleCS::FParameters>();
		PassParameters->Common = GetDownsampleParameters(View, Output, Input, DownSampleParameter.Quality);
		PassParameters->BloomThreshold = DownSampleParameter.BloomThreshold;
		PassParameters->UVScale = DownSampleParameter.UVScale;
		PassParameters->DispatchThreadIdToInputUV = ((FScreenTransform::Identity + 0.5f) / Output.ViewRect.Size()) * FScreenTransform::ChangeTextureBasisFromTo(FScreenPassTextureViewport(Input), FScreenTransform::ETextureBasis::ViewportUV, FScreenTransform::ETextureBasis::TextureUV);
		PassParameters->OutComputeTexture = GraphBuilder.CreateUAV(Output.Texture);
		PassParameters->Index = DownSampleParameter.Index;
		FDownsampleCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FDownsampleCS::FQuality>(DownSampleParameter.Quality);
		PermutationVector.Set<FDownsampleCS::FNeedClampLuminance>(DownSampleParameter.bNeedClampLuminance);

		TShaderMapRef<FDownsampleCS> ComputeShader(View.ShaderMap, PermutationVector);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("Downsample(%s Quality=%s CS) %dx%d -> %dx%d",
				Output.Texture->Name,
				DownSampleParameter.Quality == EDownsampleQuality::High ? TEXT("High") : TEXT("Bilinear"),
				Input.ViewRect.Width(), Input.ViewRect.Height(),
				Output.ViewRect.Width(), Output.ViewRect.Height()),
			ERDGPassFlags::Compute,
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(Output.ViewRect.Size(), FIntPoint(GDownsampleTileSizeX, GDownsampleTileSizeY)));	}
	else
	{
		FDownsamplePS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FDownsamplePS::FQuality>(DownSampleParameter.Quality);
		PermutationVector.Set<FDownsamplePS::FNeedClampLuminance>(DownSampleParameter.bNeedClampLuminance);
		FDownsamplePS::FParameters* PassParameters = GraphBuilder.AllocParameters<FDownsamplePS::FParameters>();
		PassParameters->Common = GetDownsampleParameters(View, Output, Input, DownSampleParameter.Quality);
		PassParameters->BloomThreshold = DownSampleParameter.BloomThreshold;
		PassParameters->UVScale = DownSampleParameter.UVScale;
		PassParameters->Index = DownSampleParameter.Index;
		PassParameters->RenderTargets[0] = FRenderTargetBinding(Output.Texture,ERenderTargetLoadAction::ELoad);

		TShaderMapRef<FDownsamplePS> PixelShader(View.ShaderMap, PermutationVector);
		FPixelShaderUtils::AddFullscreenPass(
			GraphBuilder,
			View.ShaderMap,
			RDG_EVENT_NAME("Downsample(%s Quality=%s PS) %dx%d -> %dx%d",
				Output.Texture->Name,
				DownSampleParameter.Quality == EDownsampleQuality::High ? TEXT("High") : TEXT("Bilinear"),
				Input.ViewRect.Width(), Input.ViewRect.Height(),
				Output.ViewRect.Width(), Output.ViewRect.Height()),
			PixelShader,
			PassParameters,
			Output.ViewRect);
	}

}

void URenderAdapterBase::AddTextureBlurPass(FRDGBuilder& GraphBuilder, const FViewInfo& View, FRDGTextureRef InTexture, FRDGTextureRef& OutTexture, FBlurParameter BlurParameter)
{

	switch (BlurParameter.BlurMethod)
	{
	case EBlurMethod::BilateralFilter:
		DrawBilateralFilter(GraphBuilder,View,InTexture,OutTexture,&BlurParameter);
		break;
	case EBlurMethod::DualKawase:
		DrawDualKawaseBlur(GraphBuilder,View,InTexture,OutTexture,&BlurParameter);
		break;
	case EBlurMethod::Gaussian:
		DrawGaussianBlur(GraphBuilder,View,InTexture,OutTexture,&BlurParameter);
		break;
	}
}

