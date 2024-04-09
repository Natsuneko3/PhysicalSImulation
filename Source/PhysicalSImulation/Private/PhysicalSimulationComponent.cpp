// Copyright Natsu Neko, Inc. All Rights Reserved.

#include "PhysicalSimulationComponent.h"
#include "Physical2DFluidSolver.h"

#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "SceneViewExtension.h"
#include "Engine/TextureRenderTargetVolume.h"


// Sets default values
UPhysicalSimulationComponent::UPhysicalSimulationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	bTickInEditor = true;
	OutputTextures.Empty();
	//PhysicalSolverContext = MakeShared<FPhysicalSolverContext>();
	// CenterOnwerPosition = FVector3f(0);
	// LastOnwerPosition = FVector3f(0);
	/*if (!GetStaticMesh())
	{
		UStaticMesh* LoadMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
		SetStaticMesh(LoadMesh);
	}*/
}

UPhysicalSimulationComponent::~UPhysicalSimulationComponent()
{
	PhysicalSolverViewExtension->Release();
	PhysicalSolverViewExtension.Reset();
}


void UPhysicalSimulationComponent::Initial()
{
	if(!bInitialed && GetStaticMesh() && Material)
	{
		UE_LOG(LogPhysics,Log,TEXT("Physical simulation initial"))
		CreateSolverTextures();
		UpdateSolverContext();
		PhysicalSolverViewExtension = FSceneViewExtensions::NewExtension<FPhysicalSolverViewExtension>(&PhysicalSolverContext);
		//PhysicalSolverViewExtension->Initial(&PhysicalSolverContext);
		//PhysicalSolverViewExtension->Initial(&PhysicalSolverContext);


		bInitialed = true;
	}

}


// Called when the game starts or when spawned
void UPhysicalSimulationComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UPhysicalSimulationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	PhysicalSolverContext.bSimulation = false;
	Initial();
	UpdateSolverContext();
}

void UPhysicalSimulationComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UPhysicalSimulationComponent::SetupSolverParameter()
{
	UWorld* World = GetWorld();
	FSolverBaseParameter SolverBase;

	if(World == nullptr)
	{
		SolverBase.dt = 0.002;
	}else
	{
		SolverBase.dt =  GetWorld()->GetDeltaSeconds();
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
}

void UPhysicalSimulationComponent::UpdateSolverContext()
{
	SetupSolverParameter();
	if(GetOwner())
	{
		PhysicalSolverContext.WorldVelocity = FVector3f(GetOwner()->GetVelocity());
		PhysicalSolverContext.WorldPosition = FVector3f(GetOwner()->GetActorLocation());
	}

	PhysicalSolverContext.bSimulation = bSimulation;
	PhysicalSolverContext.SimulatorType = SimulatorType;
	PhysicalSolverContext.SolverParameter = &SolverParameter;
	PhysicalSolverContext.OutputTextures = OutputTextures;
}

void UPhysicalSimulationComponent::CreateSolverTextures()
{
	switch (SimulatorType)
	{
	case ESimulatorType::PlaneSmokeFluid:
		Create2DRenderTarget();
		break;
	case ESimulatorType::CubeSmokeFluid:
		Create3DRenderTarget();
		break;
	case ESimulatorType::Liquid:
		Create3DRenderTarget();
		break;
	}
}

void UPhysicalSimulationComponent::Create3DRenderTarget()
{
	OutputTextures.Empty();
	UTextureRenderTargetVolume* VolumeRT = NewObject<UTextureRenderTargetVolume>();
	VolumeRT->InitAutoFormat(GridSize.X, GridSize.Y, GridSize.Z);
	VolumeRT->OverrideFormat = PF_R16F;
	VolumeRT->ClearColor = FLinearColor::Black;
	VolumeRT->bCanCreateUAV = true;
	VolumeRT->UpdateResourceImmediate(true);

	OutputTextures.Add(VolumeRT);
	UMaterialInstanceDynamic* MID =  CreateDynamicMaterialInstance(0,Material.Get());
	MID->SetTextureParameterValue(TEXT("RT"),OutputTextures[0]);
	SetMaterial(0,MID);
	//GetStaticMesh()->SetMaterial(0,MID);

}

void UPhysicalSimulationComponent::Create2DRenderTarget()
{
	/*for(UTextureRenderTarget* RT: OutputTextures)
	{
		RT->ReleaseResource();
	}*/
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
	UMaterialInstanceDynamic* MID =  CreateDynamicMaterialInstance(0,Material.Get());
	MID->SetTextureParameterValue(TEXT("RT"),OutputTextures[0]);
	MID->SetTextureParameterValue(TEXT("RT2"),OutputTextures[1]);
	SetMaterial(0,MID);
	//GetStaticMesh()->SetMaterial(0,MID);
}
