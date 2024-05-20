#include "PhysicalSolver.h"
#if ENGINE_MINOR_VERSION >1
#include "DataDrivenShaderPlatformInfo.h"
#endif
#include "PhysicalSimulationSceneProxy.h"


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

		SHADER_PARAMETER(FVector2f, DownSampleScale)
		SHADER_PARAMETER(FVector2f, BlurDir)
		SHADER_PARAMETER(FVector2f, TextureSize)
		SHADER_PARAMETER(float, Coefficient)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutTexture)

	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FBilateralFilterCS, "/Plugin/PhysicalSimulation/TextureBlur/TextureBlur.usf", "MainCS", SF_Compute);

///////////////////FPhysicalSolverBase/////////////////////
void FPhysicalSolverBase::SetupSolverBaseParameters(FSolverBaseParameter& Parameter, FSceneView& InView, FPhysicalSimulationSceneProxy* InSceneProxy)
{
	Parameter.dt = InSceneProxy->World->GetDeltaSeconds();
	Parameter.dx = *InSceneProxy->Dx;
	Parameter.Time = Frame;
	Parameter.View = InView.ViewUniformBuffer;
	Parameter.GridSize = FVector3f(InSceneProxy->GridSize.X, InSceneProxy->GridSize.Y, InSceneProxy->GridSize.Z);
	Parameter.WarpSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
}

void FPhysicalSolverBase::InitialPlaneMesh()
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

	FRHIResourceCreateInfo CreateInfoVB(TEXT("PSPlaneMeshVertexBuffer"), &Vertices);
	VertexBufferRHI = RHICreateVertexBuffer(Vertices.GetResourceDataSize(), BUF_Static, CreateInfoVB);
	TResourceArray<uint16, INDEXBUFFER_ALIGNMENT> IndexBuffer;
	const uint32 NumIndices = UE_ARRAY_COUNT(SpriteIndices);
	IndexBuffer.AddUninitialized(NumIndices);
	FMemory::Memcpy(IndexBuffer.GetData(), SpriteIndices, NumIndices * sizeof(uint16));

	FRHIResourceCreateInfo CreateInfoIB(TEXT("PSPlaneMeshIndexBuffer"), &IndexBuffer);
	IndexBufferRHI = RHICreateIndexBuffer(sizeof(uint16), IndexBuffer.GetResourceDataSize(), BUF_Static, CreateInfoIB);
	NumPrimitives = 2;
}

void FPhysicalSolverBase::InitialCubeMesh()
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
	VertexBufferRHI = RHICreateVertexBuffer(Vertices.GetResourceDataSize(), BUF_Static, CreateInfoVB);

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
	IndexBufferRHI = RHICreateIndexBuffer(sizeof(uint16), IndexBuffer.GetResourceDataSize(), BUF_Static, CreateInfoIB);
	NumPrimitives = 12;
}

void FPhysicalSolverBase::AddTextureBlurPass(FRDGBuilder& GraphBuilder, const FViewInfo& View, FRDGTextureRef InTexture, FRDGTextureRef& OutTexture, float BlurSize)
{
	FIntPoint ViewSize = OutTexture->Desc.Extent;

	FString IsHorizon;
	FVector2f BlurDir;

	IsHorizon = "Horizon";
	BlurDir = FVector2f(1.0f, 0.0f);

	FRDGTextureRef TempTexture = GraphBuilder.CreateTexture(OutTexture->Desc,TEXT("BilateralFilterTempTexture"));
	FVector2f DownSampleScale = FVector2f(InTexture->Desc.Extent.X / OutTexture->Desc.Extent.X, InTexture->Desc.Extent.Y / OutTexture->Desc.Extent.Y);

	FBilateralFilterCS::FParameters* HorizonPassParameters = GraphBuilder.AllocParameters<FBilateralFilterCS::FParameters>();
	HorizonPassParameters->DownSampleScale = DownSampleScale;
	HorizonPassParameters->InputTexture = InTexture;
	HorizonPassParameters->InputTextureSampler = TStaticSamplerState<SF_Point>::GetRHI();
	HorizonPassParameters->Coefficient = BlurSize;
	HorizonPassParameters->BlurDir = BlurDir;
	HorizonPassParameters->OutTexture = GraphBuilder.CreateUAV(TempTexture);
	TShaderMapRef<FBilateralFilterCS> ComputeShader(View.ShaderMap);

	FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(ViewSize, FIntPoint(8, 8));
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("BilateralFilter %s %dx%d(CS)",
		               *IsHorizon, ViewSize.X, ViewSize.Y),
		ComputeShader, HorizonPassParameters,
		GroupCount);

	IsHorizon = "Vertical";
	BlurDir = FVector2f(0.0f, 1.0f);
	FBilateralFilterCS::FParameters* VerticalPassParameters = GraphBuilder.AllocParameters<FBilateralFilterCS::FParameters>();
	VerticalPassParameters->DownSampleScale = DownSampleScale;
	VerticalPassParameters->InputTexture = TempTexture;
	VerticalPassParameters->InputTextureSampler = TStaticSamplerState<SF_Point>::GetRHI();
	VerticalPassParameters->Coefficient = BlurSize;
	VerticalPassParameters->BlurDir = BlurDir;
	VerticalPassParameters->OutTexture = GraphBuilder.CreateUAV(OutTexture);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("BilateralFilter %s %dx%d(CS)",
		               *IsHorizon, ViewSize.X, ViewSize.Y),
		ComputeShader, VerticalPassParameters,
		GroupCount);
	//AddCopyTexturePass(GraphBuilder,PassParameters->OutTexture->GetRHI(),SceneColor)
}
