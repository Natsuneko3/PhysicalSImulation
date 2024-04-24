// Fill out your copyright notice in the Description page of Project Settings.


#include "PhysicalSimulationSceneProxy.h"
#include "PhysicalSimulationComponent.h"
#include "PhysicalSimulationSystem.h"
#include "Engine/TextureRenderTargetVolume.h"
#include "PhysicalSolver.h"
DECLARE_CYCLE_STAT(TEXT("GetDynamicMeshElements"), STAT_PS_GetDynamicMeshElements, STATGROUP_PS)
void FPhysicalSolverBase::SetupSolverBaseParameters(FSolverBaseParameter& Parameter,FSceneView& InView)
{
	Parameter.dt = SceneProxy->World->GetDeltaSeconds();
	Parameter.dx = SceneProxy->Dx;
	Parameter.Time = Frame;
	Parameter.View = InView.ViewUniformBuffer;
	Parameter.GridSize = FVector3f(SceneProxy->GridSize.X,SceneProxy->GridSize.Y,1);
	Parameter.WarpSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();

}
FPhysicalSimulationSceneProxy::FPhysicalSimulationSceneProxy(UPhysicalSimulationComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent), Component(InComponent)
{
	UPhysicalSimulationSystem* SubSystem = InComponent->GetWorld()->GetSubsystem<UPhysicalSimulationSystem>();
	if (SubSystem)
	{
		SubSystem->AddSceneProxyToViewExtension(this);
		switch (Component->SimulatorType)
		{
		case ESimulatorType::PlaneSmokeFluid:
			PhysicalSolver = MakeShareable(new FPhysical2DFluidSolver(this));
			Create2DRenderTarget();
			break;
		case ESimulatorType::CubeSmokeFluid:
			PhysicalSolver = MakeShareable(new FPhysical3DFluidSolver(this));
			Create3DRenderTarget();
			break;
		case ESimulatorType::Liquid:
			PhysicalSolver = MakeShareable(new FPhysicalLiquidSolver(this)); //new FPhysicalLiquidSolver;
			Create3DRenderTarget();
			break;
		}
	}
	bSimulation = Component->bSimulation;
	StaticMesh = InComponent->GetStaticMesh();
	Material = InComponent->Material.Get();
	FeatureLevel = InComponent->GetWorld()->FeatureLevel;
	int x = FMath::Max(InComponent->GridSize.X - 1, 8);
	int y = FMath::Max(InComponent->GridSize.Y - 1, 8);
	int z = FMath::Max(InComponent->GridSize.Z - 1, 8);
	GridSize = FIntVector(x, y, z);
	World = Component->GetWorld();
	Dx = Component->Dx;
	PlandFluidParameters = &Component->PlandFluidParameters;
	LiquidSolverParameter = &Component->LiquidSolverParameter;
}

FPhysicalSimulationSceneProxy::~FPhysicalSimulationSceneProxy()
{
	FPrimitiveSceneProxy::DestroyRenderThreadResources();
	if (ViewExtension)
	{
		ViewExtension->RemoveProxy(this);
	}
}

SIZE_T FPhysicalSimulationSceneProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

void FPhysicalSimulationSceneProxy::CreateRenderThreadResources(FRHICommandListBase& RHICmdList)
{
	FPrimitiveSceneProxy::CreateRenderThreadResources(RHICmdList);
	if (ViewExtension)
	{
		ViewExtension->AddProxy(this);
		ViewExtension->Initial(RHICmdList);
	}
}

void FPhysicalSimulationSceneProxy::DestroyRenderThreadResources()
{

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
