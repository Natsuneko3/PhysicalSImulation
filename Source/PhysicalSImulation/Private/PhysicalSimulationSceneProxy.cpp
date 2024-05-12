// Fill out your copyright notice in the Description page of Project Settings.


#include "PhysicalSimulationSceneProxy.h"

#include "PhysicalSimulationComponent.h"
#include "PhysicalSimulationSystem.h"
#include "PhysicalSolver.h"
#include "PsychedelicSolver.h"
DECLARE_CYCLE_STAT(TEXT("GetDynamicMeshElements"), STAT_PS_GetDynamicMeshElements, STATGROUP_PS)


FPhysicalSimulationSceneProxy::FPhysicalSimulationSceneProxy(UPhysicalSimulationComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent), Component(InComponent)
{
	UPhysicalSimulationSystem* SubSystem = InComponent->GetWorld()->GetSubsystem<UPhysicalSimulationSystem>();
	int x = FMath::Max(InComponent->GridSize.X, 8);
	int y = FMath::Max(InComponent->GridSize.Y, 8);
	int z = FMath::Max(InComponent->GridSize.Z, 8);
	GridSize = FIntVector(x, y, z);
	bSimulation = Component->bSimulation;
	StaticMesh = InComponent->GetStaticMesh();
	Material = InComponent->Material.Get();
	FeatureLevel = InComponent->GetWorld()->FeatureLevel;

	World = Component->GetWorld();
	Dx = &Component->Dx;
	PlandFluidParameters = &Component->PlandFluidParameters;
	LiquidSolverParameter = &Component->LiquidSolverParameter;
	bFacingCamera = &Component->FacingCamera;
	ActorTransform = &Component->GetOwner()->GetActorTransform();

	if (SubSystem)
	{
		switch (Component->SimulatorType)
		{
		case ESimulatorType::PlaneSmokeFluid:
			PhysicalSolver = MakeShareable(new FPhysical2DFluidSolver(this));

			break;
		case ESimulatorType::Psychedelic:
			PhysicalSolver = MakeShareable(new FPsychedelicSolver(this));

			break;
		case ESimulatorType::Liquid:
			PhysicalSolver = MakeShareable(new FPhysicalLiquidSolver(this));

			break;
		}
		//PhysicalSolver = MakeShareable(new FPhysical2DFluidSolver(this));
		ViewExtension = SubSystem->PhysicalSolverViewExtension;
	}
	IsInitialized = true;
}

FPhysicalSimulationSceneProxy::~FPhysicalSimulationSceneProxy()
{
	ViewExtension.Reset();
	PhysicalSolver.Reset();
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
		ViewExtension->AddProxy(this, RHICmdList);
	}
}

void FPhysicalSimulationSceneProxy::DestroyRenderThreadResources()
{
	if (ViewExtension)
		ViewExtension->RemoveProxy(this);
}

// These setups associated volume mesh for built-in Unreal passes.
// Actual rendering is FPhysicalSimulationViewExtension::PreRenderView_RenderThread.
void FPhysicalSimulationSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{


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
