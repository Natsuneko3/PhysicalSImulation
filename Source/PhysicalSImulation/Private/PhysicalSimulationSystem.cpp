// Copyright Natsu Neko, Inc. All Rights Reserved.

#include "PhysicalSimulationSystem.h"
#include "Physical2DFluidSolver.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"



// Sets default values
UPhysicalSimulationComponent::UPhysicalSimulationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	bTickInEditor = true;
	CenterOnwerPosition = FVector3f(0);
	LastOnwerPosition = FVector3f(0);
	Initial();
}

UPhysicalSimulationComponent::~UPhysicalSimulationComponent()
{
	delete PhysicalSolver;
}

void UPhysicalSimulationComponent::DoSimulation()
{
	SetupSolverParameter();
	
	
	
	if(!SimulationTexture || !PressureTexture )
	{
		InitialRenderTaget();
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
			FPhysicalSolverContext Context;
			Context.FeatureLevel = GetWorld()->Scene->GetFeatureLevel();
			Context.OwnerActor = GetOwner();

			PhysicalSolver->SetParameter(SolverParameter);
			PhysicalSolver->Update_RenderThread(GraphBuilder,OutputArray,Context);
			
			//
			
		GraphBuilder.Execute();
	});


	
}

void UPhysicalSimulationComponent::Initial()
{

	InitialRenderTaget();
	PhysicalSolver = MakeShareable(new FPhysical2DFluidSolver).Object;


	//SetupSolverParameter();
}

void UPhysicalSimulationComponent::InitialRenderTaget()
{
	UTextureRenderTarget2D* NewRenderTarget2D = NewObject<UTextureRenderTarget2D>();
	check(NewRenderTarget2D);
	NewRenderTarget2D->RenderTargetFormat = RTF_RGBA16f;
	NewRenderTarget2D->ClearColor = FLinearColor::Black;
	NewRenderTarget2D->bAutoGenerateMips = true;
	NewRenderTarget2D->bCanCreateUAV = true;
	NewRenderTarget2D->InitAutoFormat(GridSize.X, GridSize.Y);
	NewRenderTarget2D->UpdateResourceImmediate(true);

	UTextureRenderTarget2D* NewPressureRenderTarget2D = NewObject<UTextureRenderTarget2D>();
	check(NewPressureRenderTarget2D);
	NewPressureRenderTarget2D->RenderTargetFormat = RTF_R16f;
	NewPressureRenderTarget2D->ClearColor = FLinearColor::Black;
	NewPressureRenderTarget2D->bAutoGenerateMips = true;
	NewPressureRenderTarget2D->bCanCreateUAV = true;
	NewPressureRenderTarget2D->InitAutoFormat(GridSize.X, GridSize.Y);
	NewPressureRenderTarget2D->UpdateResourceImmediate(true);

	PressureTexture = NewPressureRenderTarget2D;
	SimulationTexture = NewRenderTarget2D;
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
	if(bSimulation)
	{
		LastOnwerPosition = CenterOnwerPosition;
		CenterOnwerPosition = FVector3f(GetOwner()->GetActorLocation());
		DoSimulation();
	}
}

void UPhysicalSimulationComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	UE_LOG(LogTemp,Log,TEXT("CHange name:%s"),*PropertyChangedEvent.GetPropertyName().ToString());

	//Initial();
	
}

void UPhysicalSimulationComponent::SetupSolverParameter()
{
	/*FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
			GetWorld()->GetGameViewport()->Viewport,
			GetWorld()->Scene,
			GetWorld()->GetGameViewport()->EngineShowFlags));*/
	// this View is deleted by the FSceneViewFamilyContext destructor
	/*FViewInfo
	FSceneView* View =  GEngine->GameViewportCalcSceneView(&ViewFamily);
	FSceneView* View = GEngine->GameViewport.*/
	FSolverBaseParameter SolverBase ;
	SolverBase.dt = GetWorld()->GetDeltaSeconds() * 2;
	SolverBase.dx = 1.0;//1.0 / (float)GridSize.X;
	SolverBase.GridSize = FVector3f( GridSize);
	SolverBase.Time = 0;
	UTexture2D* NoiseTexture  = LoadObject<UTexture2D>(nullptr,TEXT("/SpeedTreeImporter/SpeedTree9/game_wind_noise.game_wind_noise"));
	SolverBase.WarpSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
	//SolverBase.View
	//SolverBase.NoiseTexture = NoiseTexture->TextureReference.TextureReferenceRHI;

	//SolverBase.View = GetViewFamilyInfo()//GetWorld()->Scene->GetRenderScene()
	
	SolverParameter = FSolverParameter();
	SolverParameter.FluidParameter.SolverBaseParameter = SolverBase;
	SolverParameter.FluidParameter.VorticityMult = VorticityMult;
	SolverParameter.FluidParameter.NoiseFrequency = NoiseFrequency;
	SolverParameter.FluidParameter.NoiseIntensity = NoiseIntensity;
	SolverParameter.FluidParameter.VelocityDissipate = VelocityDissipate;
	SolverParameter.FluidParameter.DensityDissipate = DensityDissipate;
	SolverParameter.FluidParameter.GravityScale = GravityScale;
	SolverParameter.FluidParameter.UseFFT = true;

}

