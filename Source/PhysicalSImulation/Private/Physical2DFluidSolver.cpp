#include "Physical2DFluidSolver.h"

#include "MeshPassProcessor.inl"
#include "PhysicalSimulationMeshProcessor.h"
#include "PhysicalSimulationSceneProxy.h"
#include "ShaderParameterStruct.h"
#include "PhysicalSolver.h"
#include "PixelShaderUtils.h"

DECLARE_CYCLE_STAT(TEXT("PreVelocitySolver"), STAT_PreVelocitySolver, STATGROUP_PS)
DECLARE_CYCLE_STAT(TEXT("AdvectionVelocity"), STAT_AdvectionVelocity, STATGROUP_PS)
DECLARE_CYCLE_STAT(TEXT("ComputeDivergence"), STAT_ComputeDivergence, STATGROUP_PS)
DECLARE_CYCLE_STAT(TEXT("FFTY"), STAT_FFTY, STATGROUP_PS)
DECLARE_CYCLE_STAT(TEXT("FFTX"), STAT_FFTX, STATGROUP_PS)
DECLARE_CYCLE_STAT(TEXT("InvFFTX"), STAT_InvFFTX, STATGROUP_PS)
DECLARE_CYCLE_STAT(TEXT("InvFFTY"), STAT_InvFFTY, STATGROUP_PS)
DECLARE_CYCLE_STAT(TEXT("AdvectionDensity"), STAT_AdvectionDensity, STATGROUP_PS)

enum EShadertype
{
	PreVel,
	Advection,
	IteratePressure,
	ComputeDivergence
};

const bool bUseFFT = true;
const FIntVector ThreadNum = FIntVector(8, 8, 1); //!bUseFFT? FIntVector(256,1,1):

class FSmokePlaneVS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FSmokePlaneVS);
	SHADER_USE_PARAMETER_STRUCT(FSmokePlaneVS, FGlobalShader);

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
	SHADER_PARAMETER(FMatrix44f,LocalToWorld)
	END_SHADER_PARAMETER_STRUCT()
};
class FSmokePlanePS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FSmokePlanePS);
	SHADER_USE_PARAMETER_STRUCT(FSmokePlanePS, FGlobalShader);

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
	SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D,SimulationTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState,SimulationTextureSampler)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FSmokePlanePS, "/PluginShader/SmokePlanePass.usf", "MainPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FSmokePlaneVS, "/PluginShader/SmokePlanePass.usf", "MainVS", SF_Vertex);

template <int GroupSize>
class FFFTSolvePoissonCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FFFTSolvePoissonCS);
	SHADER_USE_PARAMETER_STRUCT(FFFTSolvePoissonCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
		//SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D,FFTSolvePoissonSRV)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSolverBaseParameter, BaseParameter)
		SHADER_PARAMETER(int, bIsInverse)
		SHADER_PARAMETER(int, bTransformX)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, PressureGridUAV)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true; //IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), GroupSize);
	}
};

class F2DFluidCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(F2DFluidCS);
	SHADER_USE_PARAMETER_STRUCT(F2DFluidCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)

		SHADER_PARAMETER(int, AdvectionDensity)
		SHADER_PARAMETER(int, IterationIndex)
		SHADER_PARAMETER(int, bUseFFTPressure)
		SHADER_PARAMETER(int, FluidShaderType)

		SHADER_PARAMETER_STRUCT_INCLUDE(FFluidParameter, FluidParameter)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, SimGridSRV)

		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, SimGridUAV)


		SHADER_PARAMETER_STRUCT_INCLUDE(ShaderPrint::FShaderParameters, ShaderPrintUniformBuffer)

		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, PressureGridUAV)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), ThreadNum.X);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), ThreadNum.Y);
		OutEnvironment.SetDefine(TEXT("PREVELOCITY"), 0);
		OutEnvironment.SetDefine(TEXT("ADVECTION"), 1);

		OutEnvironment.SetDefine(TEXT("ITERATEPRESSURE"), 2);
		OutEnvironment.SetDefine(TEXT("COMPUTEDIVERGENCE"), 3);

		OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
	}
};

IMPLEMENT_GLOBAL_SHADER(F2DFluidCS, "/PluginShader/2DFluid.usf", "MainCS", SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FFFTSolvePoissonCS<512>, TEXT("/PluginShader/FFTSolvePoisson.usf"), TEXT("MainCS"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FFFTSolvePoissonCS<256>, TEXT("/PluginShader/FFTSolvePoisson.usf"), TEXT("MainCS"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FFFTSolvePoissonCS<128>, TEXT("/PluginShader/FFTSolvePoisson.usf"), TEXT("MainCS"), SF_Compute);


FPhysical2DFluidSolver::FPhysical2DFluidSolver(FPhysicalSimulationSceneProxy* InSceneProxy)
	: FPhysicalSolverBase(InSceneProxy)
{
	SceneProxy = InSceneProxy;
	Frame = 0;
	GridSize = FIntPoint(InSceneProxy->GridSize.X -1, InSceneProxy->GridSize.Y -1);
	ENQUEUE_RENDER_COMMAND(InitPhysical2DFluidSolver)(
		[this](FRHICommandListImmediate& RHICmdList)
		{
			Initial(RHICmdList);
		});
}

FPhysical2DFluidSolver::~FPhysical2DFluidSolver()
{
	VertexBufferRHI.SafeRelease();
	IndexBufferRHI.SafeRelease();
}

void FPhysical2DFluidSolver::SetSolverParameter(FFluidParameter& SolverParameter, FSceneView& InView)
{
	SolverParameter.DensityDissipate = SceneProxy->PlandFluidParameters->DensityDissipate;
	SolverParameter.GravityScale = SceneProxy->PlandFluidParameters->GravityScale;
	SolverParameter.NoiseFrequency = SceneProxy->PlandFluidParameters->NoiseFrequency;
	SolverParameter.NoiseIntensity = SceneProxy->PlandFluidParameters->NoiseIntensity;
	SolverParameter.VelocityDissipate = SceneProxy->PlandFluidParameters->VelocityDissipate;
	SolverParameter.VorticityMult = SceneProxy->PlandFluidParameters->VorticityMult;
	SolverParameter.UseFFT = true;
	SetupSolverBaseParameters(SolverParameter.SolverBaseParameter, InView);
}

void FPhysical2DFluidSolver::PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView)
{
	DECLARE_GPU_STAT(PlaneFluidSolver)
	RDG_EVENT_SCOPE(GraphBuilder, "PlaneFluidSolver");
	RDG_GPU_STAT_SCOPE(GraphBuilder, PlaneFluidSolver);

	Frame++;

	FRDGTextureRef SimulationTexture = GraphBuilder.RegisterExternalTexture(SimulationTexturePool);
	FRDGTextureRef PressureTexture = GraphBuilder.RegisterExternalTexture(PressureTexturePool);
	//TODO:Need Change shader map based of platforms
	auto ShaderMap = GetGlobalShaderMap(InView.FeatureLevel);

	FRDGTextureDesc TempDesc = FRDGTextureDesc::Create2D(GridSize, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV | TexCreate_RenderTargetable);
	FRDGTextureRef TempGrid = GraphBuilder.CreateTexture(TempDesc,TEXT("TempGrid"));
	AddClearUAVPass(GraphBuilder,GraphBuilder.CreateUAV(TempGrid),FLinearColor::Black);
	FRDGTextureUAVRef SimUAV = GraphBuilder.CreateUAV(SimulationTexture);
	FRDGTextureUAVRef PressureUAV = GraphBuilder.CreateUAV(PressureTexture);
	if(Frame == 1)
	{
		AddClearUAVPass(GraphBuilder,GraphBuilder.CreateUAV(GraphBuilder.RegisterExternalTexture(SimulationTexturePool)),FLinearColor::Black);
		AddClearUAVPass(GraphBuilder,GraphBuilder.CreateUAV(GraphBuilder.RegisterExternalTexture(PressureTexturePool)),FLinearColor::Black);
	}

	FRDGTextureSRVRef SimSRV = nullptr;

	FFluidParameter FluidParameter;
	SimSRV = GraphBuilder.CreateSRV(SimulationTexture);
	ShaderPrint::FShaderPrintSetup ShaderPrintSetup(InView);
	FShaderPrintData ShaderPrintData = ShaderPrint::CreateShaderPrintData(GraphBuilder, ShaderPrintSetup);


	FluidParameter.DensityDissipate = SceneProxy->PlandFluidParameters->DensityDissipate;
	FluidParameter.GravityScale = SceneProxy->PlandFluidParameters->GravityScale;
	FluidParameter.NoiseFrequency = SceneProxy->PlandFluidParameters->NoiseFrequency;
	FluidParameter.NoiseIntensity = SceneProxy->PlandFluidParameters->NoiseIntensity;
	FluidParameter.VelocityDissipate = SceneProxy->PlandFluidParameters->VelocityDissipate;
	FluidParameter.VorticityMult = SceneProxy->PlandFluidParameters->VorticityMult;
	FluidParameter.UseFFT = true;

	FluidParameter.SolverBaseParameter.dt = SceneProxy->World->GetDeltaSeconds();
	FluidParameter.SolverBaseParameter.dx = *SceneProxy->Dx;
	FluidParameter.SolverBaseParameter.Time = Frame;
	FluidParameter.SolverBaseParameter.View = InView.ViewUniformBuffer;
	FluidParameter.SolverBaseParameter.GridSize = FVector3f(SceneProxy->GridSize.X, SceneProxy->GridSize.Y, SceneProxy->GridSize.Z);
	FluidParameter.SolverBaseParameter.WarpSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
	FluidParameter.SimSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
	FluidParameter.WorldVelocity = FVector3f(0); //Context->WorldVelocity;
	FluidParameter.WorldPosition = FVector3f(0); //Context->WorldPosition;


	//PreVelocitySolver
	{
		SCOPE_CYCLE_COUNTER(STAT_PreVelocitySolver);

		F2DFluidCS::FParameters* PassParameters = GraphBuilder.AllocParameters<F2DFluidCS::FParameters>();
		PassParameters->FluidParameter = FluidParameter;
		PassParameters->FluidShaderType = PreVel;
		PassParameters->SimGridSRV = SimSRV;
		PassParameters->SimGridUAV = SimUAV;
		PassParameters->PressureGridUAV = PressureUAV;

		PassParameters->bUseFFTPressure = bUseFFT;

		PassParameters->AdvectionDensity = false;
		PassParameters->IterationIndex = 0;
		TShaderMapRef<F2DFluidCS> ComputeShader(ShaderMap);


		FComputeShaderUtils::AddPass(GraphBuilder,
		                             RDG_EVENT_NAME("PreVelocitySolver"),
		                             ERDGPassFlags::Compute,
		                             ComputeShader,
		                             PassParameters,
		                             FComputeShaderUtils::GetGroupCount(FIntVector(GridSize.X, GridSize.Y, 1), ThreadNum));
	}



	//Advection Velocity
	{
		SCOPE_CYCLE_COUNTER(STAT_AdvectionVelocity);

		if (RHIGetInterfaceType() < ERHIInterfaceType::D3D12)
		{
			AddCopyTexturePass(GraphBuilder, SimulationTexture, TempGrid);
			SimSRV = GraphBuilder.CreateSRV(TempGrid);
		}

		F2DFluidCS::FParameters* AdvectionPassParameters = GraphBuilder.AllocParameters<F2DFluidCS::FParameters>();
		//SetParameter(PassParameters, false, 0, SimUAV, PressureUAV, SimSRV, Advection);
		AdvectionPassParameters->AdvectionDensity = false;
		AdvectionPassParameters->IterationIndex = 0;
		AdvectionPassParameters->FluidParameter = FluidParameter;
		AdvectionPassParameters->PressureGridUAV = PressureUAV;
		AdvectionPassParameters->bUseFFTPressure = bUseFFT;

		AdvectionPassParameters->SimGridSRV = SimSRV;
		AdvectionPassParameters->SimGridUAV = SimUAV;
		AdvectionPassParameters->FluidShaderType = Advection;

		TShaderMapRef<F2DFluidCS> ComputeShader(ShaderMap);

		FComputeShaderUtils::AddPass(GraphBuilder,
		                             RDG_EVENT_NAME("AdvectionVelocity"),
		                             ERDGPassFlags::Compute,
		                             ComputeShader,
		                             AdvectionPassParameters,
		                             FComputeShaderUtils::GetGroupCount(FIntVector(GridSize.X, GridSize.Y, 1), ThreadNum));
	}
	//Solver Pressure Fleid
	if (bUseFFT)
	{
		{
			SCOPE_CYCLE_COUNTER(STAT_ComputeDivergence);

			//SetParameter(PassParameters, false, 0, SimUAV, PressureUAV, SimSRV, ComputeDivergence);
			//PermutationVector.Set<F2DFluidCS::FAdvection>(true);
			TShaderMapRef<F2DFluidCS> ComputeShader(ShaderMap);

			F2DFluidCS::FParameters* ComputeDivergenceParameters = GraphBuilder.AllocParameters<F2DFluidCS::FParameters>();
			//SetParameter(PassParameters, false, 0, SimUAV, PressureUAV, SimSRV, Advection);
			ComputeDivergenceParameters->AdvectionDensity = false;
			ComputeDivergenceParameters->IterationIndex = 0;
			ComputeDivergenceParameters->FluidParameter = FluidParameter;
			ComputeDivergenceParameters->PressureGridUAV = PressureUAV;
			ComputeDivergenceParameters->bUseFFTPressure = bUseFFT;

			ComputeDivergenceParameters->SimGridSRV = SimSRV;
			ComputeDivergenceParameters->SimGridUAV = SimUAV;
			ComputeDivergenceParameters->FluidShaderType = ComputeDivergence;
			FComputeShaderUtils::AddPass(GraphBuilder,
			                             RDG_EVENT_NAME("ComputeDivergence"),
			                             ERDGPassFlags::Compute,
			                             ComputeShader,
			                             ComputeDivergenceParameters,
			                             FComputeShaderUtils::GetGroupCount(FIntVector(GridSize.X, GridSize.Y, 1), ThreadNum));
		}

		//FFT Y
		FFFTSolvePoissonCS<256>::FParameters* FFTPassParameters = GraphBuilder.AllocParameters<FFFTSolvePoissonCS<256>::FParameters>();
		{
			SCOPE_CYCLE_COUNTER(STAT_FFTY);

			TShaderMapRef<FFFTSolvePoissonCS<256>> ComputeShader(ShaderMap);

			FFTPassParameters->bIsInverse = false;
			FFTPassParameters->bTransformX = false;
			FFTPassParameters->PressureGridUAV = PressureUAV;
			FFTPassParameters->BaseParameter = FluidParameter.SolverBaseParameter;

			FComputeShaderUtils::AddPass(GraphBuilder,
			                             RDG_EVENT_NAME("FFTY"),
			                             ERDGPassFlags::Compute,
			                             ComputeShader,
			                             FFTPassParameters,
			                             FIntVector(GridSize.X, 1, 1)); //FComputeShaderUtils::GetGroupCount( FIntVector(GridSize.X,GridSize.Y,1),FIntVector(GridSize.X ,1,1)));
		}

		//FFT X
		{
			SCOPE_CYCLE_COUNTER(STAT_FFTX);
			//FFFTSolvePoissonCS<256>::FParameters* PassParameters = GraphBuilder.AllocParameters<FFFTSolvePoissonCS<256>::FParameters>();
			TShaderMapRef<FFFTSolvePoissonCS<256>> ComputeShader(ShaderMap);

			FFTPassParameters->bIsInverse = false;
			FFTPassParameters->bTransformX = true;
			FFTPassParameters->PressureGridUAV = PressureUAV;
			FFTPassParameters->BaseParameter = FluidParameter.SolverBaseParameter;

			FComputeShaderUtils::AddPass(GraphBuilder,
			                             RDG_EVENT_NAME("FFTX"),
			                             ERDGPassFlags::Compute,
			                             ComputeShader,
			                             FFTPassParameters,
			                             FIntVector(GridSize.Y, 1, 1)); //FComputeShaderUtils::GetGroupCount( FIntVector(GridSize.X,GridSize.Y,1),FIntVector(GridSize.X ,1,1)));
		}

		//Inv FFT X
		{
			SCOPE_CYCLE_COUNTER(STAT_InvFFTX);

			TShaderMapRef<FFFTSolvePoissonCS<256>> ComputeShader(ShaderMap);
			FFTPassParameters->bIsInverse = true;
			FFTPassParameters->bTransformX = true;
			FFTPassParameters->PressureGridUAV = PressureUAV;
			FFTPassParameters->BaseParameter = FluidParameter.SolverBaseParameter;

			FComputeShaderUtils::AddPass(GraphBuilder,
			                             RDG_EVENT_NAME("InvFFTX"),
			                             ERDGPassFlags::Compute,
			                             ComputeShader,
			                             FFTPassParameters,
			                             FIntVector(GridSize.Y, 1, 1));
		}

		//Inv FFT Y
		{
			SCOPE_CYCLE_COUNTER(STAT_InvFFTY);

			TShaderMapRef<FFFTSolvePoissonCS<256>> ComputeShader(ShaderMap);
			FFTPassParameters->bIsInverse = true;
			FFTPassParameters->bTransformX = false;
			FFTPassParameters->PressureGridUAV = PressureUAV;
			FFTPassParameters->BaseParameter = FluidParameter.SolverBaseParameter;

			FComputeShaderUtils::AddPass(GraphBuilder,
			                             RDG_EVENT_NAME("InvFFTY"),
			                             ERDGPassFlags::Compute,
			                             ComputeShader,
			                             FFTPassParameters,
			                             FIntVector(GridSize.X, 1, 1));
		}
	}
	else
	{
		/*for (int i = 0; i < 20; i++)
		{
			//F2DFluidCS::FParameters* PassParameters = GraphBuilder.AllocParameters<F2DFluidCS::FParameters>();

			//SetParameter(PassParameters, false, i, SimUAV, PressureUAV, SimSRV, IteratePressure);

			F2DFluidCS::FPermutationDomain PermutationVector;
			//PermutationVector.Set<F2DFluidCS::FIteratePressure>(true);
			TShaderMapRef<F2DFluidCS> ComputeShader(ShaderMap, PermutationVector);


			FComputeShaderUtils::AddPass(GraphBuilder,
			                             RDG_EVENT_NAME("iteratePressure"),
			                             ERDGPassFlags::Compute,
			                             ComputeShader,
			                             PassParameters,
			                             FComputeShaderUtils::GetGroupCount(FIntVector(GridSize.X, GridSize.Y, 1), ThreadNum));
			//AddCopyTexturePass(GraphBuilder,TempGrid,OutTextureArray[1]);
		}*/
	}


	//Advection Density
	{
		SCOPE_CYCLE_COUNTER(STAT_AdvectionDensity);

		if (RHIGetInterfaceType() < ERHIInterfaceType::D3D12)
		{
			AddCopyTexturePass(GraphBuilder, SimulationTexture, TempGrid);
			SimSRV = GraphBuilder.CreateSRV(TempGrid);
		}
		F2DFluidCS::FParameters* AdvectionDensityPassParameters = GraphBuilder.AllocParameters<F2DFluidCS::FParameters>();

		//SetParameter(PassParameters, false, 0, SimUAV, PressureUAV, SimSRV, Advection);
		AdvectionDensityPassParameters->AdvectionDensity = true;
		AdvectionDensityPassParameters->IterationIndex = 0;
		AdvectionDensityPassParameters->FluidParameter = FluidParameter;
		AdvectionDensityPassParameters->PressureGridUAV = PressureUAV;
		AdvectionDensityPassParameters->bUseFFTPressure = bUseFFT;

		AdvectionDensityPassParameters->SimGridSRV = SimSRV;
		AdvectionDensityPassParameters->SimGridUAV = SimUAV;
		AdvectionDensityPassParameters->FluidShaderType = Advection;
		//SetParameter(PassParameters, true, 0, SimUAV, PressureUAV, SimSRV, Advection);
		TShaderMapRef<F2DFluidCS> ComputeShader(ShaderMap);


		FComputeShaderUtils::AddPass(GraphBuilder,
		                             RDG_EVENT_NAME("AdvectionDensity"),
		                             ERDGPassFlags::Compute,
		                             ComputeShader,
		                             AdvectionDensityPassParameters,
		                             FComputeShaderUtils::GetGroupCount(FIntVector(GridSize.X, GridSize.Y, 1), ThreadNum));
	}
}

void FPhysical2DFluidSolver::Initial(FRHICommandListImmediate& RHICmdList)
{
	InitialedDelegate.Broadcast();

	FPooledRenderTargetDesc RGBADesc(FPooledRenderTargetDesc::Create2DDesc(GridSize, PF_FloatRGBA,
	                                                                       FClearValueBinding(), TexCreate_None, TexCreate_ShaderResource | TexCreate_RenderTargetable | TexCreate_UAV, false));

	FPooledRenderTargetDesc FloatDesc(FPooledRenderTargetDesc::Create2DDesc(GridSize, PF_R32_FLOAT,
	                                                                        FClearValueBinding(), TexCreate_None, TexCreate_ShaderResource | TexCreate_RenderTargetable | TexCreate_UAV, false));
	GRenderTargetPool.FindFreeElement(RHICmdList, RGBADesc, SimulationTexturePool, TEXT("SimulationTexture"));
	GRenderTargetPool.FindFreeElement(RHICmdList, FloatDesc, PressureTexturePool, TEXT("PressureTexture"));



	InitialPlaneMesh();
}

void FPhysical2DFluidSolver::Render_RenderThread(FPostOpaqueRenderParameters& Parameters)
{
	const FSceneView* View = static_cast<FSceneView*>(Parameters.Uid);
	FRDGBuilder& GraphBuilder = *Parameters.GraphBuilder;
	if (!SimulationTexturePool.IsValid())
	{
		return;
	};

	FSmokePlaneVS::FParameters* InVSParameters = GraphBuilder.AllocParameters<FSmokePlaneVS::FParameters>();
	FSmokePlanePS::FParameters* InPSParameters = GraphBuilder.AllocParameters<FSmokePlanePS::FParameters>();
	InVSParameters->View = View->ViewUniformBuffer;
	InVSParameters->LocalToWorld = FMatrix44f(SceneProxy->ActorTransform->ToMatrixWithScale());

	InPSParameters->RenderTargets[0] = FRenderTargetBinding(Parameters.ColorTexture, ERenderTargetLoadAction::ELoad);
	InPSParameters->SimulationTexture = GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalTexture(SimulationTexturePool));
	InPSParameters->SimulationTextureSampler = TStaticSamplerState<SF_Point>::GetRHI();
	InPSParameters->View = View->ViewUniformBuffer;

	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FSmokePlaneVS> VertexShader(GlobalShaderMap);
	TShaderMapRef<FSmokePlanePS> PixelShader(GlobalShaderMap);
	DrawMesh(VertexShader, PixelShader, InVSParameters, InPSParameters, Parameters,1);

}

void FPhysical2DFluidSolver::Release()
{
	FPhysicalSolverBase::Release();
}
