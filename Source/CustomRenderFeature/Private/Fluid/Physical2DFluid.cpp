#include "Physical2DFluid.h"

#include "MeshPassProcessor.inl"
#include "ShaderParameterStruct.h"
#include "PixelShaderUtils.h"
#include "RenderTargetPool.h"

DECLARE_CYCLE_STAT(TEXT("PreVelocitySolver"), STAT_PreVelocitySolver, STATGROUP_CustomRenderFeature)
DECLARE_CYCLE_STAT(TEXT("AdvectionVelocity"), STAT_AdvectionVelocity, STATGROUP_CustomRenderFeature)
DECLARE_CYCLE_STAT(TEXT("ComputeDivergence"), STAT_ComputeDivergence, STATGROUP_CustomRenderFeature)
DECLARE_CYCLE_STAT(TEXT("FFTY"), STAT_FFTY, STATGROUP_CustomRenderFeature)
DECLARE_CYCLE_STAT(TEXT("FFTX"), STAT_FFTX, STATGROUP_CustomRenderFeature)
DECLARE_CYCLE_STAT(TEXT("InvFFTX"), STAT_InvFFTX, STATGROUP_CustomRenderFeature)
DECLARE_CYCLE_STAT(TEXT("InvFFTY"), STAT_InvFFTY, STATGROUP_CustomRenderFeature)
DECLARE_CYCLE_STAT(TEXT("AdvectionDensity"), STAT_AdvectionDensity, STATGROUP_CustomRenderFeature)

const bool bUseFFT = true;
const FIntVector ThreadNum = FIntVector(8, 8, 1); //!bUseFFT? FIntVector(256,1,1):

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
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, SimulationTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SimulationTextureSampler)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FSmokePlanePS, "/Plugin/CustomRenderFeature/SmokePlanePass.usf", "MainPS", SF_Pixel);

class F2DFluidCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(F2DFluidCS);
	SHADER_USE_PARAMETER_STRUCT(F2DFluidCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)

		SHADER_PARAMETER(int, AdvectionDensity)
		SHADER_PARAMETER(int, IterationIndex)
		SHADER_PARAMETER(int, bIsXDirector)
		SHADER_PARAMETER(int, FluidShaderType)

		SHADER_PARAMETER_STRUCT_INCLUDE(FFluidParameter, FluidParameter)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SimGridSRV)
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
		OutEnvironment.SetDefine(TEXT("POISSONFILTER"), 3);
		OutEnvironment.SetDefine(TEXT("COMPUTEDIVERGENCE"), 4);

		OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
	}
};

class FPoissonFilterCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FPoissonFilterCS);
	SHADER_USE_PARAMETER_STRUCT(FPoissonFilterCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )

	    SHADER_PARAMETER_RDG_TEXTURE_SRV(RWTexture2D,PoissonFilterSRV)
	    SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, PressureGridUAV)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), 8);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), 8);
	}
};
IMPLEMENT_GLOBAL_SHADER(FPoissonFilterCS, "/Plugin/CustomRenderFeature/2DFluid.usf", "PoissonFilterCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(F2DFluidCS, "/Plugin/CustomRenderFeature/2DFluid.usf", "MainCS", SF_Compute);

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
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InPressureSRV)
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

IMPLEMENT_SHADER_TYPE(template<>, FFFTSolvePoissonCS<512>, TEXT("/Plugin/CustomRenderFeature/FFTSolvePoisson.usf"), TEXT("MainCS"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FFFTSolvePoissonCS<256>, TEXT("/Plugin/CustomRenderFeature/FFTSolvePoisson.usf"), TEXT("MainCS"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FFFTSolvePoissonCS<128>, TEXT("/Plugin/CustomRenderFeature/FFTSolvePoisson.usf"), TEXT("MainCS"), SF_Compute);

UPhysical2DFluidSolver::UPhysical2DFluidSolver()
{
}

UPhysical2DFluidSolver::~UPhysical2DFluidSolver()
{
	VertexBufferRHI.SafeRelease();
	IndexBufferRHI.SafeRelease();
}

void UPhysical2DFluidSolver::SetSolverParameter(FFluidParameter& SolverParameter, FSceneView& InView)
{
	SolverParameter.DensityDissipate = PlandFluidParameters.DensityDissipate;
	SolverParameter.GravityScale = PlandFluidParameters.GravityScale;
	SolverParameter.NoiseFrequency = PlandFluidParameters.NoiseFrequency;
	SolverParameter.NoiseIntensity = PlandFluidParameters.NoiseIntensity;
	SolverParameter.VelocityDissipate = PlandFluidParameters.VelocityDissipate;
	SolverParameter.VorticityMult = PlandFluidParameters.VorticityMult;
	SolverParameter.UseFFT = true;
	SolverParameter.SimSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
	SolverParameter.WorldVelocity = FVector3f(0); //Context->WorldVelocity;
	SolverParameter.WorldPosition = FVector3f(0); //Context->WorldPosition;*/
	SolverParameter.InTexture = PlandFluidParameters.InTexture1->GetResource()->GetTextureRHI();
	SolverParameter.InTextureSampler = TStaticSamplerState<SF_Bilinear,AM_Wrap,AM_Wrap,AM_Wrap>::GetRHI();

	SolverParameter.SolverBaseParameter.dt = GetWorld()->GetDeltaSeconds();
	SolverParameter.SolverBaseParameter.dx = 1.f;
	SolverParameter.SolverBaseParameter.Time = Frame;
	SolverParameter.SolverBaseParameter.View = InView.ViewUniformBuffer;
	SolverParameter.SolverBaseParameter.GridSize = PlandFluidParameters.GridSize;
	SolverParameter.SolverBaseParameter.WarpSampler = TStaticSamplerState<>::GetRHI();

}

void UPhysical2DFluidSolver::PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView)
{
	DECLARE_GPU_STAT(PlaneFluidSolver)
	RDG_EVENT_SCOPE(GraphBuilder, "PlaneFluidSolver");
	RDG_GPU_STAT_SCOPE(GraphBuilder, PlaneFluidSolver);

	if(!PlandFluidParameters.InTexture1)
	{
		return;
	}
	Frame++;

	FRDGTextureRef SimulationTexture = GraphBuilder.RegisterExternalTexture(SimulationTexturePool);
	FRDGTextureRef PressureTexture = GraphBuilder.RegisterExternalTexture(PressureTexturePool);
	//TODO:Need Change shader map based of platforms
	auto ShaderMap = GetGlobalShaderMap(InView.FeatureLevel);

	FRDGTextureDesc TempDesc = FRDGTextureDesc::Create2D(GridSize, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV | TexCreate_RenderTargetable);
	FRDGTextureRef TempGrid = GraphBuilder.CreateTexture(TempDesc,TEXT("TempGrid"));
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(TempGrid), FLinearColor::Black);
	FRDGTextureUAVRef SimUAV = GraphBuilder.CreateUAV(SimulationTexture);
	FRDGTextureUAVRef PressureUAV = GraphBuilder.CreateUAV(PressureTexture);
	if (Frame == 1)
	{
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(GraphBuilder.RegisterExternalTexture(SimulationTexturePool)), FLinearColor::Black);
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(GraphBuilder.RegisterExternalTexture(PressureTexturePool)), FLinearColor::Black);
	}

	FRDGTextureSRVRef SimSRV = nullptr;

	FFluidParameter FluidParameter;
	SimSRV = GraphBuilder.CreateSRV(SimulationTexture);
	ShaderPrint::FShaderPrintSetup ShaderPrintSetup(InView);
	FShaderPrintData ShaderPrintData = ShaderPrint::CreateShaderPrintData(GraphBuilder, ShaderPrintSetup);

	SetSolverParameter(FluidParameter, InView);
	FRDGTextureRef PreVelocityTexture = GraphBuilder.CreateTexture(TempDesc,TEXT("PreVelocityTexture"));
	FRDGTextureRef AdvectionVelocityTexture = GraphBuilder.CreateTexture(TempDesc,TEXT("AdvectionVelocityTexture"));
	FRDGTextureRef AdvectionDensityTexture = GraphBuilder.CreateTexture(TempDesc,TEXT("AdvectionVelocityTexture"));
	//PreVelocitySolver
	{
		SCOPE_CYCLE_COUNTER(STAT_PreVelocitySolver);

		F2DFluidCS::FParameters* PassParameters = GraphBuilder.AllocParameters<F2DFluidCS::FParameters>();
		PassParameters->FluidParameter = FluidParameter;
		PassParameters->FluidShaderType = PreVel;
		PassParameters->SimGridSRV = SimulationTexture;
		PassParameters->SimGridUAV = GraphBuilder.CreateUAV(PreVelocityTexture); //SimUAV;
		PassParameters->PressureGridUAV = GraphBuilder.CreateUAV(PressureTexture);

		PassParameters->bIsXDirector = bUseFFT;

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
		AdvectionPassParameters->bIsXDirector = bUseFFT;

		AdvectionPassParameters->SimGridSRV = PreVelocityTexture;
		AdvectionPassParameters->SimGridUAV = GraphBuilder.CreateUAV(AdvectionVelocityTexture);
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
			ComputeDivergenceParameters->bIsXDirector = bUseFFT;

			ComputeDivergenceParameters->SimGridSRV = AdvectionVelocityTexture;
			ComputeDivergenceParameters->SimGridUAV = SimUAV;
			ComputeDivergenceParameters->FluidShaderType = ComputeDivergence;
			FComputeShaderUtils::AddPass(GraphBuilder,
			                             RDG_EVENT_NAME("ComputeDivergence"),
			                             ERDGPassFlags::Compute,
			                             ComputeShader,
			                             ComputeDivergenceParameters,
			                             FComputeShaderUtils::GetGroupCount(FIntVector(GridSize.X, GridSize.Y, 1), ThreadNum));
		}
		FRDGTextureDesc TempPressureDesc = FRDGTextureDesc::Create2D(GridSize, PF_R32_FLOAT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV | TexCreate_RenderTargetable);
		FRDGTextureRef TempPressureGrid1 = GraphBuilder.CreateTexture(TempPressureDesc,TEXT("TempPressure1"));
		FRDGTextureRef TempPressureGrid2 = GraphBuilder.CreateTexture(TempPressureDesc,TEXT("TempPressure2"));
		FRDGTextureRef TempPressureGrid3 = GraphBuilder.CreateTexture(TempPressureDesc,TEXT("TempPressure3"));

		FRDGTextureDesc PoissonFilterDesc = FRDGTextureDesc::Create2D(GridSize, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV | TexCreate_RenderTargetable);
		FRDGTextureRef PoissonFilterTexture = GraphBuilder.CreateTexture(PoissonFilterDesc,TEXT("PoissonFilter"));
		{
			F2DFluidCS::FParameters* AdvectionPassParameters = GraphBuilder.AllocParameters<F2DFluidCS::FParameters>();
			//SetParameter(PassParameters, false, 0, SimUAV, PressureUAV, SimSRV, Advection);
			AdvectionPassParameters->AdvectionDensity = false;
			AdvectionPassParameters->IterationIndex = 0;
			AdvectionPassParameters->FluidParameter = FluidParameter;

			AdvectionPassParameters->bIsXDirector = true;
			AdvectionPassParameters->SimGridSRV = PreVelocityTexture;
			AdvectionPassParameters->SimGridUAV = GraphBuilder.CreateUAV(PoissonFilterTexture);
			AdvectionPassParameters->PressureGridUAV = PressureUAV;
			AdvectionPassParameters->FluidShaderType = PoissonFilter;

			TShaderMapRef<F2DFluidCS> ComputeShader(ShaderMap);

			FComputeShaderUtils::AddPass(GraphBuilder,
										 RDG_EVENT_NAME("PoissonFilterX"),
										 ERDGPassFlags::Compute,
										 ComputeShader,
										 AdvectionPassParameters,
										 FComputeShaderUtils::GetGroupCount(FIntVector(GridSize.X, GridSize.Y, 1), ThreadNum));
		}

		{
			FPoissonFilterCS::FParameters* FPoissonFilterParameters = GraphBuilder.AllocParameters<FPoissonFilterCS::FParameters>();

			FPoissonFilterParameters->PoissonFilterSRV = GraphBuilder.CreateSRV(PoissonFilterTexture);
			FPoissonFilterParameters->PressureGridUAV = PressureUAV;

			TShaderMapRef<FPoissonFilterCS> ComputeShader(ShaderMap);

			FComputeShaderUtils::AddPass(GraphBuilder,
										 RDG_EVENT_NAME("PoissonFilterY"),
										 ERDGPassFlags::Compute,
										 ComputeShader,
										 FPoissonFilterParameters,
										 FComputeShaderUtils::GetGroupCount(FIntVector(GridSize.X, GridSize.Y, 1), ThreadNum));
		}
		//FFT Y
		/*{
			SCOPE_CYCLE_COUNTER(STAT_FFTY);
			FFFTSolvePoissonCS<256>::FParameters* FFTPassParameters = GraphBuilder.AllocParameters<FFFTSolvePoissonCS<256>::FParameters>();

			TShaderMapRef<FFFTSolvePoissonCS<256>> ComputeShader(ShaderMap);

			FFTPassParameters->bIsInverse = false;
			FFTPassParameters->bTransformX = false;
			//AddCopyTexturePass(GraphBuilder,PressureTexture,TempPressureGrid);
			FFTPassParameters->InPressureSRV = PressureTexture;
			FFTPassParameters->PressureGridUAV = GraphBuilder.CreateUAV(TempPressureGrid1);
			FFTPassParameters->BaseParameter = FluidParameter.SolverBaseParameter;

			FComputeShaderUtils::AddPass(GraphBuilder,
			                             RDG_EVENT_NAME("FFTY"),
			                             ERDGPassFlags::Compute,
			                             ComputeShader,
			                             FFTPassParameters,
			                             FIntVector(GridSize.X - 1, 1, 1)); //FComputeShaderUtils::GetGroupCount( FIntVector(GridSize.X,GridSize.Y,1),FIntVector(GridSize.X ,1,1)));
		}

		//FFT X
		{
			SCOPE_CYCLE_COUNTER(STAT_FFTX);
			//FFFTSolvePoissonCS<256>::FParameters* PassParameters = GraphBuilder.AllocParameters<FFFTSolvePoissonCS<256>::FParameters>();
			TShaderMapRef<FFFTSolvePoissonCS<256>> ComputeShader(ShaderMap);
			//AddCopyTexturePass(GraphBuilder,PressureTexture,TempPressureGrid);
			FFFTSolvePoissonCS<256>::FParameters* FFTPassParameters = GraphBuilder.AllocParameters<FFFTSolvePoissonCS<256>::FParameters>();
			FFTPassParameters->InPressureSRV = TempPressureGrid1;
			FFTPassParameters->bIsInverse = false;
			FFTPassParameters->bTransformX = true;
			FFTPassParameters->PressureGridUAV = GraphBuilder.CreateUAV(TempPressureGrid2);
			FFTPassParameters->BaseParameter = FluidParameter.SolverBaseParameter;

			FComputeShaderUtils::AddPass(GraphBuilder,
			                             RDG_EVENT_NAME("FFTX"),
			                             ERDGPassFlags::Compute,
			                             ComputeShader,
			                             FFTPassParameters,
			                             FIntVector(GridSize.Y - 1, 1, 1)); //FComputeShaderUtils::GetGroupCount( FIntVector(GridSize.X,GridSize.Y,1),FIntVector(GridSize.X ,1,1)));
		}

		//Inv FFT X
		{
			SCOPE_CYCLE_COUNTER(STAT_InvFFTX);

			TShaderMapRef<FFFTSolvePoissonCS<256>> ComputeShader(ShaderMap);
			FFFTSolvePoissonCS<256>::FParameters* FFTPassParameters = GraphBuilder.AllocParameters<FFFTSolvePoissonCS<256>::FParameters>();
			//AddCopyTexturePass(GraphBuilder,PressureTexture,TempPressureGrid);
			FFTPassParameters->InPressureSRV = TempPressureGrid2;
			FFTPassParameters->bIsInverse = true;
			FFTPassParameters->bTransformX = true;
			FFTPassParameters->PressureGridUAV = GraphBuilder.CreateUAV(TempPressureGrid3);
			FFTPassParameters->BaseParameter = FluidParameter.SolverBaseParameter;

			FComputeShaderUtils::AddPass(GraphBuilder,
			                             RDG_EVENT_NAME("InvFFTX"),
			                             ERDGPassFlags::Compute,
			                             ComputeShader,
			                             FFTPassParameters,
			                             FIntVector(GridSize.Y - 1, 1, 1));
		}

		//Inv FFT Y
		{
			SCOPE_CYCLE_COUNTER(STAT_InvFFTY);

			TShaderMapRef<FFFTSolvePoissonCS<256>> ComputeShader(ShaderMap);
			FFFTSolvePoissonCS<256>::FParameters* FFTPassParameters = GraphBuilder.AllocParameters<FFFTSolvePoissonCS<256>::FParameters>();
			//AddCopyTexturePass(GraphBuilder,PressureTexture,TempPressureGrid);
			FFTPassParameters->InPressureSRV = TempPressureGrid3;
			FFTPassParameters->bIsInverse = true;
			FFTPassParameters->bTransformX = false;
			FFTPassParameters->PressureGridUAV = PressureUAV;
			FFTPassParameters->BaseParameter = FluidParameter.SolverBaseParameter;

			FComputeShaderUtils::AddPass(GraphBuilder,
			                             RDG_EVENT_NAME("InvFFTY"),
			                             ERDGPassFlags::Compute,
			                             ComputeShader,
			                             FFTPassParameters,
			                             FIntVector(GridSize.X - 1, 1, 1));
		}*/
	}
	else
	{
		for (int i = 0; i < 20; i++)
		{
			F2DFluidCS::FParameters* PressurePassParameters = GraphBuilder.AllocParameters<F2DFluidCS::FParameters>();
			//SetParameter(PassParameters, false, 0, SimUAV, PressureUAV, SimSRV, Advection);
			PressurePassParameters->AdvectionDensity = false;
			PressurePassParameters->IterationIndex = 0;
			PressurePassParameters->FluidParameter = FluidParameter;
			PressurePassParameters->PressureGridUAV = PressureUAV;
			PressurePassParameters->bIsXDirector = bUseFFT;

			PressurePassParameters->SimGridSRV = PreVelocityTexture;
			PressurePassParameters->SimGridUAV = GraphBuilder.CreateUAV(AdvectionVelocityTexture);
			PressurePassParameters->FluidShaderType = Advection;

			TShaderMapRef<F2DFluidCS> ComputeShader(ShaderMap);


			FComputeShaderUtils::AddPass(GraphBuilder,
			                             RDG_EVENT_NAME("iteratePressure"),
			                             ERDGPassFlags::Compute,
			                             ComputeShader,
			                             PressurePassParameters,
			                             FComputeShaderUtils::GetGroupCount(FIntVector(GridSize.X, GridSize.Y, 1), ThreadNum));
			//AddCopyTexturePass(GraphBuilder,TempGrid,OutTextureArray[1]);
		}
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
		AdvectionDensityPassParameters->bIsXDirector = bUseFFT;

		AdvectionDensityPassParameters->SimGridSRV = AdvectionVelocityTexture;
		AdvectionDensityPassParameters->SimGridUAV = GraphBuilder.CreateUAV(SimulationTexture);
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

void UPhysical2DFluidSolver::Initial_RenderThread(FRHICommandListImmediate& RHICmdList)
{
	GridSize = FIntPoint( PlandFluidParameters.GridSize.X,PlandFluidParameters.GridSize.Y);
	FPooledRenderTargetDesc RGBADesc(FPooledRenderTargetDesc::Create2DDesc(GridSize, PF_FloatRGBA,
	                                                                       FClearValueBinding(), TexCreate_None, TexCreate_ShaderResource | TexCreate_RenderTargetable | TexCreate_UAV, false));

	FPooledRenderTargetDesc FloatDesc(FPooledRenderTargetDesc::Create2DDesc(GridSize, PF_R32_FLOAT,
	                                                                        FClearValueBinding(), TexCreate_None, TexCreate_ShaderResource | TexCreate_RenderTargetable | TexCreate_UAV, false));
	GRenderTargetPool.FindFreeElement(RHICmdList, RGBADesc, SimulationTexturePool, TEXT("SimulationTexture"));
	GRenderTargetPool.FindFreeElement(RHICmdList, FloatDesc, PressureTexturePool, TEXT("PressureTexture"));
	InitialPlaneMesh(RHICmdList);
}

void UPhysical2DFluidSolver::PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs)
{
	//UE_LOG(LogTemp,Log,TEXT("Test: %i"),Parameters.View->StereoViewIndex);
	if (!SimulationTexturePool.IsValid() || !View.IsPrimarySceneView())
	{
		return;
	};
	const FViewInfo& ViewInfo = static_cast<const FViewInfo&>(View);
	FSmokePlanePS::FParameters* InPSParameters = GraphBuilder.AllocParameters<FSmokePlanePS::FParameters>();

	InPSParameters->RenderTargets[0] = FRenderTargetBinding((*Inputs.SceneTextures)->SceneColorTexture, ERenderTargetLoadAction::ELoad);
	InPSParameters->SimulationTexture = GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalTexture(SimulationTexturePool));
	InPSParameters->SimulationTextureSampler = TStaticSamplerState<SF_Point>::GetRHI();
	InPSParameters->View = View.ViewUniformBuffer;

	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FSmokePlanePS> PixelShader(GlobalShaderMap);
	DrawMesh(ViewInfo, FMatrix44f(FVector3f(0),FVector3f(0),FVector3f(0),FVector3f(0)),
		PixelShader, InPSParameters, GraphBuilder, ViewInfo.ViewRect, 1);
	FRDGTextureRef SceneColorTexture = GetSceneTexture(Inputs);
	FRDGTextureRef OutTexture = (*Inputs.SceneTextures)->SceneColorTexture;
	//AddTextureCombinePass(GraphBuilder,ViewInfo,Inputs,GraphBuilder.RegisterExternalTexture(SimulationTexturePool),OutTexture,&TextureBlendDesc);
}

void UPhysical2DFluidSolver::Release()
{
	SimulationTexturePool->Release();
	PressureTexturePool->Release();
}
