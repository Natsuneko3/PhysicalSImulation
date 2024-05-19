#include "PhysicalSolver.h"
#include "PhysicalSimulationSceneProxy.h"
#include "DataDrivenShaderPlatformInfo.h"


/*
class FTextureBlurCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTextureBlurCS);
	SHADER_USE_PARAMETER_STRUCT(FTextureBlurCS, FGlobalShader);


	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutComputeTexture)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true; //IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), 8);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), 8);
	}
};

IMPLEMENT_GLOBAL_SHADER(FTextureBlurCS, "/Engine/Private/PostProcessDownsample.usf", "MainCS", SF_Compute);
*/

void FPSVertexBuffer::SetDynamicUsage(bool bInDynamicUsage)
{
	bDynamicUsage = bInDynamicUsage;
}

void FPSVertexBuffer::CreateBuffers(FRHICommandListBase& RHICmdList, int32 InNumVertices)
{
	//Make sure we don't have dangling buffers
	if (NumAllocatedVertices > 0)
	{
		ReleaseBuffers();
	}

	//The buffer will always be a shader resource, but they can be static/dynamic depending of the usage
	const EBufferUsageFlags Usage = BUF_ShaderResource | (bDynamicUsage ? BUF_Dynamic : BUF_Static);
	NumAllocatedVertices = InNumVertices;

	uint32 PositionSize = NumAllocatedVertices * sizeof(FVector3f);
	// create vertex buffer
	{
		FRHIResourceCreateInfo CreateInfo(TEXT("PhysicalSpritePositionBuffer"));
		PositionBuffer.VertexBufferRHI = RHICmdList.CreateVertexBuffer(PositionSize, Usage, CreateInfo);
		if (RHISupportsManualVertexFetch(GMaxRHIShaderPlatform))
		{
			PositionBufferSRV = RHICmdList.CreateShaderResourceView(PositionBuffer.VertexBufferRHI, sizeof(float), PF_R32_FLOAT);
		}
	}

	uint32 TangentSize = NumAllocatedVertices * 2 * sizeof(FPackedNormal);
	// create vertex buffer
	{
		FRHIResourceCreateInfo CreateInfo(TEXT("PhysicalSpriteTangentBuffer"));
		TangentBuffer.VertexBufferRHI = RHICmdList.CreateVertexBuffer(TangentSize, Usage, CreateInfo);
		if (RHISupportsManualVertexFetch(GMaxRHIShaderPlatform))
		{
			TangentBufferSRV = RHICmdList.CreateShaderResourceView(TangentBuffer.VertexBufferRHI, sizeof(FPackedNormal), PF_R8G8B8A8_SNORM);
		}
	}

	uint32 TexCoordSize = NumAllocatedVertices * sizeof(FVector2f);
	// create vertex buffer
	{
		FRHIResourceCreateInfo CreateInfo(TEXT("PhysicalSpriteTexCoordBuffer"));
		TexCoordBuffer.VertexBufferRHI = RHICmdList.CreateVertexBuffer(TexCoordSize, Usage, CreateInfo);
		if (RHISupportsManualVertexFetch(GMaxRHIShaderPlatform))
		{
			TexCoordBufferSRV = RHICmdList.CreateShaderResourceView(TexCoordBuffer.VertexBufferRHI, sizeof(FVector2f), PF_G32R32F);
		}
	}

	uint32 ColorSize = NumAllocatedVertices * sizeof(FColor);
	// create vertex buffer
	{
		FRHIResourceCreateInfo CreateInfo(TEXT("PhysicalSpriteColorBuffer"));
		ColorBuffer.VertexBufferRHI = RHICmdList.CreateVertexBuffer(ColorSize, Usage, CreateInfo);
		if (RHISupportsManualVertexFetch(GMaxRHIShaderPlatform))
		{
			ColorBufferSRV = RHICmdList.CreateShaderResourceView(ColorBuffer.VertexBufferRHI, sizeof(FColor), PF_R8G8B8A8);
		}
	}

	//Create Index Buffer
	{
		FRHIResourceCreateInfo CreateInfo(TEXT("PhysicalSpriteIndexBuffer"));
		IndexBuffer.IndexBufferRHI = RHICmdList.CreateIndexBuffer(sizeof(uint32), Vertices.Num() * sizeof(uint32), Usage, CreateInfo);
	}
}

void FPSVertexBuffer::ReleaseBuffers()
{
	PositionBuffer.ReleaseRHI();
	TangentBuffer.ReleaseRHI();
	TexCoordBuffer.ReleaseRHI();
	ColorBuffer.ReleaseRHI();
	IndexBuffer.ReleaseRHI();

	TangentBufferSRV.SafeRelease();
	TexCoordBufferSRV.SafeRelease();
	ColorBufferSRV.SafeRelease();
	PositionBufferSRV.SafeRelease();

	NumAllocatedVertices = 0;
}

void FPSVertexBuffer::CommitVertexData(FRHICommandListBase& RHICmdList)
{
	if (Vertices.Num())
	{
		//Check if we have to accommodate the buffer size
		if (NumAllocatedVertices != Vertices.Num())
		{
			CreateBuffers(RHICmdList, Vertices.Num());
		}

		//Lock vertices
		FVector3f* PositionBufferData = nullptr;
		uint32 PositionSize = Vertices.Num() * sizeof(FVector3f);
		{
			void* Data = RHICmdList.LockBuffer(PositionBuffer.VertexBufferRHI, 0, PositionSize, RLM_WriteOnly);
			PositionBufferData = static_cast<FVector3f*>(Data);
		}

		FPackedNormal* TangentBufferData = nullptr;
		uint32 TangentSize = Vertices.Num() * 2 * sizeof(FPackedNormal);
		{
			void* Data = RHICmdList.LockBuffer(TangentBuffer.VertexBufferRHI, 0, TangentSize, RLM_WriteOnly);
			TangentBufferData = static_cast<FPackedNormal*>(Data);
		}

		FVector2f* TexCoordBufferData = nullptr;
		uint32 TexCoordSize = Vertices.Num() * sizeof(FVector2f);
		{
			void* Data = RHICmdList.LockBuffer(TexCoordBuffer.VertexBufferRHI, 0, TexCoordSize, RLM_WriteOnly);
			TexCoordBufferData = static_cast<FVector2f*>(Data);
		}

		FColor* ColorBufferData = nullptr;
		uint32 ColorSize = Vertices.Num() * sizeof(FColor);
		{
			void* Data = RHICmdList.LockBuffer(ColorBuffer.VertexBufferRHI, 0, ColorSize, RLM_WriteOnly);
			ColorBufferData = static_cast<FColor*>(Data);
		}

		uint32* IndexBufferData = nullptr;
		uint32 IndexSize = Vertices.Num() * sizeof(uint32);
		{
			void* Data = RHICmdList.LockBuffer(IndexBuffer.IndexBufferRHI, 0, IndexSize, RLM_WriteOnly);
			IndexBufferData = static_cast<uint32*>(Data);
		}

		//Fill verts
		for (int32 i = 0; i < Vertices.Num(); i++)
		{
			PositionBufferData[i] = (FVector3f)Vertices[i].Position;
			TangentBufferData[2 * i + 0] = Vertices[i].TangentX;
			TangentBufferData[2 * i + 1] = Vertices[i].TangentZ;
			ColorBufferData[i] = Vertices[i].Color;
			TexCoordBufferData[i] = Vertices[i].TextureCoordinate[0];
			IndexBufferData[i] = i;
		}

		// Unlock the buffer.
		RHICmdList.UnlockBuffer(PositionBuffer.VertexBufferRHI);
		RHICmdList.UnlockBuffer(TangentBuffer.VertexBufferRHI);
		RHICmdList.UnlockBuffer(TexCoordBuffer.VertexBufferRHI);
		RHICmdList.UnlockBuffer(ColorBuffer.VertexBufferRHI);
		RHICmdList.UnlockBuffer(IndexBuffer.IndexBufferRHI);

		//We clear the vertex data, as it isn't needed anymore
		Vertices.Empty();
	}
}

void FPSVertexBuffer::InitRHI(FRHICommandListBase& RHICmdList)
{
	//Automatically try to create the data and use it
	CommitVertexData(RHICmdList);
}

void FPSVertexBuffer::ReleaseRHI()
{
	PositionBuffer.ReleaseRHI();
	TangentBuffer.ReleaseRHI();
	TexCoordBuffer.ReleaseRHI();
	ColorBuffer.ReleaseRHI();
	IndexBuffer.ReleaseRHI();

	TangentBufferSRV.SafeRelease();
	TexCoordBufferSRV.SafeRelease();
	ColorBufferSRV.SafeRelease();
	PositionBufferSRV.SafeRelease();
}

void FPSVertexBuffer::InitResource(FRHICommandListBase& RHICmdList)
{
	FRenderResource::InitResource(RHICmdList);
	PositionBuffer.InitResource(RHICmdList);
	TangentBuffer.InitResource(RHICmdList);
	TexCoordBuffer.InitResource(RHICmdList);
	ColorBuffer.InitResource(RHICmdList);
	IndexBuffer.InitResource(RHICmdList);
}

void FPSVertexBuffer::ReleaseResource()
{
	FRenderResource::ReleaseResource();
	PositionBuffer.ReleaseResource();
	TangentBuffer.ReleaseResource();
	TexCoordBuffer.ReleaseResource();
	ColorBuffer.ReleaseResource();
	IndexBuffer.ReleaseResource();
}

FPSSpriteVertexFactory::FPSSpriteVertexFactory(ERHIFeatureLevel::Type FeatureLevel)
	: FLocalVertexFactory(FeatureLevel, "FPhysicalSimulationSpriteVertexFactory")
{
}


FPSVertexFactory::FPSVertexFactory(ERHIFeatureLevel::Type FeatureLevel): FLocalVertexFactory(FeatureLevel, "FPhysicalVertexFactory")
{
}

void FPSVertexFactory::Init(const FPSVertexBuffer* InVertexBuffer)
{
	if (IsInRenderingThread())
	{
		FLocalVertexFactory::FDataType VertexData;
		VertexData.NumTexCoords = 1;

		//SRV setup
		VertexData.LightMapCoordinateIndex = 0;
		VertexData.TangentsSRV = InVertexBuffer->TangentBufferSRV;
		VertexData.TextureCoordinatesSRV = InVertexBuffer->TexCoordBufferSRV;
		VertexData.ColorComponentsSRV = InVertexBuffer->ColorBufferSRV;
		VertexData.PositionComponentSRV = InVertexBuffer->PositionBufferSRV;

		// Vertex Streams
		VertexData.PositionComponent = FVertexStreamComponent(&InVertexBuffer->PositionBuffer, 0, sizeof(FVector3f), VET_Float3, EVertexStreamUsage::Default);
		VertexData.TangentBasisComponents[0] = FVertexStreamComponent(&InVertexBuffer->TangentBuffer, 0, 2 * sizeof(FPackedNormal), VET_PackedNormal, EVertexStreamUsage::ManualFetch);
		VertexData.TangentBasisComponents[1] = FVertexStreamComponent(&InVertexBuffer->TangentBuffer, sizeof(FPackedNormal), 2 * sizeof(FPackedNormal), VET_PackedNormal, EVertexStreamUsage::ManualFetch);
		VertexData.ColorComponent = FVertexStreamComponent(&InVertexBuffer->ColorBuffer, 0, sizeof(FColor), VET_Color, EVertexStreamUsage::ManualFetch);
		VertexData.TextureCoordinates.Add(FVertexStreamComponent(&InVertexBuffer->TexCoordBuffer, 0, sizeof(FVector2f), VET_Float2, EVertexStreamUsage::ManualFetch));

		SetData(VertexData);
		VertexBuffer = InVertexBuffer;

		InitResource();
	}
	else
	{
		FPSVertexFactory* ThisFactory = this;
		ENQUEUE_RENDER_COMMAND(SpriteVertexFactoryInit)(
			[ThisFactory, InVertexBuffer](FRHICommandListImmediate& RHICmdList)
			{
				ThisFactory->Init(InVertexBuffer);
			});
	}
}

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


	/*Vertices[0].Position = FVector4f(1, 1, 0, 1);
	Vertices[0].UV = FVector2f(1, 1);

	Vertices[1].Position = FVector4f(0, 1, 0, 1);
	Vertices[1].UV = FVector2f(0, 1);

	Vertices[2].Position = FVector4f(1, 0, 0, 1);
	Vertices[2].UV = FVector2f(1, 0);

	Vertices[3].Position = FVector4f(0, 0, 0, 1);
	Vertices[3].UV = FVector2f(0, 0);*/

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
		// bottom face
		2, 3, 1, 2, 1, 0
		/*0, 1, 2,
		1, 3, 2,*/
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

void FPhysicalSolverBase::AddTextureBlurPass(FRDGBuilder& GraphBuilder, FRDGTextureRef InTexture, FRDGTextureRef& OutTexture)
{
}
