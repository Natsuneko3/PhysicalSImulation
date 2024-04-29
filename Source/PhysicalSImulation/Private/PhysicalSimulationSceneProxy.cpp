// Fill out your copyright notice in the Description page of Project Settings.


#include "PhysicalSimulationSceneProxy.h"

#include "DataDrivenShaderPlatformInfo.h"
#include "DynamicMeshBuilder.h"
#include "PhysicalSimulationComponent.h"
#include "PhysicalSimulationSystem.h"
#include "Engine/TextureRenderTargetVolume.h"
#include "PhysicalSolver.h"
DECLARE_CYCLE_STAT(TEXT("GetDynamicMeshElements"), STAT_PS_GetDynamicMeshElements, STATGROUP_PS)

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
void FPhysicalSolverBase::SetupSolverBaseParameters(FSolverBaseParameter& Parameter,FSceneView& InView)
{
	Parameter.dt = SceneProxy->World->GetDeltaSeconds();
	Parameter.dx = SceneProxy->Dx;
	Parameter.Time = Frame;
	Parameter.View = InView.ViewUniformBuffer;
	Parameter.GridSize = FVector3f(SceneProxy->GridSize.X,SceneProxy->GridSize.Y,1);
	Parameter.WarpSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();

}

void FPhysicalSolverBase::InitialPlaneMesh(FRHICommandListImmediate& RHICmdList)
{
	TArray<FPSSpriteVertex> Vertices;
	Vertices.Add(FDynamicMeshVertex(FVector3f(0.f)));
	Vertices.Add(FDynamicMeshVertex(FVector3f(0.f,1.f,0.f)));
	Vertices.Add(FDynamicMeshVertex(FVector3f(1.f,0.f,0.f)));
	Vertices.Add(FDynamicMeshVertex(FVector3f(1.f,1.f,0.f)));

	PSVertexBuffer = MakeUnique<FPSVertexBuffer>();
	PSVertexBuffer->Vertices = Vertices;
	//Inner will do the InitRHI()
	PSVertexBuffer->InitResource(RHICmdList);

	PSVertexFactory = MakeUnique<FPSVertexFactory>(SceneProxy->FeatureLevel);
	PSVertexFactory->Init(PSVertexBuffer.Get());
}

FPhysicalSimulationSceneProxy::FPhysicalSimulationSceneProxy(UPhysicalSimulationComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent), Component(InComponent)
{
	UPhysicalSimulationSystem* SubSystem = InComponent->GetWorld()->GetSubsystem<UPhysicalSimulationSystem>();
	int x = FMath::Max(InComponent->GridSize.X - 1, 8);
	int y = FMath::Max(InComponent->GridSize.Y - 1, 8);
	int z = FMath::Max(InComponent->GridSize.Z - 1, 8);
	GridSize = FIntVector(x, y, z);
	//OutputTextures.Add(InComponent->TestTexture);
	bSimulation = Component->bSimulation;
	StaticMesh = InComponent->GetStaticMesh();
	Material = InComponent->Material.Get();
	FeatureLevel = InComponent->GetWorld()->FeatureLevel;

	World = Component->GetWorld();
	Dx = Component->Dx;
	PlandFluidParameters = &Component->PlandFluidParameters;
	LiquidSolverParameter = &Component->LiquidSolverParameter;

	if (SubSystem)
	{
		//Create3DRenderTarget();
		switch (Component->SimulatorType)
		{
		case ESimulatorType::PlaneSmokeFluid:
			PhysicalSolver = MakeShareable(new FPhysical2DFluidSolver(this));
			//Create2DRenderTarget();
			break;
		case ESimulatorType::CubeSmokeFluid:
			PhysicalSolver = MakeShareable(new FPhysical3DFluidSolver(this));
			//Create3DRenderTarget();
			break;
		case ESimulatorType::Liquid:
			PhysicalSolver = MakeShareable(new FPhysicalLiquidSolver(this)); //new FPhysicalLiquidSolver;
			//Create3DRenderTarget();
			break;
		}
		ViewExtension = SubSystem->PhysicalSolverViewExtension;

	}


}

FPhysicalSimulationSceneProxy::~FPhysicalSimulationSceneProxy()
{
	ViewExtension.Reset();
}

SIZE_T FPhysicalSimulationSceneProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

void FPhysicalSimulationSceneProxy::CreateRenderThreadResources(FRHICommandListBase& RHICmdList)
{
	FPrimitiveSceneProxy::CreateRenderThreadResources(RHICmdList);
	if(ViewExtension)
	{
		ViewExtension->AddProxy(this,RHICmdList);
	}

}

void FPhysicalSimulationSceneProxy::DestroyRenderThreadResources()
{
	if(ViewExtension)
		ViewExtension->RemoveProxy(this);
}

// These setups associated volume mesh for built-in Unreal passes.
// Actual rendering is FPhysicalSimulationViewExtension::PreRenderView_RenderThread.
void FPhysicalSimulationSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	/*SCOPE_CYCLE_COUNTER(STAT_PS_GetDynamicMeshElements);
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const FSceneView* View = Views[ViewIndex];

		if (IsShown(View) && VisibilityMap & 1 << ViewIndex)
		{
			FMeshBatch& Mesh = Collector.AllocateMesh();
			Mesh.bWireframe = AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;

			ViewExtension->CreateMeshBatch(Mesh, this,Material->GetRenderProxy());

			Collector.AddMesh(ViewIndex, Mesh);
		}
	}*/
	ViewExtension->GetDynamicMeshElements(Views, ViewFamily, VisibilityMap, Collector, this);
}

FPrimitiveViewRelevance FPhysicalSimulationSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = IsShown(View);
	Result.bShadowRelevance = IsShadowCast(View) && ShouldRenderInMainPass();
	Result.bDynamicRelevance = true;
	Result.bStaticRelevance = false;
	Result.bRenderInMainPass = ShouldRenderInMainPass();
	Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
	Result.bRenderCustomDepth = ShouldRenderCustomDepth();
	return Result;
}

void FPhysicalSimulationSceneProxy::Create3DRenderTarget()
{
	OutputTextures.Empty();
	UTextureRenderTargetVolume* VolumeRT = NewObject<UTextureRenderTargetVolume>();
	VolumeRT->InitAutoFormat(GridSize.X, GridSize.Y, GridSize.Z);
	VolumeRT->OverrideFormat = PF_R32_FLOAT;
	VolumeRT->ClearColor = FLinearColor::Black;
	VolumeRT->bCanCreateUAV = true;
	VolumeRT->UpdateResourceImmediate(true);

	OutputTextures.Add(VolumeRT);
}

void FPhysicalSimulationSceneProxy::Create2DRenderTarget()
{
	OutputTextures.Empty();
	UTextureRenderTarget2D* NewRenderTarget2D = NewObject<UTextureRenderTarget2D>();
	check(NewRenderTarget2D);
	NewRenderTarget2D->RenderTargetFormat = RTF_RGBA16f;
	NewRenderTarget2D->ClearColor = FLinearColor::Red;
	NewRenderTarget2D->bAutoGenerateMips = true;
	NewRenderTarget2D->bCanCreateUAV = true;
	NewRenderTarget2D->InitAutoFormat(GridSize.X, GridSize.Y);
	NewRenderTarget2D->UpdateResourceImmediate(true);

	UTextureRenderTarget2D* NewPressureRenderTarget2D = NewObject<UTextureRenderTarget2D>();
	check(NewPressureRenderTarget2D);
	NewPressureRenderTarget2D->RenderTargetFormat = RTF_R16f;
	NewPressureRenderTarget2D->ClearColor = FLinearColor::Red;
	NewPressureRenderTarget2D->bAutoGenerateMips = true;
	NewPressureRenderTarget2D->bCanCreateUAV = true;
	NewPressureRenderTarget2D->InitAutoFormat(GridSize.X, GridSize.Y);
	NewPressureRenderTarget2D->UpdateResourceImmediate(true);

	OutputTextures.Add(NewRenderTarget2D);
	OutputTextures.Add(NewPressureRenderTarget2D);
}
