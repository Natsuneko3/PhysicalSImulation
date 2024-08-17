// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CommonRenderResources.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphEvent.h"
#include "ShaderParameterStruct.h"
#include "PostProcess/PostProcessing.h"

#if ENGINE_MINOR_VERSION >1
#include "DataDrivenShaderPlatformInfo.h"
#endif
//#include "PhysicalSolver.generated.h"

class FPhysicalSimulationSceneProxy;

class FCommonMeshVS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FCommonMeshVS);
	SHADER_USE_PARAMETER_STRUCT(FCommonMeshVS, FGlobalShader);

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER(FMatrix44f, LocalToWorld)
	END_SHADER_PARAMETER_STRUCT()
};

DEFINE_LOG_CATEGORY_STATIC(LogSimulation, Log, All);

DECLARE_STATS_GROUP(TEXT("Physical Simulation"), STATGROUP_PS, STATCAT_Advanced)

BEGIN_SHADER_PARAMETER_STRUCT(FSolverBaseParameter, PHYSICALSIMULATION_API)
	SHADER_PARAMETER(FVector3f, GridSize)
	SHADER_PARAMETER(int, Time)
	SHADER_PARAMETER(float, dt)
	SHADER_PARAMETER(float, dx)
	SHADER_PARAMETER_SAMPLER(SamplerState, WarpSampler)
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FFluidParameter, PHYSICALSIMULATION_API)
	SHADER_PARAMETER_STRUCT_INCLUDE(FSolverBaseParameter, SolverBaseParameter)
	SHADER_PARAMETER(float, VorticityMult)
	SHADER_PARAMETER(float, NoiseFrequency)
	SHADER_PARAMETER(float, NoiseIntensity)
	SHADER_PARAMETER(float, VelocityDissipate)
	SHADER_PARAMETER(float, DensityDissipate)
	SHADER_PARAMETER(float, GravityScale)
SHADER_PARAMETER(int, UseFFT)
SHADER_PARAMETER(FVector3f, WorldPosition)
SHADER_PARAMETER(FVector3f, WorldVelocity)
	SHADER_PARAMETER_TEXTURE(Texture2D, InTexture)
SHADER_PARAMETER_SAMPLER(SamplerState, SimSampler)
SHADER_PARAMETER_SAMPLER(SamplerState, InTextureSampler)
END_SHADER_PARAMETER_STRUCT()

DECLARE_MULTICAST_DELEGATE(FPhysicalSolverInitialed);

UENUM()
enum class ESimulatorType : uint8
{
	PlaneSmokeFluid = 0,
	Psychedelic = 1,
	Liquid = 2,
	RadianceCascades = 3,
	TranslucentPostPrecess = 4
};

class FPhysicalSolverBase
{
public:
	FPhysicalSolverBase(FPhysicalSimulationSceneProxy* InSceneProxy)
	{
	}

	virtual ~FPhysicalSolverBase()
	{
	}

	int Frame = 0;

	virtual void Initial_RenderThread(FRHICommandListImmediate& RHICmdList)
	{
	}

	virtual void PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView)
	{
	}

	virtual void PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily)
	{
	}

	virtual void Render_RenderThread(FPostOpaqueRenderParameters& Parameters)
	{
	}

	virtual void PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs)
	{
	};

	FPhysicalSolverInitialed InitialedDelegate;

protected:
	FPhysicalSimulationSceneProxy* SceneProxy;
	void SetupSolverBaseParameters(FSolverBaseParameter& Parameter, FSceneView& InView, FPhysicalSimulationSceneProxy* InSceneProxy);
	void InitialPlaneMesh(FRHICommandList& RHICmdList);
	void InitialCubeMesh(FRHICommandList& RHICmdList);

	virtual void Release()
	{
	}

	FBufferRHIRef VertexBufferRHI;
	FBufferRHIRef IndexBufferRHI;

	template <typename TShaderClassPS>
	void DrawMesh(const FViewInfo& View,
	FMatrix44f Transform,
		const TShaderRef<TShaderClassPS>& PixelShader,
	              typename TShaderClassPS::FParameters* InPSParameters,
	              FRDGBuilder& GraphBuilder, const FIntRect& ViewportRect, uint32 NumInstance)
	{
		if (NumPrimitives == 0)
		{
			UE_LOG(LogSimulation, Log, TEXT("The Mesh has not initial yet"));
			return;
		}
		FCommonMeshVS::FParameters* InVSParameters = GraphBuilder.AllocParameters<FCommonMeshVS::FParameters>();
		InVSParameters->View = View.ViewUniformBuffer;
		InVSParameters->LocalToWorld = Transform;

		FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
		TShaderMapRef<FCommonMeshVS> VertexShader(GlobalShaderMap);
		FString Name = NumPrimitives==2?"DrawPlaneMesh":"DrawCubeMesh";
		GraphBuilder.AddPass(
			RDG_EVENT_NAME("%s",*Name),
			InPSParameters,
			ERDGPassFlags::Raster,
			[VertexShader,InVSParameters,PixelShader,InPSParameters,ViewportRect,this,NumInstance](FRHICommandList& RHICmdList)
			{
				FGraphicsPipelineStateInitializer GraphicsPSOInit;
				RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
				GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero>::GetRHI();
				GraphicsPSOInit.RasterizerState = GetStaticRasterizerState<true>(FM_Solid, CM_CW);
				GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Less>::GetRHI();
				GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
				GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
				GraphicsPSOInit.PrimitiveType = PT_TriangleList;
				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

				SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), *InVSParameters);
				SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), *InPSParameters);

				RHICmdList.SetViewport(ViewportRect.Min.X, ViewportRect.Min.Y, 0.0f, ViewportRect.Max.X, ViewportRect.Max.Y, 1.0f);
				RHICmdList.SetStreamSource(0, VertexBufferRHI, 0);

				RHICmdList.DrawIndexedPrimitive(IndexBufferRHI, 0, 0, NumPrimitives==2? 4:8, 0, NumPrimitives, NumInstance);
			});
	}


	void AddTextureBlurPass(FRDGBuilder& GraphBuilder,const FViewInfo& View,FRDGTextureRef InTexture,FRDGTextureRef& OutTexture,float BlurSize);

private:
	uint32 NumPrimitives;
};
