#include "PsychedelicSolver.h"



#include "PhysicalSimulationSceneProxy.h"
#include "PixelShaderUtils.h"
#include "RenderTargetPool.h"


const FIntVector ThreadNumber = FIntVector(8, 8, 1); //!bUseFFTSolverPressure? FIntVector(256,1,1):
 TAutoConsoleVariable<bool> CVarUseFFT(
	TEXT("r.UseFFT"),
	true,
	TEXT("Enables namespace filtering features in the Blueprint editor (experimental).")
);

class FPPPsychedelicPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FPPPsychedelicPS);
	SHADER_USE_PARAMETER_STRUCT(FPPPsychedelicPS, FGlobalShader);

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D,SimulationTexture)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
	SHADER_PARAMETER_SAMPLER(SamplerState,SimulationTextureSampler)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};


class FPPFluidCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FPPFluidCS);
	SHADER_USE_PARAMETER_STRUCT(FPPFluidCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)

		SHADER_PARAMETER(int, AdvectionDensity)
		SHADER_PARAMETER(int, IterationIndex)
		SHADER_PARAMETER(int, bUseFFTPressure)
		SHADER_PARAMETER(int, FluidShaderType)

		SHADER_PARAMETER_STRUCT_INCLUDE(FFluidParameter, FluidParameter)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
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
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), ThreadNumber.X);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), ThreadNumber.Y);
		OutEnvironment.SetDefine(TEXT("PREVELOCITY"), 0);
		OutEnvironment.SetDefine(TEXT("ADVECTION"), 1);

		OutEnvironment.SetDefine(TEXT("ITERATEPRESSURE"), 2);
		OutEnvironment.SetDefine(TEXT("COMPUTEDIVERGENCE"), 3);

		OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
	}
};

IMPLEMENT_GLOBAL_SHADER(FPPFluidCS, "/PluginShader/PPFluid/PPFluid.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FPPPsychedelicPS, "/PluginShader/PPFluid/PPFluidPixelShader.usf", "MainPS", SF_Pixel);

template <int GroupSize>
class FPPFFTSolvePoissonCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FPPFFTSolvePoissonCS);
	SHADER_USE_PARAMETER_STRUCT(FPPFFTSolvePoissonCS, FGlobalShader);

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
IMPLEMENT_SHADER_TYPE(template<>, FPPFFTSolvePoissonCS<512>, TEXT("/PluginShader/FFTSolvePoisson.usf"), TEXT("MainCS"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FPPFFTSolvePoissonCS<256>, TEXT("/PluginShader/FFTSolvePoisson.usf"), TEXT("MainCS"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FPPFFTSolvePoissonCS<128>, TEXT("/PluginShader/FFTSolvePoisson.usf"), TEXT("MainCS"), SF_Compute);



FPsychedelicSolver::FPsychedelicSolver(FPhysicalSimulationSceneProxy* InSceneProxy):FPhysicalSolverBase(InSceneProxy)
{
	GridSize = FIntPoint( InSceneProxy->GridSize.X,InSceneProxy->GridSize.Y);
	SceneProxy = InSceneProxy;
}

FPsychedelicSolver::~FPsychedelicSolver()
{
}

void FPsychedelicSolver::Initial_RenderThread(FRHICommandListImmediate& RHICmdList)
{
	/*FPooledRenderTargetDesc RGBADesc(FPooledRenderTargetDesc::Create2DDesc(GridSize, PF_FloatRGBA,
																			   FClearValueBinding(), TexCreate_None, TexCreate_ShaderResource | TexCreate_RenderTargetable | TexCreate_UAV, false));

	FPooledRenderTargetDesc FloatDesc(FPooledRenderTargetDesc::Create2DDesc(GridSize, PF_R32_FLOAT,
																			FClearValueBinding(), TexCreate_None, TexCreate_ShaderResource | TexCreate_RenderTargetable | TexCreate_UAV, false));
	GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, RGBADesc, SimulationTexturePool, TEXT("PsyChedelicSimulationTexture"));
	GRenderTargetPool.FindFreeElement(GraphBuilder .RHICmdList, FloatDesc, PressureTexturePool, TEXT("PsyChedelicPressureTexture"));*/
}

void FPsychedelicSolver::PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs)
{
	DECLARE_GPU_STAT(PlaneFluidSolver)
	RDG_EVENT_SCOPE(GraphBuilder, "PlaneFluidSolver");
	RDG_GPU_STAT_SCOPE(GraphBuilder, PlaneFluidSolver);

	Frame++;
	if(Frame == 1)
	{
		FPooledRenderTargetDesc RGBADesc(FPooledRenderTargetDesc::Create2DDesc((*Inputs.SceneTextures)->SceneColorTexture->Desc.Extent, PF_FloatRGBA,
																		   FClearValueBinding(), TexCreate_None, TexCreate_ShaderResource | TexCreate_RenderTargetable | TexCreate_UAV, false));

		FPooledRenderTargetDesc FloatDesc(FPooledRenderTargetDesc::Create2DDesc((*Inputs.SceneTextures)->SceneColorTexture->Desc.Extent, PF_R32_FLOAT,
																				FClearValueBinding(), TexCreate_None, TexCreate_ShaderResource | TexCreate_RenderTargetable | TexCreate_UAV, false));
		GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList,RGBADesc, SimulationTexturePool, TEXT("PsyChedelicSimulationTexture"));
		GRenderTargetPool.FindFreeElement(GraphBuilder .RHICmdList, FloatDesc, PressureTexturePool, TEXT("PsyChedelicPressureTexture"));
		AddClearUAVPass(GraphBuilder,GraphBuilder.CreateUAV(GraphBuilder.RegisterExternalTexture(SimulationTexturePool)),FLinearColor::Black);
		AddClearUAVPass(GraphBuilder,GraphBuilder.CreateUAV(GraphBuilder.RegisterExternalTexture(PressureTexturePool)),FLinearColor::Black);
	}
	bool bUseFFTSolverPressure = CVarUseFFT.GetValueOnAnyThread();
	FRDGTextureRef SimulationTexture = GraphBuilder.RegisterExternalTexture(SimulationTexturePool);
	FRDGTextureRef PressureTexture = GraphBuilder.RegisterExternalTexture(PressureTexturePool);
	//TODO:Need Change shader map based of platforms
	auto ShaderMap = GetGlobalShaderMap(View.FeatureLevel);

	FRDGTextureDesc TempDesc = FRDGTextureDesc::Create2D(Inputs.ViewFamilyTexture->Desc.Extent, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV | TexCreate_RenderTargetable);
	FRDGTextureRef TempGrid = GraphBuilder.CreateTexture(TempDesc,TEXT("TempGrid"));
	AddClearUAVPass(GraphBuilder,GraphBuilder.CreateUAV(TempGrid),FLinearColor::Black);
	FRDGTextureUAVRef PressureUAV = GraphBuilder.CreateUAV(PressureTexture);

	FFluidParameter FluidParameter;
	ShaderPrint::FShaderPrintSetup ShaderPrintSetup(View);
	FShaderPrintData ShaderPrintData = ShaderPrint::CreateShaderPrintData(GraphBuilder, ShaderPrintSetup);


	FluidParameter.DensityDissipate = SceneProxy->PlandFluidParameters->DensityDissipate;
	FluidParameter.GravityScale = SceneProxy->PlandFluidParameters->GravityScale;
	FluidParameter.NoiseFrequency = SceneProxy->PlandFluidParameters->NoiseFrequency;
	FluidParameter.NoiseIntensity = SceneProxy->PlandFluidParameters->NoiseIntensity;
	FluidParameter.VelocityDissipate = SceneProxy->PlandFluidParameters->VelocityDissipate;
	FluidParameter.VorticityMult = SceneProxy->PlandFluidParameters->VorticityMult;

	FluidParameter.UseFFT = true;

	FluidParameter.SolverBaseParameter.dt = 0.06;//SceneProxy->World->GetDeltaSeconds() * 2;
	FluidParameter.SolverBaseParameter.dx = *SceneProxy->Dx;
	FluidParameter.SolverBaseParameter.Time = Frame;
	FluidParameter.SolverBaseParameter.View = View.ViewUniformBuffer;
	FluidParameter.SolverBaseParameter.GridSize = FVector3f(SceneProxy->GridSize.X, SceneProxy->GridSize.Y, SceneProxy->GridSize.Z);
	FluidParameter.SolverBaseParameter.WarpSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
	FluidParameter.SimSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
	FluidParameter.WorldVelocity = FVector3f(0); //Context->WorldVelocity;
	FluidParameter.WorldPosition = FVector3f(0); //Context->WorldPosition;

	FRDGTextureRef PreVelocityTexture = GraphBuilder.CreateTexture(TempDesc,TEXT("PreVelocityTexture"));
	FRDGTextureRef AdvectionVelocityTexture = GraphBuilder.CreateTexture(TempDesc,TEXT("AdvectionVelocityTexture"));
	FRDGTextureRef AdvectionDensityTexture = GraphBuilder.CreateTexture(TempDesc,TEXT("AdvectionVelocityTexture"));
	//PreVelocitySolver
	{

		FPPFluidCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FPPFluidCS::FParameters>();
		PassParameters->FluidParameter = FluidParameter;
		PassParameters->FluidShaderType = PreVel;
		PassParameters->SimGridSRV = SimulationTexture;
		PassParameters->SimGridUAV = GraphBuilder.CreateUAV(PreVelocityTexture);//SimUAV;
		PassParameters->PressureGridUAV = GraphBuilder.CreateUAV(PressureTexture);
		PassParameters->SceneTexturesStruct = Inputs.SceneTextures;
		PassParameters->bUseFFTPressure = bUseFFTSolverPressure;

		PassParameters->AdvectionDensity = false;
		PassParameters->IterationIndex = 0;
		TShaderMapRef<FPPFluidCS> ComputeShader(ShaderMap);


		FComputeShaderUtils::AddPass(GraphBuilder,
		                             RDG_EVENT_NAME("PreVelocitySolver"),
		                             ERDGPassFlags::Compute,
		                             ComputeShader,
		                             PassParameters,
		                             FComputeShaderUtils::GetGroupCount(FIntVector(Inputs.ViewFamilyTexture->Desc.Extent.X, Inputs.ViewFamilyTexture->Desc.Extent.Y, 1), ThreadNumber));
	}



	//Advection Velocity
	{

		FPPFluidCS::FParameters* AdvectionPassParameters = GraphBuilder.AllocParameters<FPPFluidCS::FParameters>();
		//SetParameter(PassParameters, false, 0, SimUAV, PressureUAV, SimSRV, Advection);
		AdvectionPassParameters->AdvectionDensity = false;
		AdvectionPassParameters->IterationIndex = 0;
		AdvectionPassParameters->FluidParameter = FluidParameter;
		AdvectionPassParameters->PressureGridUAV = PressureUAV;
		AdvectionPassParameters->bUseFFTPressure = bUseFFTSolverPressure;
		AdvectionPassParameters->SceneTexturesStruct = Inputs.SceneTextures;
		AdvectionPassParameters->SimGridSRV = PreVelocityTexture;
		AdvectionPassParameters->SimGridUAV = GraphBuilder.CreateUAV(AdvectionVelocityTexture);
		AdvectionPassParameters->FluidShaderType = Advection;

		TShaderMapRef<FPPFluidCS> ComputeShader(ShaderMap);

		FComputeShaderUtils::AddPass(GraphBuilder,
		                             RDG_EVENT_NAME("AdvectionVelocity"),
		                             ERDGPassFlags::Compute,
		                             ComputeShader,
		                             AdvectionPassParameters,
		                             FComputeShaderUtils::GetGroupCount(FIntVector(Inputs.ViewFamilyTexture->Desc.Extent.X, Inputs.ViewFamilyTexture->Desc.Extent.Y, 1), ThreadNumber));
	}
	//Solver Pressure Fleid
	if (CVarUseFFT.GetValueOnAnyThread())
	{
		{

			//SetParameter(PassParameters, false, 0, SimUAV, PressureUAV, SimSRV, ComputeDivergence);
			//PermutationVector.Set<FPPFluidCS::FAdvection>(true);
			TShaderMapRef<FPPFluidCS> ComputeShader(ShaderMap);

			FPPFluidCS::FParameters* ComputeDivergenceParameters = GraphBuilder.AllocParameters<FPPFluidCS::FParameters>();
			//SetParameter(PassParameters, false, 0, SimUAV, PressureUAV, SimSRV, Advection);
			ComputeDivergenceParameters->AdvectionDensity = false;
			ComputeDivergenceParameters->IterationIndex = 0;
			ComputeDivergenceParameters->FluidParameter = FluidParameter;
			ComputeDivergenceParameters->PressureGridUAV = PressureUAV;
			ComputeDivergenceParameters->bUseFFTPressure = bUseFFTSolverPressure;
			ComputeDivergenceParameters->SceneTexturesStruct = Inputs.SceneTextures;
			ComputeDivergenceParameters->SimGridSRV = AdvectionVelocityTexture;
			ComputeDivergenceParameters->SimGridUAV = GraphBuilder.CreateUAV(SimulationTexture);
			ComputeDivergenceParameters->FluidShaderType = ComputeDivergence;
			FComputeShaderUtils::AddPass(GraphBuilder,
			                             RDG_EVENT_NAME("ComputeDivergence"),
			                             ERDGPassFlags::Compute,
			                             ComputeShader,
			                             ComputeDivergenceParameters,
		                             FComputeShaderUtils::GetGroupCount(FIntVector(Inputs.ViewFamilyTexture->Desc.Extent.X, Inputs.ViewFamilyTexture->Desc.Extent.Y, 1), ThreadNumber));
		}

		FRDGTextureDesc TempPressureDesc = FRDGTextureDesc::Create2D(GridSize, PF_R32_FLOAT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV | TexCreate_RenderTargetable);
		FRDGTextureRef TempPressureGrid1 = GraphBuilder.CreateTexture(TempPressureDesc,TEXT("TempPressure1"));
		FRDGTextureRef TempPressureGrid2 = GraphBuilder.CreateTexture(TempPressureDesc,TEXT("TempPressure2"));
		FRDGTextureRef TempPressureGrid3 = GraphBuilder.CreateTexture(TempPressureDesc,TEXT("TempPressure3"));
		//FFT Y
		/*{
			FPPFFTSolvePoissonCS<256>::FParameters* FFTPassParameters = GraphBuilder.AllocParameters<FPPFFTSolvePoissonCS<256>::FParameters>();

			TShaderMapRef<FPPFFTSolvePoissonCS<256>> ComputeShader(ShaderMap);

			FFTPassParameters->bIsInverse = false;
			FFTPassParameters->bTransformX = false;
			//AddCopyTexturePass(GraphBuilder,PressureTexture,TempPressureGrid);
			FFTPassParameters->InPressureSRV = PressureTexture;
			FFTPassParameters->PressureGridUAV =GraphBuilder.CreateUAV(TempPressureGrid1);
			FFTPassParameters->BaseParameter = FluidParameter.SolverBaseParameter;

			FComputeShaderUtils::AddPass(GraphBuilder,
			                             RDG_EVENT_NAME("FFTY"),
			                             ERDGPassFlags::Compute,
			                             ComputeShader,
			                             FFTPassParameters,
			                             FIntVector(GridSize.X -1 , 1, 1)); //FComputeShaderUtils::GetGroupCount( FIntVector(GridSize.X,GridSize.Y,1),FIntVector(GridSize.X ,1,1)));
		}

		//FFT X
		{
			//FPPFFTSolvePoissonCS<256>::FParameters* PassParameters = GraphBuilder.AllocParameters<FPPFFTSolvePoissonCS<256>::FParameters>();
			TShaderMapRef<FPPFFTSolvePoissonCS<256>> ComputeShader(ShaderMap);
			//AddCopyTexturePass(GraphBuilder,PressureTexture,TempPressureGrid);
			FPPFFTSolvePoissonCS<256>::FParameters* FFTPassParameters = GraphBuilder.AllocParameters<FPPFFTSolvePoissonCS<256>::FParameters>();
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

			TShaderMapRef<FPPFFTSolvePoissonCS<256>> ComputeShader(ShaderMap);
			FPPFFTSolvePoissonCS<256>::FParameters* FFTPassParameters = GraphBuilder.AllocParameters<FPPFFTSolvePoissonCS<256>::FParameters>();
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

			TShaderMapRef<FPPFFTSolvePoissonCS<256>> ComputeShader(ShaderMap);
			FPPFFTSolvePoissonCS<256>::FParameters* FFTPassParameters = GraphBuilder.AllocParameters<FPPFFTSolvePoissonCS<256>::FParameters>();
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
		/*for (int i = 0; i < 20; i++)
		{
			FPPFluidCS::FParameters* PressurePassParameters = GraphBuilder.AllocParameters<FPPFluidCS::FParameters>();
			//SetParameter(PassParameters, false, 0, SimUAV, PressureUAV, SimSRV, Advection);
			PressurePassParameters->AdvectionDensity = false;
			PressurePassParameters->IterationIndex = 0;
			PressurePassParameters->FluidParameter = FluidParameter;
			PressurePassParameters->PressureGridUAV = PressureUAV;
			PressurePassParameters->bUseFFTPressure = bUseFFTSolverPressure;
			PressurePassParameters->SceneTexturesStruct = Inputs.SceneTextures;

			PressurePassParameters->SimGridSRV = PreVelocityTexture;
			PressurePassParameters->SimGridUAV = GraphBuilder.CreateUAV(AdvectionVelocityTexture);
			PressurePassParameters->FluidShaderType = Advection;

			TShaderMapRef<FPPFluidCS> ComputeShader(ShaderMap);


			FComputeShaderUtils::AddPass(GraphBuilder,
			                             RDG_EVENT_NAME("iteratePressure"),
			                             ERDGPassFlags::Compute,
			                             ComputeShader,
			                             PressurePassParameters,
			                             FComputeShaderUtils::GetGroupCount(FIntVector(GridSize.X, GridSize.Y, 1), ThreadNumber));
			//AddCopyTexturePass(GraphBuilder,TempGrid,OutTextureArray[1]);
		}*/
	}


	//Advection Density
	{

		if (RHIGetInterfaceType() < ERHIInterfaceType::D3D12)
		{
			AddCopyTexturePass(GraphBuilder, SimulationTexture, TempGrid);
			//SimSRV = GraphBuilder.CreateSRV(TempGrid);
		}
		FPPFluidCS::FParameters* AdvectionDensityPassParameters = GraphBuilder.AllocParameters<FPPFluidCS::FParameters>();

		//SetParameter(PassParameters, false, 0, SimUAV, PressureUAV, SimSRV, Advection);
		AdvectionDensityPassParameters->AdvectionDensity = true;
		AdvectionDensityPassParameters->IterationIndex = 0;
		AdvectionDensityPassParameters->FluidParameter = FluidParameter;
		AdvectionDensityPassParameters->PressureGridUAV = PressureUAV;
		AdvectionDensityPassParameters->bUseFFTPressure = bUseFFTSolverPressure;
		AdvectionDensityPassParameters->SceneTexturesStruct = Inputs.SceneTextures;

		AdvectionDensityPassParameters->SimGridSRV = AdvectionVelocityTexture;
		AdvectionDensityPassParameters->SimGridUAV = GraphBuilder.CreateUAV(AdvectionDensityTexture);
		AdvectionDensityPassParameters->FluidShaderType = Advection;
		//SetParameter(PassParameters, true, 0, SimUAV, PressureUAV, SimSRV, Advection);
		TShaderMapRef<FPPFluidCS> ComputeShader(ShaderMap);


		FComputeShaderUtils::AddPass(GraphBuilder,
		                             RDG_EVENT_NAME("AdvectionDensity"),
		                             ERDGPassFlags::Compute,
		                             ComputeShader,
		                             AdvectionDensityPassParameters,
		                             FComputeShaderUtils::GetGroupCount(FIntVector(Inputs.ViewFamilyTexture->Desc.Extent.X, Inputs.ViewFamilyTexture->Desc.Extent.Y, 1), ThreadNumber));
		SimulationTexturePool =  GraphBuilder.ConvertToExternalTexture(AdvectionDensityTexture);
	}

	TShaderMapRef<FPPPsychedelicPS> PixelShader(ShaderMap);
	FPPPsychedelicPS::FParameters* PixelShaderParameters = GraphBuilder.AllocParameters<FPPPsychedelicPS::FParameters>();
	FRDGTextureRef SceneColor = GraphBuilder.CreateTexture((*Inputs.SceneTextures)->SceneColorTexture->Desc,TEXT("PsychedelicInSceneColor"));
	AddCopyTexturePass(GraphBuilder,(*Inputs.SceneTextures)->SceneColorTexture,SceneColor);
	PixelShaderParameters->View = View.ViewUniformBuffer;
	PixelShaderParameters->RenderTargets[0] = FRenderTargetBinding((*Inputs.SceneTextures)->SceneColorTexture, ERenderTargetLoadAction::ENoAction);
	PixelShaderParameters->SceneTexturesStruct = Inputs.SceneTextures;
	PixelShaderParameters->SimulationTexture = AdvectionDensityTexture;
	PixelShaderParameters->SimulationTextureSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
	FPixelShaderUtils::AddFullscreenPass(
				GraphBuilder,
				ShaderMap,
				RDG_EVENT_NAME("PostProcessPsychedelic"),
				PixelShader,
				PixelShaderParameters,
				View.UnconstrainedViewRect);
}
