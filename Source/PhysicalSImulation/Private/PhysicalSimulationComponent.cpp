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
	// CenterOnwerPosition = FVector3f(0);
	// LastOnwerPosition = FVector3f(0);
	if (!GetStaticMesh())
	{
		SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")));
	}

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		Initial();
	}
}

UPhysicalSimulationComponent::~UPhysicalSimulationComponent()
{
	delete PhysicalSolver;
}

void UPhysicalSimulationComponent::DoSimulation()
{
	/*SetupSolverParameter();

	if(!SimulationTexture || !PressureTexture )
	{
		Initial();
	}

		ENQUEUE_RENDER_COMMAND(ActorPhysicalSimulation)([this](FRHICommandListImmediate& RHICmdList)
	{

		FRDGBuilder GraphBuilder(RHICmdList);
			TArray<FRDGTextureRef> OutputArray;
			TRefCountPtr<IPooledRenderTarget> PreViewTextureRT = CreateRenderTarget(SimulationTexture->GetResource()->GetTextureRHI(), TEXT("PreViewTexture"));
			FRDGTextureRef OutRT = GraphBuilder.RegisterExternalTexture(PreViewTextureRT);

			TRefCountPtr<IPooledRenderTarget> PressureTextureRT = CreateRenderTarget(PressureTexture->GetResource()->GetTextureRHI(), TEXT("PressureTexture"));
			FRDGTextureRef OutPreRT = GraphBuilder.RegisterExternalTexture(PressureTextureRT);
	
			OutputArray.Add(OutRT);
			OutputArray.Add(OutPreRT);

			PhysicalSolver->SetParameter(SolverParameter);
			PhysicalSolver->Update_RenderThread(GraphBuilder,OutputArray,PhysicalSolverContext);

		GraphBuilder.Execute();
	});*/
}

void UPhysicalSimulationComponent::Initial()
{

	CreateSolverTextures();
	if(GetOwner())
	{
		SetupSolverParameter();
		PhysicalSolverContext.WorldVelocity = FVector3f(GetOwner()->GetVelocity());
		PhysicalSolverContext.WorldPosition = FVector3f(GetOwner()->GetActorLocation());
	}

	PhysicalSolverContext.SimulatorType = SimulatorType;
	PhysicalSolverContext.SolverParameter = &SolverParameter;
	PhysicalSolverContext.OutputTextures = OutputTextures;

	PhysicalSolverViewExtension = FSceneViewExtensions::NewExtension<FPhysicalSolverViewExtension>();
	PhysicalSolverViewExtension->Initial(&PhysicalSolverContext);
	if(Material)
	{
		UMaterialInstanceDynamic* MID =  CreateDynamicMaterialInstance(0,Material.Get());
		MID->SetTextureParameterValue(TEXT("RT"),OutputTextures[0]);
		MID->SetTextureParameterValue(TEXT("RT2"),OutputTextures[1]);
		GetStaticMesh()->SetMaterial(0,MID);
	}

	//SetupSolverParameter();
}


// Called when the game starts or when spawned
void UPhysicalSimulationComponent::BeginPlay()
{
	Super::BeginPlay();

	//DoSimulation();
}

void UPhysicalSimulationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (bSimulation)
	{
		// LastOnwerPosition = CenterOnwerPosition;
		// CenterOnwerPosition = FVector3f(GetOwner()->GetActorLocation());
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
}

void UPhysicalSimulationComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

}

/*FPrimitiveSceneProxy* UPhysicalSimulationComponent::CreateSceneProxy()
{
	return new FPhysicalSolverSceneProxy(this);
}*/

void UPhysicalSimulationComponent::SetupSolverParameter()
{

	if(!GetWorld())
	{
		return;
	}
	FSolverBaseParameter SolverBase;
	SolverBase.dt = GetWorld()->GetDeltaSeconds();
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

void UPhysicalSimulationComponent::CreateSolverTextures()
{
	OutputTextures.Empty();
	switch (SimulatorType)
	{
	case ESimulatorType::PlaneSmokeFluid:
		Create2DRenderTarget();
		break;

	case ESimulatorType::CubeSmokeFluid:
		Create3DRenderTarget();
		break;
	case ESimulatorType::Water:
		Create3DRenderTarget();
		break;
	}
}

void UPhysicalSimulationComponent::Create3DRenderTarget()
{
	UTextureRenderTargetVolume* VolumeRT = NewObject<UTextureRenderTargetVolume>();
	VolumeRT->InitAutoFormat(GridSize.X, GridSize.Y, GridSize.Z);
	VolumeRT->OverrideFormat = PF_FloatRGB;
	VolumeRT->ClearColor = FLinearColor::Black;
	VolumeRT->bCanCreateUAV = true;
	VolumeRT->UpdateResourceImmediate(true);
	SimulationTexture = VolumeRT;
}

void UPhysicalSimulationComponent::Create2DRenderTarget()
{
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
