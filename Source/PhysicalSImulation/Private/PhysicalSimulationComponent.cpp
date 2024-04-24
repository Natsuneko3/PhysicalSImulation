// Copyright Natsu Neko, Inc. All Rights Reserved.

#include "PhysicalSimulationComponent.h"
#include "Physical2DFluidSolver.h"
#include "PhysicalSimulationSceneProxy.h"
#include "PhysicalSimulationSystem.h"

#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "SceneViewExtension.h"
#include "Engine/TextureRenderTargetVolume.h"


// Sets default values
UPhysicalSimulationComponent::UPhysicalSimulationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	bTickInEditor = true;
	if (!GetStaticMesh())
	{
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane"));
		SetStaticMesh(Mesh);
	}
	Initial();
}

UPhysicalSimulationComponent::~UPhysicalSimulationComponent()
{
	PhysicalSolverViewExtension = nullptr;
}

void UPhysicalSimulationComponent::Initial()
{
	/*if (Material)
	{
		UE_LOG(LogSimulation, Log, TEXT("Physical simulation initial"))
		CreateSolverTextures();
		UpdateSolverContext();
		/*UPhysicalSimulationSystem* SubSystem = GetWorld()->GetSubsystem<UPhysicalSimulationSystem>();
		if (SubSystem)
		{
			PhysicalSolverViewExtension = SubSystem->FindOrCreateViewExtension(this);
		}#1#
	}*/
}

void UPhysicalSimulationComponent::BeginDestroy()
{
	Super::BeginDestroy();
	/*if (GetWorld())
	{
		UPhysicalSimulationSystem* SubSystem = GetWorld()->GetSubsystem<UPhysicalSimulationSystem>();
		if (SubSystem)
		{
			SubSystem->RemoveViewExtension(GetUniqueID());
		}
	}*/
}

void UPhysicalSimulationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UPhysicalSimulationSystem* SubSystem = GetWorld()->GetSubsystem<UPhysicalSimulationSystem>();
	if (SubSystem)
	{
		for (int i = 0; i < SubSystem->OutParticle.Num(); i++)
		{
			DrawDebugPoint(GetWorld(), SubSystem->OutParticle[i], 2, FColor::Green);
		}
	}
}

void UPhysicalSimulationComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);


	Initial();
}

FPrimitiveSceneProxy* UPhysicalSimulationComponent::CreateSceneProxy()
{
	if (Material)
	{
		return new FPhysicalSimulationSceneProxy(this);
	}
	return nullptr;
}

void UPhysicalSimulationComponent::SetTextureToMaterial()
{
	FPhysicalSimulationSceneProxy* PSSceneProxy = static_cast<FPhysicalSimulationSceneProxy*>(SceneProxy);
	UMaterialInstanceDynamic* MID = CreateDynamicMaterialInstance(0, Material.Get());
	int RTNum = 0;
	for (int i = 0; i < PSSceneProxy->OutputTextures.Num(); i++)
	{
		if (PSSceneProxy->OutputTextures[i])
		{
			RTNum++;
			FString RTName = "RT" + RTNum;
			MID->SetTextureParameterValue(*RTName, PSSceneProxy->OutputTextures[i]);
			SetMaterial(0, MID);
		}
	}
}

/*
void UPhysicalSimulationComponent::SetupSolverParameter()
{
	UWorld* World = GetWorld();

	FSolverBaseParameter SolverBase;

	if (World == nullptr)
	{
		SolverBase.dt = 0.002;
	}
	else
	{
		SolverBase.dt = GetWorld()->GetDeltaSeconds() * 2.0;
	}

	SolverBase.dx = 1.0; //1.0 / (float)GridSize.X;
	SolverBase.GridSize = FVector3f(GridSize);
	SolverBase.Time = 0;
	UTexture2D* NoiseTexture = LoadObject<UTexture2D>(nullptr,TEXT("/SpeedTreeImporter/SpeedTree9/game_wind_noise.game_wind_noise"));
	SolverBase.WarpSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();


	SolverParameter.FluidParameter.SolverBaseParameter = SolverBase;
	SolverParameter.FluidParameter.VorticityMult = VorticityMult;
	SolverParameter.FluidParameter.NoiseFrequency = NoiseFrequency;
	SolverParameter.FluidParameter.NoiseIntensity = NoiseIntensity;
	SolverParameter.FluidParameter.VelocityDissipate = VelocityDissipate;
	SolverParameter.FluidParameter.DensityDissipate = DensityDissipate;
	SolverParameter.FluidParameter.GravityScale = GravityScale;
	SolverParameter.FluidParameter.UseFFT = true;

	SolverParameter.LiuquidParameter.GravityScale = GravityScale;
	SolverParameter.LiuquidParameter.LifeTime = LifeTime;
	SolverParameter.LiuquidParameter.SolverBaseParameter = SolverBase;
}

void UPhysicalSimulationComponent::UpdateSolverContext()
{
	//PhysicalSolverContext.ActorName = "";
	SetupSolverParameter();
	if (GetOwner())
	{
		PhysicalSolverContext.WorldVelocity = FVector3f(GetOwner()->GetVelocity());
		PhysicalSolverContext.WorldPosition = FVector3f(GetOwner()->GetActorLocation());
		PhysicalSolverContext.ActorName = GetOwner()->GetName();
	}

	PhysicalSolverContext.World = GetWorld();
	PhysicalSolverContext.bSimulation = bSimulation;
	PhysicalSolverContext.SimulatorType = SimulatorType;
	PhysicalSolverContext.SolverParameter = &SolverParameter;
	PhysicalSolverContext.OutputTextures = OutputTextures;
	PhysicalSolverContext.SpawnRate = SpawnRate;
}

void UPhysicalSimulationComponent::CreateSolverTextures()
{
	UStaticMesh* Mesh;
	switch (SimulatorType)
	{
	case ESimulatorType::PlaneSmokeFluid:
		Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane"));
		SetStaticMesh(Mesh);
		Create2DRenderTarget();
		break;
	case ESimulatorType::CubeSmokeFluid:
		Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube"));
		SetStaticMesh(Mesh);
		Create3DRenderTarget();
		break;
	case ESimulatorType::Liquid:
		Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube"));
		SetStaticMesh(Mesh);
		Create3DRenderTarget();
		break;
	}
}

void UPhysicalSimulationComponent::Create3DRenderTarget()
{
	OutputTextures.Empty();
	UTextureRenderTargetVolume* VolumeRT = NewObject<UTextureRenderTargetVolume>();
	VolumeRT->InitAutoFormat(GridSize.X, GridSize.Y, GridSize.Z);
	VolumeRT->OverrideFormat = PF_R32_FLOAT;
	VolumeRT->ClearColor = FLinearColor::Black;
	VolumeRT->bCanCreateUAV = true;
	VolumeRT->UpdateResourceImmediate(true);

	OutputTextures.Add(VolumeRT);
	UMaterialInstanceDynamic* MID = CreateDynamicMaterialInstance(0, Material.Get());
	MID->SetTextureParameterValue(TEXT("RT"), OutputTextures[0]);
	SetMaterial(0, MID);
	//GetStaticMesh()->SetMaterial(0,MID);
}

void UPhysicalSimulationComponent::Create2DRenderTarget()
{
	/*for(UTextureRenderTarget* RT: OutputTextures)
	{
		RT->ReleaseResource();
	}#1#
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
	UMaterialInstanceDynamic* MID = CreateDynamicMaterialInstance(0, Material.Get());
	MID->SetTextureParameterValue(TEXT("RT"), OutputTextures[0]);
	MID->SetTextureParameterValue(TEXT("RT2"), OutputTextures[1]);
	SetMaterial(0, MID);
}*/
