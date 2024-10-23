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
DECLARE_CYCLE_STAT(TEXT("PoissonFilterX"), STAT_PoissonFilterX, STATGROUP_CustomRenderFeature)
DECLARE_CYCLE_STAT(TEXT("PoissonFilterY"), STAT_PoissonFilterY, STATGROUP_CustomRenderFeature)
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
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, SimulationTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, SimulationTextureSampler)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};

class FPreVelocityCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FPreVelocityCS);
	SHADER_USE_PARAMETER_STRUCT(FPreVelocityCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)

		SHADER_PARAMETER(int, AdvectionDensity)
		SHADER_PARAMETER(int, IterationIndex)
		SHADER_PARAMETER(int, bIsXDirector)


		SHADER_PARAMETER_STRUCT_INCLUDE(FFluidParameter, FluidParameter)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SimGridSRV)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, SimGridUAV)
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
	}
};

class FAdvectionCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FAdvectionCS);
	SHADER_USE_PARAMETER_STRUCT(FAdvectionCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)

		SHADER_PARAMETER(int, AdvectionDensity)
		SHADER_PARAMETER(int, IterationIndex)
		SHADER_PARAMETER(int, bIsXDirector)


		SHADER_PARAMETER_STRUCT_INCLUDE(FFluidParameter, FluidParameter)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SimGridSRV)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, SimGridUAV)

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
		OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
	}
};

class FComputeDivergenceCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FComputeDivergenceCS);
	SHADER_USE_PARAMETER_STRUCT(FComputeDivergenceCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
		SHADER_PARAMETER_STRUCT_INCLUDE(FFluidParameter, FluidParameter)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SimGridSRV)

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
		OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
	}
};

class FJacobiPressureCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FJacobiPressureCS);
	SHADER_USE_PARAMETER_STRUCT(FJacobiPressureCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
		SHADER_PARAMETER_STRUCT_INCLUDE(FFluidParameter, FluidParameter)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, LastPressureSRV)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, PressureGridUAV)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SimGridSRV)

	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), ThreadNum.X);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), ThreadNum.Y);
		OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
	}
};

class FPoissonFilterXCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FPoissonFilterXCS);
	SHADER_USE_PARAMETER_STRUCT(FPoissonFilterXCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)

		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SimGridSRV)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, PoissonFilterUAV)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, PoissonFilter1UAV)
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

class FPoissonFilterYCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FPoissonFilterYCS);
	SHADER_USE_PARAMETER_STRUCT(FPoissonFilterYCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)

		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, PoissonFilter1SRV)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, PoissonFilterSRV)
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

IMPLEMENT_GLOBAL_SHADER(FPoissonFilterXCS, "/Plugin/CustomRenderFeature/2DFluid.usf", "PoissonFilterXCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FPoissonFilterYCS, "/Plugin/CustomRenderFeature/2DFluid.usf", "PoissonFilterYCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FPreVelocityCS, "/Plugin/CustomRenderFeature/2DFluid.usf", "PreVelocityCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FAdvectionCS, "/Plugin/CustomRenderFeature/2DFluid.usf", "AdvectionCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FComputeDivergenceCS, "/Plugin/CustomRenderFeature/2DFluid.usf", "ComputeDivergenceCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FJacobiPressureCS, "/Plugin/CustomRenderFeature/2DFluid.usf", "JacobiIteratePressure", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FSmokePlanePS, "/Plugin/CustomRenderFeature/SmokePlanePass.usf", "MainPS", SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, FFFTSolvePoissonCS<512>, TEXT("/Plugin/CustomRenderFeature/FFTSolvePoisson.usf"), TEXT("MainCS"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FFFTSolvePoissonCS<256>, TEXT("/Plugin/CustomRenderFeature/FFTSolvePoisson.usf"), TEXT("MainCS"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FFFTSolvePoissonCS<128>, TEXT("/Plugin/CustomRenderFeature/FFTSolvePoisson.usf"), TEXT("MainCS"), SF_Compute);

UPhysical2DFluidSolver::UPhysical2DFluidSolver()
{
}

UPhysical2DFluidSolver::~UPhysical2DFluidSolver()
{
	Release();
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
	SolverParameter.InTextureSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();

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

	if (!PlandFluidParameters.InTexture1)
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
	//FRDGTextureUAVRef SimUAV = GraphBuilder.CreateUAV(SimulationTexture);
	FRDGTextureUAVRef PressureUAV = GraphBuilder.CreateUAV(PressureTexture);
	if (Frame == 1)
	{
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(SimulationTexture), FLinearColor::Transparent);
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(PressureTexture), FLinearColor::Transparent);
	}

	FFluidParameter FluidParameter;

	ShaderPrint::FShaderPrintSetup ShaderPrintSetup(InView);
	FShaderPrintData ShaderPrintData = ShaderPrint::CreateShaderPrintData(GraphBuilder, ShaderPrintSetup);

	SetSolverParameter(FluidParameter, InView);
	FRDGTextureRef PreVelocityTexture = GraphBuilder.CreateTexture(TempDesc,TEXT("PreVelocityTexture"));
	FRDGTextureRef AdvectionVelocityTexture = GraphBuilder.CreateTexture(TempDesc,TEXT("AdvectionVelocityTexture"));

	//PreVelocitySolver
	{
		SCOPE_CYCLE_COUNTER(STAT_PreVelocitySolver);

		FPreVelocityCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FPreVelocityCS::FParameters>();
		if(SolvePressureMethod ==ESolvePressureMethod::PoissonFilter)
		{
			FluidParameter.SolverBaseParameter.dt = 1.f;
		}
		PassParameters->FluidParameter = FluidParameter;
		PassParameters->SimGridSRV = SimulationTexture;
		PassParameters->SimGridUAV = GraphBuilder.CreateUAV(PreVelocityTexture); //SimUAV;
		PassParameters->PressureGridUAV = PressureUAV;//GraphBuilder.CreateUAV(PressureTexture);
		PassParameters->bIsXDirector = bUseFFT;
		PassParameters->AdvectionDensity = false;
		PassParameters->IterationIndex = 0;
		TShaderMapRef<FPreVelocityCS> ComputeShader(ShaderMap);


		FComputeShaderUtils::AddPass(GraphBuilder,
		                             RDG_EVENT_NAME("PreVelocitySolver"),
		                             ERDGPassFlags::Compute,
		                             ComputeShader,
		                             PassParameters,
		                             FComputeShaderUtils::GetGroupCount(FIntVector(GridSize.X, GridSize.Y, 1), ThreadNum));
		if(SolvePressureMethod ==ESolvePressureMethod::PoissonFilter)
		{
			FluidParameter.SolverBaseParameter.dt = GetWorld()->GetDeltaSeconds();
		}
	}

	//Advection Velocity
	{
		SCOPE_CYCLE_COUNTER(STAT_AdvectionVelocity);


		FAdvectionCS::FParameters* AdvectionPassParameters = GraphBuilder.AllocParameters<FAdvectionCS::FParameters>();
		//SetParameter(PassParameters, false, 0, SimUAV, PressureUAV, SimSRV, Advection);
		AdvectionPassParameters->AdvectionDensity = false;
		AdvectionPassParameters->IterationIndex = 0;
		AdvectionPassParameters->FluidParameter = FluidParameter;
		AdvectionPassParameters->PressureGridUAV = PressureUAV;
		AdvectionPassParameters->bIsXDirector = bUseFFT;

		AdvectionPassParameters->SimGridSRV = PreVelocityTexture;
		AdvectionPassParameters->SimGridUAV = GraphBuilder.CreateUAV(SimulationTexture);

		TShaderMapRef<FAdvectionCS> ComputeShader(ShaderMap);

		FComputeShaderUtils::AddPass(GraphBuilder,
		                             RDG_EVENT_NAME("AdvectionVelocity"),
		                             ERDGPassFlags::Compute,
		                             ComputeShader,
		                             AdvectionPassParameters,
		                             FComputeShaderUtils::GetGroupCount(FIntVector(GridSize.X, GridSize.Y, 1), ThreadNum));
	}

	//Initial Pressure Fleid
	FRDGTextureDesc TempPressureDesc = FRDGTextureDesc::Create2D(GridSize, PF_R32_FLOAT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV | TexCreate_RenderTargetable);
	FRDGTextureRef TempPressureGrid1 = GraphBuilder.CreateTexture(TempPressureDesc,TEXT("TempPressure1"));
	switch (SolvePressureMethod)
	{
	case ESolvePressureMethod::PoissonFilter:
		{
			{
				SCOPE_CYCLE_COUNTER(STAT_ComputeDivergence);
				TShaderMapRef<FComputeDivergenceCS> ComputeShader(ShaderMap);
				FComputeDivergenceCS::FParameters* ComputeDivergenceParameters = GraphBuilder.AllocParameters<FComputeDivergenceCS::FParameters>();

				ComputeDivergenceParameters->FluidParameter = FluidParameter;
				ComputeDivergenceParameters->PressureGridUAV = PressureUAV;
				ComputeDivergenceParameters->SimGridSRV = SimulationTexture;

				FComputeShaderUtils::AddPass(GraphBuilder,
											 RDG_EVENT_NAME("ComputeDivergence"),
											 ERDGPassFlags::Compute,
											 ComputeShader,
											 ComputeDivergenceParameters,
											 FComputeShaderUtils::GetGroupCount(FIntVector(GridSize.X, GridSize.Y, 1), ThreadNum));
			}

			FRDGTextureDesc PoissonFilterDesc = FRDGTextureDesc::Create2D(GridSize, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV | TexCreate_RenderTargetable);
			FRDGTextureRef PoissonFilterTexture = GraphBuilder.CreateTexture(PoissonFilterDesc,TEXT("PoissonFilter"));
			FRDGTextureRef PoissonFilterTexture1 = GraphBuilder.CreateTexture(PoissonFilterDesc,TEXT("PoissonFilter1"));
			{
				SCOPE_CYCLE_COUNTER(STAT_PoissonFilterX)
				FPoissonFilterXCS::FParameters* AdvectionPassParameters = GraphBuilder.AllocParameters<FPoissonFilterXCS::FParameters>();
				TShaderMapRef<FPoissonFilterXCS> ComputeShader(ShaderMap);
				AdvectionPassParameters->SimGridSRV = PreVelocityTexture;
				AdvectionPassParameters->PoissonFilterUAV = GraphBuilder.CreateUAV(PoissonFilterTexture);
				AdvectionPassParameters->PoissonFilter1UAV = GraphBuilder.CreateUAV(PoissonFilterTexture1);
				AdvectionPassParameters->PressureGridUAV = PressureUAV;
				FComputeShaderUtils::AddPass(GraphBuilder,
											 RDG_EVENT_NAME("PoissonFilterX"),
											 ERDGPassFlags::Compute,
											 ComputeShader,
											 AdvectionPassParameters,
											 FComputeShaderUtils::GetGroupCount(FIntVector(GridSize.X, GridSize.Y, 1), ThreadNum));
			}
			{
				SCOPE_CYCLE_COUNTER(STAT_PoissonFilterY)
				FPoissonFilterYCS::FParameters* FPoissonFilterParameters = GraphBuilder.AllocParameters<FPoissonFilterYCS::FParameters>();
				TShaderMapRef<FPoissonFilterYCS> ComputeShader(ShaderMap);
				FPoissonFilterParameters->PoissonFilterSRV = PoissonFilterTexture;
				FPoissonFilterParameters->PoissonFilter1SRV = PoissonFilterTexture1;
				FPoissonFilterParameters->PressureGridUAV = PressureUAV;
				FComputeShaderUtils::AddPass(GraphBuilder,
											 RDG_EVENT_NAME("PoissonFilterY"),
											 ERDGPassFlags::Compute,
											 ComputeShader,
											 FPoissonFilterParameters,
											 FComputeShaderUtils::GetGroupCount(FIntVector(GridSize.X, GridSize.Y, 1), ThreadNum));
			}
			break;
		}

	case ESolvePressureMethod::Normal:

		for (int i = 0; i < 10; i++)
		{
			FJacobiPressureCS::FParameters* PressurePassParameters = GraphBuilder.AllocParameters<FJacobiPressureCS::FParameters>();
			PressurePassParameters->FluidParameter = FluidParameter;
			PressurePassParameters->SimGridSRV = PreVelocityTexture;
			PressurePassParameters->PressureGridUAV = GraphBuilder.CreateUAV(TempPressureGrid1);
			PressurePassParameters->LastPressureSRV = PressureTexture;
			TShaderMapRef<FJacobiPressureCS> ComputeShader(ShaderMap);
			FComputeShaderUtils::AddPass(GraphBuilder,
										 RDG_EVENT_NAME("iteratePressure"),
										 ERDGPassFlags::Compute,
										 ComputeShader,
										 PressurePassParameters,
										 FComputeShaderUtils::GetGroupCount(FIntVector(GridSize.X, GridSize.Y, 1), ThreadNum));

			FJacobiPressureCS::FParameters* PressurePassParameters1 = GraphBuilder.AllocParameters<FJacobiPressureCS::FParameters>();
			PressurePassParameters1->FluidParameter = FluidParameter;
			PressurePassParameters1->SimGridSRV = PreVelocityTexture;
			PressurePassParameters1->PressureGridUAV = GraphBuilder.CreateUAV(PressureTexture);
			PressurePassParameters1->LastPressureSRV = TempPressureGrid1;
			TShaderMapRef<FJacobiPressureCS> ComputeShader1(ShaderMap);
			FComputeShaderUtils::AddPass(GraphBuilder,
										 RDG_EVENT_NAME("iteratePressure1"),
										 ERDGPassFlags::Compute,
										 ComputeShader1,
										 PressurePassParameters,
										 FComputeShaderUtils::GetGroupCount(FIntVector(GridSize.X, GridSize.Y, 1), ThreadNum));

		}
			break;

	case ESolvePressureMethod::FFT:
		{
			{
				SCOPE_CYCLE_COUNTER(STAT_ComputeDivergence);
				TShaderMapRef<FComputeDivergenceCS> ComputeShader(ShaderMap);
				FComputeDivergenceCS::FParameters* ComputeDivergenceParameters = GraphBuilder.AllocParameters<FComputeDivergenceCS::FParameters>();
				ComputeDivergenceParameters->FluidParameter = FluidParameter;
				ComputeDivergenceParameters->PressureGridUAV = PressureUAV;
				ComputeDivergenceParameters->SimGridSRV = SimulationTexture;

				FComputeShaderUtils::AddPass(GraphBuilder,
											 RDG_EVENT_NAME("ComputeDivergence"),
											 ERDGPassFlags::Compute,
											 ComputeShader,
											 ComputeDivergenceParameters,
											 FComputeShaderUtils::GetGroupCount(FIntVector(GridSize.X, GridSize.Y, 1), ThreadNum));
			}


			FRDGTextureRef TempPressureGrid2 = GraphBuilder.CreateTexture(TempPressureDesc,TEXT("TempPressure2"));
			FRDGTextureRef TempPressureGrid3 = GraphBuilder.CreateTexture(TempPressureDesc,TEXT("TempPressure3"));
			//FFT Y
			{
				SCOPE_CYCLE_COUNTER(STAT_FFTY);
				FFFTSolvePoissonCS<256>::FParameters* FFTPassParameters = GraphBuilder.AllocParameters<FFFTSolvePoissonCS<256>::FParameters>();
				TShaderMapRef<FFFTSolvePoissonCS<256>> ComputeShader(ShaderMap);
				FFTPassParameters->bIsInverse = false;
				FFTPassParameters->bTransformX = false;
				FFTPassParameters->InPressureSRV = PressureTexture;
				FFTPassParameters->PressureGridUAV = GraphBuilder.CreateUAV(TempPressureGrid1);
				FFTPassParameters->BaseParameter = FluidParameter.SolverBaseParameter;

				FComputeShaderUtils::AddPass(GraphBuilder,
											 RDG_EVENT_NAME("FFTY"),
											 ERDGPassFlags::Compute,
											 ComputeShader,
											 FFTPassParameters,
											 FIntVector(GridSize.X -1, 1, 1)); //FComputeShaderUtils::GetGroupCount( FIntVector(GridSize.X,GridSize.Y,1),FIntVector(GridSize.X ,1,1)));
			}

			//FFT X
			{
				SCOPE_CYCLE_COUNTER(STAT_FFTX);

				TShaderMapRef<FFFTSolvePoissonCS<256>> ComputeShader(ShaderMap);
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
											 FIntVector(GridSize.Y -1, 1, 1)); //FComputeShaderUtils::GetGroupCount( FIntVector(GridSize.X,GridSize.Y,1),FIntVector(GridSize.X ,1,1)));
			}

			//Inv FFT X
			{
				SCOPE_CYCLE_COUNTER(STAT_InvFFTX);

				TShaderMapRef<FFFTSolvePoissonCS<256>> ComputeShader(ShaderMap);
				FFFTSolvePoissonCS<256>::FParameters* FFTPassParameters = GraphBuilder.AllocParameters<FFFTSolvePoissonCS<256>::FParameters>();
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
											 FIntVector(GridSize.Y -1, 1, 1));
			}

			//Inv FFT Y
			{
				SCOPE_CYCLE_COUNTER(STAT_InvFFTY);

				TShaderMapRef<FFFTSolvePoissonCS<256>> ComputeShader(ShaderMap);
				FFFTSolvePoissonCS<256>::FParameters* FFTPassParameters = GraphBuilder.AllocParameters<FFFTSolvePoissonCS<256>::FParameters>();
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
											 FIntVector(GridSize.X -1, 1, 1));
			}
		}
	}

	//Advection Density
	/*{
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
	}*/
}

void UPhysical2DFluidSolver::Initial_RenderThread(FRHICommandListImmediate& RHICmdList)
{
	GridSize = FIntPoint(PlandFluidParameters.GridSize.X, PlandFluidParameters.GridSize.Y);
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
	InPSParameters->SceneTexturesStruct = Inputs.SceneTextures;

	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FSmokePlanePS> PixelShader(GlobalShaderMap);
	DrawMesh(ViewInfo, FMatrix44f(FVector3f(0), FVector3f(0), FVector3f(0), FVector3f(0)),
	         PixelShader, InPSParameters, GraphBuilder, ViewInfo.ViewRect, 1);
}

void UPhysical2DFluidSolver::Release()
{
	if (SimulationTexturePool)
	{
		GRenderTargetPool.FreeUnusedResource(SimulationTexturePool);
	}

	if (PressureTexturePool)
	{
		GRenderTargetPool.FreeUnusedResource(PressureTexturePool);
	}
}
