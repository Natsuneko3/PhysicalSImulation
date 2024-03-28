#include "Physical2DFluidSolver.h"


#include "PhysicalSolver.h"

#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "ShaderParameterStruct.h"
#include "Engine/TextureRenderTarget2D.h"

enum EShadertype
{
	PreVel ,
	Advection,
	IteratePressure,
	ComputeDivergence
};

const bool bUseFFT = true;
const FIntVector ThreadNum = FIntVector(8,8,1);//!bUseFFT? FIntVector(256,1,1):

template <int GroupSize>
class FFFTSolvePoissonCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FFFTSolvePoissonCS);
	SHADER_USE_PARAMETER_STRUCT(FFFTSolvePoissonCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		//SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D,FFTSolvePoissonSRV)
	SHADER_PARAMETER_STRUCT_INCLUDE(FSolverBaseParameter, BaseParameter)
		SHADER_PARAMETER(int,bIsInverse)
		SHADER_PARAMETER(int,bTransformX)
	    SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, PressureGridUAV)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;//IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
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

	// class FInitial	  : SHADER_PERMUTATION_BOOL("INIT");
	// class FPreVel	  : SHADER_PERMUTATION_BOOL("PREVELOCITY");
	// class FIteratePressure	  : SHADER_PERMUTATION_BOOL("ITERATEPRESSURE");
	// class FFFTPressure	  : SHADER_PERMUTATION_BOOL("FFTSOLVERPRESSUREX");
	//
	// class FAdvection	  : SHADER_PERMUTATION_BOOL("ADVECTION");
	// using FPermutationDomain = TShaderPermutationDomain<FInitial,FPreVel,FAdvection,FIteratePressure
	// ,FFFTPressure>;
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )

		SHADER_PARAMETER(int, AdvectionDensity)
		SHADER_PARAMETER(int, IterationIndex)
		SHADER_PARAMETER(int, bUseFFTPressure)
		SHADER_PARAMETER(int, FluidShaderType)

		SHADER_PARAMETER_STRUCT_INCLUDE(FFluidParameter,FluidParameter)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, SimGridSRV)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, PressureGridSRV)
	    SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, SimGridUAV)
		SHADER_PARAMETER_SAMPLER(SamplerState, SimSampler)

		SHADER_PARAMETER(FVector3f,WorldVelocity)
		SHADER_PARAMETER(FVector3f,WorldPosition)

		
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, PressureGridUAV)
	END_SHADER_PARAMETER_STRUCT()

	 static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	 {
	
	 	FPermutationDomain PermutationVector(Parameters.PermutationId);

		// Only generate one permutation for each define.
		// uint32 DefineEnabled = 0;
		// DefineEnabled += PermutationVector.Get<FInitial>()		== true ? 1 : 0;
		// DefineEnabled += PermutationVector.Get<FPreVel>()		== true ? 1 : 0;
		// DefineEnabled += PermutationVector.Get<FAdvection>()		== true ? 1 : 0;
		// DefineEnabled += PermutationVector.Get<FIteratePressure>()		== true ? 1 : 0;
		// DefineEnabled += PermutationVector.Get<FFFTPressure>()		== true ? 1 : 0;
		// return DefineEnabled == 1;
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
IMPLEMENT_SHADER_TYPE(template<>,FFFTSolvePoissonCS<256>, TEXT("/PluginShader/FFTSolvePoisson.usf"), TEXT("MainCS"), SF_Compute);

FPhysical2DFluidSolver::FPhysical2DFluidSolver()
{
	Frame = 0;
	GridSize = FIntPoint(127);
}

void FPhysical2DFluidSolver::SetParameter(FSolverParameter* InParameter)
{
	GridSize =  FIntPoint(InParameter->FluidParameter.SolverBaseParameter.GridSize.X,
		InParameter->FluidParameter.SolverBaseParameter.GridSize.Y) - 1;

	SolverParameter = &InParameter->FluidParameter;
}

void FPhysical2DFluidSolver::Update_RenderThread(FRDGBuilder& GraphBuilder, FPhysicalSolverContext* Context)
{
	DECLARE_GPU_STAT(PlaneFluidSolver)
	RDG_EVENT_SCOPE(GraphBuilder, "PlaneFluidSolver");
	RDG_GPU_STAT_SCOPE(GraphBuilder, PlaneFluidSolver);
	if(!SimulatorTextureRT&&!PressureTextureRT)
	{

	}
	/*FRDGTextureDesc SimGridDesc = FRDGTextureDesc::Create2D(GridSize,PF_FloatRGBA,FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV | TexCreate_RenderTargetable);
	 =  GraphBuilder.CreateTexture(SimGridDesc,TEXT("SimGrid"),ERDGTextureFlags::MultiFrame);
	FRDGTextureDesc PressureGridDesc =  FRDGTextureDesc::Create2D(GridSize,PF_R16F,FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV | TexCreate_RenderTargetable);
	 =  GraphBuilder.CreateTexture(PressureGridDesc,TEXT("PressureGrid"),ERDGTextureFlags::MultiFrame);*/
	FRDGTextureRef SimulationTexture;
	FRDGTextureRef PressureTexture;

	/*if(0)//Context->OutputTextures[0]&&Context->OutputTextures[0]->GetResource()&&Context->OutputTextures[0]->GetResource()->GetTextureRHI())
	{
		SimulatorTextureRT = CreateRenderTarget(Context->OutputTextures[0]->GetResource()->GetTextureRHI(), TEXT("DensityTexture"));
		SimulationTexture= GraphBuilder.RegisterExternalTexture(SimulatorTextureRT);
	}
	else
	{
		FRDGTextureDesc SimGridDesc = FRDGTextureDesc::Create2D(GridSize,PF_FloatRGBA,FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV | TexCreate_RenderTargetable);
		SimulationTexture =  GraphBuilder.CreateTexture(SimGridDesc,TEXT("DensityTexture"),ERDGTextureFlags::MultiFrame);
	}
	
	if(0)//Context->OutputTextures[1]&&Context->OutputTextures[1]->GetResource()&&Context->OutputTextures[1]->GetResource()->GetTextureRHI())
	{
		PressureTextureRT = CreateRenderTarget(Context->OutputTextures[1]->GetResource()->GetTextureRHI(), TEXT("PressureTexture"));
		PressureTexture = GraphBuilder.RegisterExternalTexture(PressureTextureRT);
	}
	else
	{
		FRDGTextureDesc SimGridDesc = FRDGTextureDesc::Create2D(GridSize,PF_R16F,FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV | TexCreate_RenderTargetable);
		PressureTexture =  GraphBuilder.CreateTexture(SimGridDesc,TEXT("PressureTexture"),ERDGTextureFlags::MultiFrame);
	}*/
	if(!Context->OutputTextures[0]->GetResource())
	{
		return;
	}
	SimulationTexture = RegisterExternalTexture(GraphBuilder,Context->OutputTextures[0]->GetResource()->GetTextureRHI(),TEXT("SimulationTexture"));
	PressureTexture = RegisterExternalTexture(GraphBuilder,Context->OutputTextures[1]->GetResource()->GetTextureRHI(),TEXT("SimulationTexture"));
	Frame++;
	SolverParameter->SolverBaseParameter.Time = Frame;
	//TODO:Need Change shader map based of platforms
	auto ShaderMap = GetGlobalShaderMap(Context->FeatureLevel);

	FRDGTextureDesc TempDesc =  FRDGTextureDesc::Create2D(GridSize,PF_FloatRGBA,FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV | TexCreate_RenderTargetable);
	FRDGTextureRef TempGrid =  GraphBuilder.CreateTexture(TempDesc,TEXT("TempGrid"),ERDGTextureFlags::None);

	/*AddClearRenderTargetPass(GraphBuilder,SimulationTexture);
	AddClearRenderTargetPass(GraphBuilder,PressureTexture);*/


	FRDGTextureUAVRef SimUAV =  GraphBuilder.CreateUAV(SimulationTexture);
	FRDGTextureUAVRef PressureUAV =  GraphBuilder.CreateUAV(PressureTexture);

	FRDGTextureSRVRef SimSRV = nullptr;
	FRDGTextureSRVRef PressureSRV = nullptr;
	SimSRV = GraphBuilder.CreateSRV(SimulationTexture);
	PressureSRV = GraphBuilder.CreateSRV(PressureTexture);
	//AddClearRenderTargetPass(GraphBuilder,PressureTexture);
	
	auto SetParameter = [this,&GraphBuilder,Context](F2DFluidCS::FParameters* InPassParameters,bool bAdvectionDensity,int IterationIndex
		,FRDGTextureUAVRef SimUAV,FRDGTextureUAVRef PressureUAV,FRDGTextureSRVRef SimSRV,FRDGTextureSRVRef PressureSRV,int ShaderType)
	{
		
		InPassParameters->FluidParameter = *SolverParameter;
		InPassParameters->AdvectionDensity = bAdvectionDensity;
		InPassParameters->IterationIndex = IterationIndex;
		InPassParameters->FluidShaderType = ShaderType  ;
		//UE_LOG(LogTemp,Log,TEXT("shadertype%i"),ShaderType);
		InPassParameters->SimGridSRV = SimSRV;//? SimSRV : GraphBuilder.CreateSRV(SimulationTexture);
		InPassParameters->PressureGridSRV = PressureSRV;//? PressureSRV : GraphBuilder.CreateSRV(OutTextureArray[1]);
		InPassParameters->SimSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		InPassParameters->SimGridUAV = SimUAV;//? SimUAV : GraphBuilder.CreateUAV(OutTextureArray[0]);
		InPassParameters->PressureGridUAV = PressureUAV;//? PressureUAV : GraphBuilder.CreateUAV(OutTextureArray[1]);
		InPassParameters->bUseFFTPressure = bUseFFT;
		InPassParameters->WorldVelocity =Context->WorldVelocity;
		InPassParameters->WorldPosition =Context->WorldPosition;

		
	};

	//PreVelocitySolver
	{
		F2DFluidCS::FParameters* PassParameters = GraphBuilder.AllocParameters<F2DFluidCS::FParameters>();

		SetParameter(PassParameters,false,0, SimUAV, PressureUAV, SimSRV, PressureSRV,PreVel);

		//PermutationVector.Set<F2DFluidCS::FPreVel>(true);
		TShaderMapRef<F2DFluidCS> ComputeShader(ShaderMap);

	
		FComputeShaderUtils::AddPass(GraphBuilder,
			RDG_EVENT_NAME("PreVelocitySolver"),
			ERDGPassFlags::Compute,
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(FIntVector(GridSize.X, GridSize.Y, 1),ThreadNum) );
	}


	//Advection Velocity
	{

		F2DFluidCS::FParameters* PassParameters = GraphBuilder.AllocParameters<F2DFluidCS::FParameters>();
		//GraphBuilder.RHICmdList.Transition(FRHITransitionInfo(SimulationTexture->GetRHI(), ERHIAccess::UAVCompute, ERHIAccess::SRVCompute));

		if(RHIGetInterfaceType() <ERHIInterfaceType::D3D12)
		{
			AddCopyTexturePass(GraphBuilder,SimulationTexture,TempGrid);
			SimSRV = GraphBuilder.CreateSRV(TempGrid);
			PressureSRV = GraphBuilder.CreateSRV(PressureTexture);
		}


		SetParameter(PassParameters,false,0, SimUAV, PressureUAV, SimSRV, PressureSRV,Advection);


		//PermutationVector.Set<F2DFluidCS::FAdvection>(true);
		TShaderMapRef<F2DFluidCS> ComputeShader(ShaderMap);


		FComputeShaderUtils::AddPass(GraphBuilder,
			RDG_EVENT_NAME("AdvectionVelocity"),
			ERDGPassFlags::Compute,
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(FIntVector(GridSize.X, GridSize.Y, 1),ThreadNum) );
	}
	

	if(bUseFFT)
	{
		//FRDGTextureUAVRef TempUAV =  GraphBuilder.CreateUAV(PressureTexture);
		{

			F2DFluidCS::FParameters* PassParameters = GraphBuilder.AllocParameters<F2DFluidCS::FParameters>();
			//AddCopyTexturePass(GraphBuilder,OutTextureArray[0],SimulationTexture);
			/*SimSRV = GraphBuilder.CreateSRV(SimulationTexture);
			PressureSRV = GraphBuilder.CreateSRV(PressureTexture);*/

			SetParameter(PassParameters,false,0, SimUAV, PressureUAV, SimSRV, PressureSRV,ComputeDivergence);


			//PermutationVector.Set<F2DFluidCS::FAdvection>(true);
			TShaderMapRef<F2DFluidCS> ComputeShader(ShaderMap);


			FComputeShaderUtils::AddPass(GraphBuilder,
				RDG_EVENT_NAME("ComputeDivergence"),
				ERDGPassFlags::Compute,
				ComputeShader,
				PassParameters,
				FComputeShaderUtils::GetGroupCount(FIntVector(GridSize.X, GridSize.Y, 1),ThreadNum) );
		}
		//FRDGTextureSRVRef DivSRV = GraphBuilder.CreateSRV(PressureTexture);
		//FFT Y
		{
			FFFTSolvePoissonCS<256>::FParameters* PassParameters = GraphBuilder.AllocParameters<FFFTSolvePoissonCS<256>::FParameters>();
			TShaderMapRef<FFFTSolvePoissonCS<256>> ComputeShader(ShaderMap);

			PassParameters->bIsInverse = false;
			PassParameters->bTransformX = false;
			PassParameters->PressureGridUAV = PressureUAV;
			PassParameters->BaseParameter = SolverParameter->SolverBaseParameter;

			FComputeShaderUtils::AddPass(GraphBuilder,
				RDG_EVENT_NAME("FFTY"),
				ERDGPassFlags::Compute,
				ComputeShader,
				PassParameters,
				FIntVector(GridSize.X ,1,1));//FComputeShaderUtils::GetGroupCount( FIntVector(GridSize.X,GridSize.Y,1),FIntVector(GridSize.X ,1,1)));
		}

		//FFT X
		{
			FFFTSolvePoissonCS<256>::FParameters* PassParameters = GraphBuilder.AllocParameters<FFFTSolvePoissonCS<256>::FParameters>();
			TShaderMapRef<FFFTSolvePoissonCS<256>> ComputeShader(ShaderMap);

			PassParameters->bIsInverse = false;
			PassParameters->bTransformX = true;
			PassParameters->PressureGridUAV = PressureUAV;
			PassParameters->BaseParameter = SolverParameter->SolverBaseParameter;

			FComputeShaderUtils::AddPass(GraphBuilder,
				RDG_EVENT_NAME("FFTX"),
				ERDGPassFlags::Compute,
				ComputeShader,
				PassParameters,
				FIntVector(GridSize.Y ,1,1));//FComputeShaderUtils::GetGroupCount( FIntVector(GridSize.X,GridSize.Y,1),FIntVector(GridSize.X ,1,1)));
		}

		//Inv FFT X
		{
			FFFTSolvePoissonCS<256>::FParameters* PassParameters = GraphBuilder.AllocParameters<FFFTSolvePoissonCS<256>::FParameters>();
			TShaderMapRef<FFFTSolvePoissonCS<256>> ComputeShader(ShaderMap);
			PassParameters->bIsInverse = true;
			PassParameters->bTransformX = true;
			PassParameters->PressureGridUAV = PressureUAV;
			PassParameters->BaseParameter = SolverParameter->SolverBaseParameter;

			FComputeShaderUtils::AddPass(GraphBuilder,
				RDG_EVENT_NAME("InvFFTX"),
				ERDGPassFlags::Compute,
				ComputeShader,
				PassParameters,
				FIntVector(GridSize.Y ,1,1));//FComputeShaderUtils::GetGroupCount( FIntVector(GridSize.X,GridSize.Y,1),FIntVector(GridSize.X ,1,1)));
		}

		//Inv FFT Y
		{
			FFFTSolvePoissonCS<256>::FParameters* PassParameters = GraphBuilder.AllocParameters<FFFTSolvePoissonCS<256>::FParameters>();
			TShaderMapRef<FFFTSolvePoissonCS<256>> ComputeShader(ShaderMap);
			PassParameters->bIsInverse = true;
			PassParameters->bTransformX = false;
			PassParameters->PressureGridUAV = PressureUAV;
			PassParameters->BaseParameter = SolverParameter->SolverBaseParameter;

			FComputeShaderUtils::AddPass(GraphBuilder,
				RDG_EVENT_NAME("InvFFTY"),
				ERDGPassFlags::Compute,
				ComputeShader,
				PassParameters,
				FIntVector(GridSize.X ,1,1));//FComputeShaderUtils::GetGroupCount( FIntVector(GridSize.X,GridSize.Y,1),FIntVector(GridSize.X ,1,1)));
		}

	}
	else
	{
		 
		for(int i = 0;i<20;i++)
		{
			F2DFluidCS::FParameters* PassParameters = GraphBuilder.AllocParameters<F2DFluidCS::FParameters>();

			SetParameter(PassParameters,false,i, SimUAV, PressureUAV, SimSRV, PressureSRV,IteratePressure);

			F2DFluidCS::FPermutationDomain PermutationVector;
			//PermutationVector.Set<F2DFluidCS::FIteratePressure>(true);
			TShaderMapRef<F2DFluidCS> ComputeShader(ShaderMap,PermutationVector);


			FComputeShaderUtils::AddPass(GraphBuilder,
				RDG_EVENT_NAME("iteratePressure"),
				ERDGPassFlags::Compute,
				ComputeShader,
				PassParameters,
				FComputeShaderUtils::GetGroupCount(FIntVector(GridSize.X, GridSize.Y, 1),ThreadNum) );
			//AddCopyTexturePass(GraphBuilder,TempGrid,OutTextureArray[1]);
		}
	}


	//Advection Density
	{

		F2DFluidCS::FParameters* PassParameters = GraphBuilder.AllocParameters<F2DFluidCS::FParameters>();

		//GraphBuilder.RHICmdList.Transition(FRHITransitionInfo(SimulationTexture->GetRHI(), ERHIAccess::UAVCompute, ERHIAccess::SRVCompute));

		if(RHIGetInterfaceType() <ERHIInterfaceType::D3D12)
		{
			AddCopyTexturePass(GraphBuilder,SimulationTexture,TempGrid);
			SimSRV = GraphBuilder.CreateSRV(TempGrid);
			PressureSRV = GraphBuilder.CreateSRV(PressureTexture);
		}

		SetParameter(PassParameters,true,0, SimUAV, PressureUAV, SimSRV, PressureSRV,Advection);


		TShaderMapRef<F2DFluidCS> ComputeShader(ShaderMap);


		FComputeShaderUtils::AddPass(GraphBuilder,
			RDG_EVENT_NAME("AdvectionDensity"),
			ERDGPassFlags::Compute,
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(FIntVector(GridSize.X, GridSize.Y, 1),ThreadNum) );
	}

	// //post sim
	// {
	// 	AddCopyTexturePass(GraphBuilder,OutTextureArray[0],TempGrid);
	// 	F2DFluidCS::FParameters* PassParameters = GraphBuilder.AllocParameters<F2DFluidCS::FParameters>();
	// 	SetParameter(PassParameters,false,0);
	// 	F2DFluidCS::FPermutationDomain PermutationVector;
	// 	PermutationVector.Set<F2DFluidCS::FAdvection>(true);
	// 	TShaderMapRef<F2DFluidCS> ComputeShader(ShaderMap,PermutationVector);
	//
	//
	// 	FComputeShaderUtils::AddPass(GraphBuilder,
	// 		RDG_EVENT_NAME("Advection"),
	// 		ERDGPassFlags::Compute,
	// 		ComputeShader,
	// 		PassParameters,
	// 		FComputeShaderUtils::GetGroupCount(FIntVector(GridSize.X, GridSize.Y, 1),FIntVector(ThreadNum)) );
	// }
	//OutTexture = SimulationGrid;
	
}

void FPhysical2DFluidSolver::Initial(FPhysicalSolverContext* Context)
{
	Frame = 0;
	GridSize =  FIntPoint(Context->SolverParameter->FluidParameter.SolverBaseParameter.GridSize.X,
		Context->SolverParameter->FluidParameter.SolverBaseParameter.GridSize.Y) - 1;

	ENQUEUE_RENDER_COMMAND(InitVdbRendering)(
		[this, Context](FRHICommandListImmediate& RHICmdList)
		{
			SimulatorTextureRT = CreateRenderTarget(Context->OutputTextures[0]->GetResource()->GetTextureRHI(), TEXT("DensityTexture"));
			PressureTextureRT = CreateRenderTarget(Context->OutputTextures[1]->GetResource()->GetTextureRHI(), TEXT("PressureTexture"));
		});

	SolverParameter = &Context->SolverParameter->FluidParameter;
	InitialedDelegate.Broadcast();
}





