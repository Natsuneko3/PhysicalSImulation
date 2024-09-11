#include "PsychedelicSolver.h"
#include "PixelShaderUtils.h"
#include "RenderTargetPool.h"
#include "SceneTexturesConfig.h"
#include "SceneView.h"
#include "TextureResource.h"
#include "ShaderPrintParameters.h"
#include "Fluid/FluidCommon.h"


const FIntVector ThreadNumber = FIntVector(8, 8, 1); //!bUseFFTSolverPressure? FIntVector(256,1,1):
TAutoConsoleVariable<bool> CVarUseFFT(
	TEXT("r.UseFFT"),
	true,
	TEXT("Enables namespace filtering features in the Blueprint editor (experimental).")
);

enum EShadertype
{
	PreVel,
	Advection,
	IteratePressure,
	ComputeDivergence
};

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
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SimulationTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ColorTexture)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
		SHADER_PARAMETER_STRUCT_INCLUDE(ShaderPrint::FShaderParameters, ShaderPrintParameters)
		SHADER_PARAMETER_SAMPLER(SamplerState, SimulationTextureSampler)
		SHADER_PARAMETER_SAMPLER(SamplerState, ColorTextureSampler)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};


class FPPFluidCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FPPFluidCS);
	SHADER_USE_PARAMETER_STRUCT(FPPFluidCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
	SHADER_PARAMETER_STRUCT_INCLUDE(ShaderPrint::FShaderParameters, ShaderPrintUniformBuffer)
	SHADER_PARAMETER_STRUCT_INCLUDE(FFluidParameter, FluidParameter)
		SHADER_PARAMETER(int, AdvectionDensity)
		SHADER_PARAMETER(int, IterationIndex)
		SHADER_PARAMETER(int, bUseFFTPressure)
		SHADER_PARAMETER(int, FluidShaderType)
		SHADER_PARAMETER(int, DownScale)

		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SimGridSRV)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D,TranslucencyTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, SimGridUAV)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, FluidColorUAV)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);;
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

IMPLEMENT_GLOBAL_SHADER(FPPFluidCS, "/Plugin/CustomRenderFeature/PPFluid/PPFluid.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FPPPsychedelicPS, "/Plugin/CustomRenderFeature/PPFluid/PPFluidPixelShader.usf", "MainPS", SF_Pixel);

UPsychedelicSolver::UPsychedelicSolver()
{
}

UPsychedelicSolver::~UPsychedelicSolver()
{
}

void UPsychedelicSolver::Initial_RenderThread(FRHICommandListImmediate& RHICmdList)
{
}

void UPsychedelicSolver::PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs)
{
	if (View.UnconstrainedViewRect.Area() < 2066280 || !PlandFluidParameters.InTexture1)
	{
		return;
	}
	int DownScale = 1;
	DECLARE_GPU_STAT(PlaneFluidSolver)
	RDG_EVENT_SCOPE(GraphBuilder, "PlaneFluidSolver");
	RDG_GPU_STAT_SCOPE(GraphBuilder, PlaneFluidSolver);

	FIntPoint TextureSize = (*Inputs.SceneTextures)->SceneColorTexture->Desc.Extent / DownScale;
	Frame++;
	if (Frame == 1)
	{
		FPooledRenderTargetDesc RGBADesc(FPooledRenderTargetDesc::Create2DDesc(TextureSize, PF_FloatRGBA,
		                                                                       FClearValueBinding(), TexCreate_None, TexCreate_ShaderResource | TexCreate_RenderTargetable | TexCreate_UAV, false));

		FPooledRenderTargetDesc FloatDesc(FPooledRenderTargetDesc::Create2DDesc((*Inputs.SceneTextures)->SceneColorTexture->Desc.Extent, (*Inputs.SceneTextures)->SceneColorTexture->Desc.Format,
		                                                                        FClearValueBinding(), TexCreate_None, TexCreate_ShaderResource | TexCreate_RenderTargetable | TexCreate_UAV, false));
		GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, RGBADesc, SimulationTexturePool, TEXT("PsyChedelicSimulationTexture"));
		GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, FloatDesc, PressureTexturePool, TEXT("PsyChedelicPressureTexture"));
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(GraphBuilder.RegisterExternalTexture(SimulationTexturePool)), FLinearColor::Black);
		AddCopyTexturePass(GraphBuilder,(*Inputs.SceneTextures)->SceneColorTexture,GraphBuilder.RegisterExternalTexture(PressureTexturePool));
	}
	bool bUseFFTSolverPressure = CVarUseFFT.GetValueOnAnyThread();
	FRDGTextureRef SimulationTexture = GraphBuilder.RegisterExternalTexture(SimulationTexturePool);
	FRDGTextureRef FluidColorTexture = GraphBuilder.RegisterExternalTexture(PressureTexturePool);
	auto ShaderMap = GetGlobalShaderMap(View.FeatureLevel);
	FRDGTextureDesc TempDesc = FRDGTextureDesc::Create2D(TextureSize, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV | TexCreate_RenderTargetable);

	FRDGTextureUAVRef FluidColorTextureUAV = GraphBuilder.CreateUAV(FluidColorTexture);
	const FViewInfo& ViewInfo = static_cast<const FViewInfo&>(View);
	FFluidParameter FluidParameter;
	ShaderPrint::FShaderPrintSetup ShaderPrintSetup(View);
	FShaderPrintData ShaderPrintData = ShaderPrint::CreateShaderPrintData(GraphBuilder, ShaderPrintSetup);

	FluidParameter.DensityDissipate = PlandFluidParameters.DensityDissipate;
	FluidParameter.GravityScale = PlandFluidParameters.GravityScale;
	FluidParameter.NoiseFrequency = PlandFluidParameters.NoiseFrequency;
	FluidParameter.NoiseIntensity = PlandFluidParameters.NoiseIntensity;
	FluidParameter.VelocityDissipate = PlandFluidParameters.VelocityDissipate;
	FluidParameter.VorticityMult = PlandFluidParameters.VorticityMult;

	FluidParameter.UseFFT = true;

	FluidParameter.SolverBaseParameter.dt = 0.06; //World->GetDeltaSeconds() * 2;
	FluidParameter.SolverBaseParameter.dx = 1;
	FluidParameter.SolverBaseParameter.Time = Frame;
	FluidParameter.SolverBaseParameter.View = View.ViewUniformBuffer;
	FluidParameter.SolverBaseParameter.GridSize = FVector3f(TextureSize, 1);
	FluidParameter.SolverBaseParameter.WarpSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
	FluidParameter.SimSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
	FluidParameter.WorldVelocity = FVector3f(0); //Context->WorldVelocity;
	FluidParameter.WorldPosition = FVector3f(0); //Context->WorldPosition;
	FluidParameter.InTexture = PlandFluidParameters.InTexture1->GetResource()->GetTextureRHI();
	FluidParameter.InTextureSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
	FRDGTextureRef PreVelocityTexture = GraphBuilder.CreateTexture(TempDesc,TEXT("PreVelocityTexture"));
	FRDGTextureRef TranslucencyTexture = Inputs.TranslucencyViewResourcesMap.Get(ETranslucencyPass::TPT_TranslucencyAfterDOF).ColorTexture.Target;
	if(!TranslucencyTexture)
	{
		TranslucencyTexture = GraphBuilder.RegisterExternalTexture(GSystemTextures.BlackDummy);
	}
	//PreVelocitySolver
	{
		FPPFluidCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FPPFluidCS::FParameters>();
		PassParameters->FluidParameter = FluidParameter;
		PassParameters->FluidShaderType = PreVel;
		PassParameters->SimGridSRV = SimulationTexture;
		PassParameters->TranslucencyTexture = TranslucencyTexture;
		PassParameters->SimGridUAV = GraphBuilder.CreateUAV(PreVelocityTexture); //SimUAV;
		PassParameters->FluidColorUAV = GraphBuilder.CreateUAV(FluidColorTexture);
		PassParameters->SceneTexturesStruct = Inputs.SceneTextures;
		PassParameters->DownScale = DownScale;

		PassParameters->bUseFFTPressure = bUseFFTSolverPressure;

		PassParameters->AdvectionDensity = false;
		PassParameters->IterationIndex = 0;
		TShaderMapRef<FPPFluidCS> ComputeShader(ShaderMap);


		FComputeShaderUtils::AddPass(GraphBuilder,
		                             RDG_EVENT_NAME("PreVelocitySolver"),
		                             ERDGPassFlags::Compute,
		                             ComputeShader,
		                             PassParameters,
		                             FComputeShaderUtils::GetGroupCount(FIntVector(TextureSize.X,TextureSize.Y, 1), ThreadNumber));
	}

	//Advection Velocity
	{
		FPPFluidCS::FParameters* AdvectionPassParameters = GraphBuilder.AllocParameters<FPPFluidCS::FParameters>();
		//SetParameter(PassParameters, false, 0, SimUAV, PressureUAV, SimSRV, Advection);
		AdvectionPassParameters->AdvectionDensity = false;
		AdvectionPassParameters->IterationIndex = 0;
		AdvectionPassParameters->FluidParameter = FluidParameter;
		AdvectionPassParameters->FluidColorUAV = FluidColorTextureUAV;
		AdvectionPassParameters->bUseFFTPressure = bUseFFTSolverPressure;
		AdvectionPassParameters->SceneTexturesStruct = Inputs.SceneTextures;
		AdvectionPassParameters->TranslucencyTexture = TranslucencyTexture;
		AdvectionPassParameters->SimGridSRV = PreVelocityTexture;
		AdvectionPassParameters->SimGridUAV = GraphBuilder.CreateUAV(SimulationTexture);
		AdvectionPassParameters->FluidShaderType = Advection;
		AdvectionPassParameters->DownScale = DownScale;
		TShaderMapRef<FPPFluidCS> ComputeShader(ShaderMap);

		FComputeShaderUtils::AddPass(GraphBuilder,
		                             RDG_EVENT_NAME("AdvectionVelocity"),
		                             ERDGPassFlags::Compute,
		                             ComputeShader,
		                             AdvectionPassParameters,
		                             FComputeShaderUtils::GetGroupCount(FIntVector(TextureSize.X,TextureSize.Y, 1), ThreadNumber));
	}
	FRDGTextureRef BlurColorTexture = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(TextureSize / 2,PF_FloatR11G11B10,FClearValueBinding::Black,TexCreate_ShaderResource | TexCreate_UAV),TEXT("BlurColorTexture"));
	FBlurParameter BilateralParameter;
	AddTextureBlurPass(GraphBuilder,ViewInfo,FluidColorTexture,BlurColorTexture,BilateralParameter);
	//SimulationTexturePool = GraphBuilder.ConvertToExternalTexture(AdvectionDensityTexture);

	TShaderMapRef<FPPPsychedelicPS> PixelShader(ShaderMap);
	FPPPsychedelicPS::FParameters* PixelShaderParameters = GraphBuilder.AllocParameters<FPPPsychedelicPS::FParameters>();

	PixelShaderParameters->View = View.ViewUniformBuffer;
	PixelShaderParameters->RenderTargets[0] = FRenderTargetBinding((*Inputs.SceneTextures)->SceneColorTexture, ERenderTargetLoadAction::ENoAction);
	PixelShaderParameters->SceneTexturesStruct = Inputs.SceneTextures;
	PixelShaderParameters->SimulationTexture = SimulationTexture;
	PixelShaderParameters->SimulationTextureSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
	PixelShaderParameters->ColorTexture = BlurColorTexture;//PlandFluidParameters.InTexture1->GetResource()->GetTextureRHI();
	PixelShaderParameters->ColorTextureSampler = TStaticSamplerState<SF_Bilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();

	ShaderPrint::SetParameters(GraphBuilder, ViewInfo.ShaderPrintData, PixelShaderParameters->ShaderPrintParameters);

	FPixelShaderUtils::AddFullscreenPass(
		GraphBuilder,
		ShaderMap,
		RDG_EVENT_NAME("PostProcessPsychedelic"),
		PixelShader,
		PixelShaderParameters,
		View.UnconstrainedViewRect);
}
