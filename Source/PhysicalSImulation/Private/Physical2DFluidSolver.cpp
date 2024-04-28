#include "Physical2DFluidSolver.h"

#include "PhysicalSimulationSceneProxy.h"
#include "PhysicalSolver.h"

#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "ShaderParameterStruct.h"
#include "ShaderPrintParameters.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Runtime/Renderer/Private/SceneRendering.h"

DECLARE_CYCLE_STAT(TEXT("PreVelocitySolver"),STAT_PreVelocitySolver,STATGROUP_PS)
DECLARE_CYCLE_STAT(TEXT("AdvectionVelocity"),STAT_AdvectionVelocity,STATGROUP_PS)
DECLARE_CYCLE_STAT(TEXT("ComputeDivergence"),STAT_ComputeDivergence,STATGROUP_PS)
DECLARE_CYCLE_STAT(TEXT("FFTY"),STAT_FFTY,STATGROUP_PS)
DECLARE_CYCLE_STAT(TEXT("FFTX"),STAT_FFTX,STATGROUP_PS)
DECLARE_CYCLE_STAT(TEXT("InvFFTX"),STAT_InvFFTX,STATGROUP_PS)
DECLARE_CYCLE_STAT(TEXT("InvFFTY"),STAT_InvFFTY,STATGROUP_PS)
DECLARE_CYCLE_STAT(TEXT("AdvectionDensity"),STAT_AdvectionDensity,STATGROUP_PS)
enum EShadertype
{
	PreVel,
	Advection,
	IteratePressure,
	ComputeDivergence
};

const bool bUseFFT = true;
const FIntVector ThreadNum = FIntVector(8, 8, 1); //!bUseFFT? FIntVector(256,1,1):

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
		SHADER_PARAMETER_SAMPLER(SamplerState, SimSampler)

		SHADER_PARAMETER(FVector3f, WorldVelocity)
		SHADER_PARAMETER(FVector3f, WorldPosition)
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
	Frame = 0;
	GridSize = FIntPoint(InSceneProxy->GridSize.X,InSceneProxy->GridSize.Y);
}

void FPhysical2DFluidSolver::SetSolverParameter(FFluidParameter& SolverParameter,FSceneView& InView)
{

	SolverParameter.DensityDissipate = SceneProxy->PlandFluidParameters->DensityDissipate;
	SolverParameter.GravityScale = SceneProxy->PlandFluidParameters->GravityScale;
	SolverParameter.NoiseFrequency = SceneProxy->PlandFluidParameters->NoiseFrequency;
	SolverParameter.NoiseIntensity = SceneProxy->PlandFluidParameters->NoiseIntensity;
	SolverParameter.VelocityDissipate = SceneProxy->PlandFluidParameters->VelocityDissipate;
	SolverParameter.VorticityMult = SceneProxy->PlandFluidParameters->VorticityMult;
	SolverParameter.UseFFT = true;
	SetupSolverBaseParameters(SolverParameter.SolverBaseParameter,InView);

}

void FPhysical2DFluidSolver::PreRenderView_RenderThread(FRDGBuilder& GraphBuilder,FSceneView& InView)
{
	DECLARE_GPU_STAT(PlaneFluidSolver)
	RDG_EVENT_SCOPE(GraphBuilder, "PlaneFluidSolver");
	RDG_GPU_STAT_SCOPE(GraphBuilder, PlaneFluidSolver);

	for (UTextureRenderTarget* RT : SceneProxy->OutputTextures)
	{
		if (!RT || !RT->GetResource() || !RT->GetResource()->GetTextureRHI())
		{
			return;
		}
	}
	Frame++;
	const FRDGTextureRef SimulationTexture = RegisterExternalTexture(GraphBuilder, SceneProxy->OutputTextures[0]->GetResource()->GetTextureRHI(),TEXT("SimulationTexture"));
	const FRDGTextureRef PressureTexture = RegisterExternalTexture(GraphBuilder, SceneProxy->OutputTextures[1]->GetResource()->GetTextureRHI(),TEXT("PressureTexture"));


	//TODO:Need Change shader map based of platforms
	auto ShaderMap = GetGlobalShaderMap(SceneProxy->FeatureLevel);

	FRDGTextureDesc TempDesc = FRDGTextureDesc::Create2D(GridSize, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV | TexCreate_RenderTargetable);
	FRDGTextureRef TempGrid = GraphBuilder.CreateTexture(TempDesc,TEXT("TempGrid"), ERDGTextureFlags::None);
	FRDGTextureUAVRef SimUAV = GraphBuilder.CreateUAV(SimulationTexture);
	FRDGTextureUAVRef PressureUAV = GraphBuilder.CreateUAV(PressureTexture);

	FRDGTextureSRVRef SimSRV = nullptr;

	SimSRV = GraphBuilder.CreateSRV(SimulationTexture);

	ShaderPrint::FShaderPrintSetup ShaderPrintSetup(InView);

	FShaderPrintData ShaderPrintData = ShaderPrint::CreateShaderPrintData(GraphBuilder, ShaderPrintSetup);

	F2DFluidCS::FParameters* PassParameters = GraphBuilder.AllocParameters<F2DFluidCS::FParameters>();
	SetSolverParameter(PassParameters->FluidParameter,InView);

	auto SetParameter = [this,&GraphBuilder,ShaderPrintData,SimulationTexture](F2DFluidCS::FParameters* InPassParameters, bool bAdvectionDensity, int IterationIndex
	                                                 , FRDGTextureUAVRef SimUAV, FRDGTextureUAVRef PressureUAV, FRDGTextureSRVRef SimSRV, int ShaderType)
	{
		//InPassParameters->FluidParameter = *SolverParameter;

		InPassParameters->AdvectionDensity = bAdvectionDensity;
		InPassParameters->IterationIndex = IterationIndex;
		InPassParameters->FluidShaderType = ShaderType;
		InPassParameters->SimGridSRV = SimSRV;
		InPassParameters->SimSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		InPassParameters->SimGridUAV = SimUAV;
		InPassParameters->PressureGridUAV = PressureUAV;
		InPassParameters->bUseFFTPressure = bUseFFT;
		InPassParameters->WorldVelocity = FVector3f(0);//Context->WorldVelocity;
		InPassParameters->WorldPosition = FVector3f(0);//Context->WorldPosition;
		ShaderPrint::SetParameters(GraphBuilder, ShaderPrintData,InPassParameters->ShaderPrintUniformBuffer);
	};

	//PreVelocitySolver
	{
		SCOPE_CYCLE_COUNTER(STAT_PreVelocitySolver);

		SetParameter(PassParameters, false, 0, SimUAV, PressureUAV, SimSRV,  PreVel);
		//PermutationVector.Set<F2DFluidCS::FPreVel>(true);
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
		//F2DFluidCS::FParameters* PassParameters = GraphBuilder.AllocParameters<F2DFluidCS::FParameters>();

		//GraphBuilder.RHICmdList.Transition(FRHITransitionInfo(SimulationTexture->GetRHI(), ERHIAccess::UAVCompute, ERHIAccess::SRVCompute));

		if (RHIGetInterfaceType() < ERHIInterfaceType::D3D12)
		{
			AddCopyTexturePass(GraphBuilder, SimulationTexture, TempGrid);
			SimSRV = GraphBuilder.CreateSRV(TempGrid);
			//PressureSRV = GraphBuilder.CreateSRV(PressureTexture);
		}
		SetParameter(PassParameters, false, 0, SimUAV, PressureUAV, SimSRV,  Advection);
		TShaderMapRef<F2DFluidCS> ComputeShader(ShaderMap);

		/*
		GraphBuilder.AddPass(
						RDG_EVENT_NAME("AdvectionVelocity"),
						PassParameters,
						ERDGPassFlags::Compute ,
						[this ,ComputeShader,SimUAV,PassParameters,PressureUAV,SimulationTexture](FRHICommandList& RHICmdList)
					{
							RHICmdList.Transition(FRHITransitionInfo(SimulationTexture->GetRHI(), ERHIAccess::UAVCompute, ERHIAccess::SRVCompute));
							SimSRV = GraphBuilder.CreateSRV(SimulationTexture);


							FComputeShaderUtils::Dispatch(RHICmdList,
										 ComputeShader,
										 *PassParameters,
										 FComputeShaderUtils::GetGroupCount(FIntVector(GridSize.X, GridSize.Y, 1), ThreadNum));
					});*/

		FComputeShaderUtils::AddPass(GraphBuilder,
		                             RDG_EVENT_NAME("AdvectionVelocity"),
		                             ERDGPassFlags::Compute,
		                             ComputeShader,
		                             PassParameters,
		                             FComputeShaderUtils::GetGroupCount(FIntVector(GridSize.X, GridSize.Y, 1), ThreadNum));
	}


	if (bUseFFT)
	{
		//FRDGTextureUAVRef TempUAV =  GraphBuilder.CreateUAV(PressureTexture);
		{
			SCOPE_CYCLE_COUNTER(STAT_ComputeDivergence);
			//F2DFluidCS::FParameters* PassParameters = GraphBuilder.AllocParameters<F2DFluidCS::FParameters>();
			//AddCopyTexturePass(GraphBuilder,OutTextureArray[0],SimulationTexture);
			/*SimSRV = GraphBuilder.CreateSRV(SimulationTexture);
			PressureSRV = GraphBuilder.CreateSRV(PressureTexture);*/

			SetParameter(PassParameters, false, 0, SimUAV, PressureUAV, SimSRV,  ComputeDivergence);
			//PermutationVector.Set<F2DFluidCS::FAdvection>(true);
			TShaderMapRef<F2DFluidCS> ComputeShader(ShaderMap);


			FComputeShaderUtils::AddPass(GraphBuilder,
			                             RDG_EVENT_NAME("ComputeDivergence"),
			                             ERDGPassFlags::Compute,
			                             ComputeShader,
			                             PassParameters,
			                             FComputeShaderUtils::GetGroupCount(FIntVector(GridSize.X, GridSize.Y, 1), ThreadNum));
		}
		//FRDGTextureSRVRef DivSRV = GraphBuilder.CreateSRV(PressureTexture);
		//FFT Y
		FFFTSolvePoissonCS<256>::FParameters* FFTPassParameters = GraphBuilder.AllocParameters<FFFTSolvePoissonCS<256>::FParameters>();
		{
			SCOPE_CYCLE_COUNTER(STAT_FFTY);

			TShaderMapRef<FFFTSolvePoissonCS<256>> ComputeShader(ShaderMap);

			FFTPassParameters->bIsInverse = false;
			FFTPassParameters->bTransformX = false;
			FFTPassParameters->PressureGridUAV = PressureUAV;
			FFTPassParameters->BaseParameter = PassParameters->FluidParameter.SolverBaseParameter;

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
			FFTPassParameters->BaseParameter = PassParameters->FluidParameter.SolverBaseParameter;

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
			FFTPassParameters->BaseParameter = PassParameters->FluidParameter.SolverBaseParameter;

			FComputeShaderUtils::AddPass(GraphBuilder,
			                             RDG_EVENT_NAME("InvFFTX"),
			                             ERDGPassFlags::Compute,
			                             ComputeShader,
			                             FFTPassParameters,
			                             FIntVector(GridSize.Y, 1, 1)); //FComputeShaderUtils::GetGroupCount( FIntVector(GridSize.X,GridSize.Y,1),FIntVector(GridSize.X ,1,1)));
		}

		//Inv FFT Y
		{
			SCOPE_CYCLE_COUNTER(STAT_InvFFTY);

			TShaderMapRef<FFFTSolvePoissonCS<256>> ComputeShader(ShaderMap);
			FFTPassParameters->bIsInverse = true;
			FFTPassParameters->bTransformX = false;
			FFTPassParameters->PressureGridUAV = PressureUAV;
			FFTPassParameters->BaseParameter = PassParameters->FluidParameter.SolverBaseParameter;

			FComputeShaderUtils::AddPass(GraphBuilder,
			                             RDG_EVENT_NAME("InvFFTY"),
			                             ERDGPassFlags::Compute,
			                             ComputeShader,
			                             FFTPassParameters,
			                             FIntVector(GridSize.X, 1, 1)); //FComputeShaderUtils::GetGroupCount( FIntVector(GridSize.X,GridSize.Y,1),FIntVector(GridSize.X ,1,1)));
		}
	}
	else
	{
		for (int i = 0; i < 20; i++)
		{
			//F2DFluidCS::FParameters* PassParameters = GraphBuilder.AllocParameters<F2DFluidCS::FParameters>();

			SetParameter(PassParameters, false, i, SimUAV, PressureUAV, SimSRV,  IteratePressure);

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
		}
	}


	//Advection Density
	{
		SCOPE_CYCLE_COUNTER(STAT_AdvectionDensity);
		//F2DFluidCS::FParameters* PassParameters = GraphBuilder.AllocParameters<F2DFluidCS::FParameters>();

		//GraphBuilder.RHICmdList.Transition(FRHITransitionInfo(SimulationTexture->GetRHI(), ERHIAccess::UAVCompute, ERHIAccess::SRVCompute));
		if (RHIGetInterfaceType() < ERHIInterfaceType::D3D12)
		{
			AddCopyTexturePass(GraphBuilder, SimulationTexture, TempGrid);
			SimSRV = GraphBuilder.CreateSRV(TempGrid);
		}

		SetParameter(PassParameters, true, 0, SimUAV, PressureUAV, SimSRV,  Advection);
		TShaderMapRef<F2DFluidCS> ComputeShader(ShaderMap);


		FComputeShaderUtils::AddPass(GraphBuilder,
		                             RDG_EVENT_NAME("AdvectionDensity"),
		                             ERDGPassFlags::Compute,
		                             ComputeShader,
		                             PassParameters,
		                             FComputeShaderUtils::GetGroupCount(FIntVector(GridSize.X, GridSize.Y, 1), ThreadNum));
	}
}

void FPhysical2DFluidSolver::Initial(FRHICommandListImmediate& RHICmdList)
{
	Frame = 0;
	//SetParameter(Context->SolverParameter);
	InitialedDelegate.Broadcast();
}

void FPhysical2DFluidSolver::Release()
{
	FPhysicalSolverBase::Release();
}
