// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CommonRenderResources.h"
#include "DynamicMeshBuilder.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphEvent.h"
#include "ShaderParameterStruct.h"
#include "PostProcess/PostProcessing.h"

//#include "PhysicalSolver.generated.h"

class FPhysicalSimulationSceneProxy;

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

SHADER_PARAMETER(FVector3f, WorldVelocity)
SHADER_PARAMETER_SAMPLER(SamplerState, SimSampler)
		SHADER_PARAMETER(FVector3f, WorldPosition)

	SHADER_PARAMETER(int, UseFFT)
END_SHADER_PARAMETER_STRUCT()


DECLARE_MULTICAST_DELEGATE(FPhysicalSolverInitialed);
UENUM()
enum class ESimulatorType : uint8
{
	PlaneSmokeFluid = 0,
	Psychedelic = 1,
	Liquid = 2
};

using FPSSpriteVertex = FDynamicMeshVertex;

class FPSVertexBuffer : public FVertexBuffer
{
public:
	//Buffers
	FVertexBuffer PositionBuffer;
	FVertexBuffer TangentBuffer;
	FVertexBuffer TexCoordBuffer;
	FVertexBuffer ColorBuffer;
	FIndexBuffer IndexBuffer;

	//SRVs for Manual Fetch on platforms that support it
	FShaderResourceViewRHIRef TangentBufferSRV;
	FShaderResourceViewRHIRef TexCoordBufferSRV;
	FShaderResourceViewRHIRef ColorBufferSRV;
	FShaderResourceViewRHIRef PositionBufferSRV;

	//Vertex data
	TArray<FPSSpriteVertex> Vertices;

	//Ctor
	FPSVertexBuffer()
		: bDynamicUsage(true)
		  , NumAllocatedVertices(0)
	{
	}

	/* Marks this buffer as dynamic, so it gets initialized as so. */
	void SetDynamicUsage(bool bInDynamicUsage);

	/* Initializes the buffers with the given number of vertices to accommodate. */
	void CreateBuffers(FRHICommandListBase& RHICmdList, int32 NumVertices);

	/* Clear all the buffers currently being used. */
	void ReleaseBuffers();

	/* Moves all the PaperVertex data onto the RHI buffers. */
	void CommitVertexData(FRHICommandListBase& RHICmdList);

	// FRenderResource interface
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;
	virtual void ReleaseRHI() override;
	virtual void InitResource(FRHICommandListBase& RHICmdList) override;
	virtual void ReleaseResource() override;
	// End of FRenderResource interface

	/* True if generating a commit would require a reallocation of the buffers. */
	FORCEINLINE bool CommitRequiresBufferRecreation() const { return NumAllocatedVertices != Vertices.Num(); }

	/* Checks if the buffer has been initialized. */
	FORCEINLINE bool IsInitialized() const { return NumAllocatedVertices > 0; }

	/* Obtain the index buffer initialized for this buffer. */
	FORCEINLINE const FIndexBuffer* GetIndexPtr() const { return &IndexBuffer; }

private:
	/* Indicates if this buffer will be configured for dynamic usage. */
	bool bDynamicUsage;

	/* Amount of vertices allocated on the vertex buffer. */
	int32 NumAllocatedVertices;
};

class FPSSpriteVertexFactory : public FLocalVertexFactory
{
public:
	FPSSpriteVertexFactory(ERHIFeatureLevel::Type FeatureLevel);

	/* Initializes this factory with a given vertex buffer. */
	void Init(FRHICommandListBase& RHICmdList, const FPSVertexBuffer* InVertexBuffer);

private:
	/* Vertex buffer used to initialize this factory. */
	const FPSVertexBuffer* VertexBuffer;
};

class FPSCubeVertexBuffer : public FRenderResource
{
public:
	FStaticMeshVertexBuffers Buffers;

	FPSCubeVertexBuffer()
	{
		TArray<FDynamicMeshVertex> Vertices;

		// Vertex position constructed in the shader
		Vertices.Add(FDynamicMeshVertex(FVector3f(-3.0f, 1.0f, 0.5f)));
		Vertices.Add(FDynamicMeshVertex(FVector3f(1.0f, -3.0f, 0.5f)));
		Vertices.Add(FDynamicMeshVertex(FVector3f(1.0f, 1.0f, 0.5f)));

		Buffers.PositionVertexBuffer.Init(Vertices.Num());
		Buffers.StaticMeshVertexBuffer.Init(Vertices.Num(), 1);

		for (int32 i = 0; i < Vertices.Num(); i++)
		{
			const FDynamicMeshVertex& Vertex = Vertices[i];

			Buffers.PositionVertexBuffer.VertexPosition(i) = Vertex.Position;
			Buffers.StaticMeshVertexBuffer.SetVertexTangents(i, Vertex.TangentX.ToFVector3f(), Vertex.GetTangentY(), Vertex.TangentZ.ToFVector3f());
			Buffers.StaticMeshVertexBuffer.SetVertexUV(i, 0, Vertex.TextureCoordinate[0]);
		}
	}

	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
	{
		Buffers.PositionVertexBuffer.InitResource(RHICmdList);
		Buffers.StaticMeshVertexBuffer.InitResource(RHICmdList);
	}

	virtual void ReleaseRHI() override
	{
		Buffers.PositionVertexBuffer.ReleaseResource();
		Buffers.StaticMeshVertexBuffer.ReleaseResource();
	}
};

//static TGlobalResource<FPSCubeVertexBuffer> GPSCubeVertexBuffer;

class FPSVertexFactory final : public FLocalVertexFactory
{
public:
	FPSVertexFactory(ERHIFeatureLevel::Type FeatureLevel);

	/* Initializes this factory with a given vertex buffer. */
	void Init(const FPSVertexBuffer* VertexBuffer);

private:
	/* Vertex buffer used to initialize this factory. */
	const FPSVertexBuffer* VertexBuffer;
};

class FPhysicalSolverBase
{
public:
	FPhysicalSolverBase(FPhysicalSimulationSceneProxy* InSceneProxy)
	{

	}
	virtual ~FPhysicalSolverBase(){}

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
	virtual void PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs) {};

	FPhysicalSolverInitialed InitialedDelegate;

protected:
	FPhysicalSimulationSceneProxy* SceneProxy;
	void SetupSolverBaseParameters(FSolverBaseParameter& Parameter, FSceneView& InView,FPhysicalSimulationSceneProxy* InSceneProxy);
	void InitialPlaneMesh();
	void InitialCubeMesh();

	virtual void Release()
	{
	}

	FBufferRHIRef VertexBufferRHI;
	FBufferRHIRef IndexBufferRHI;

	template <typename TShaderClassVS, typename TShaderClassPS>
	void DrawMesh(const TShaderRef<TShaderClassVS>& VertexShader, const TShaderRef<TShaderClassPS>& PixelShader,
	              typename TShaderClassVS::FParameters* InVSParameters,
	              typename TShaderClassPS::FParameters* InPSParameters,
	              FRDGBuilder& GraphBuilder,const FIntRect& ViewportRect,uint32 NumInstance)
	{
		if (NumPrimitives == 0)
		{
			UE_LOG(LogSimulation,Log,TEXT("The Mesh has not initial yet"));
			return;
		}

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("DrawPSCubeMesh"),
			InPSParameters,
			ERDGPassFlags::Raster,
			[VertexShader,PixelShader,InVSParameters,InPSParameters,ViewportRect,this,NumInstance](FRHICommandList& RHICmdList)
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
				RHICmdList.DrawIndexedPrimitive(IndexBufferRHI, 0, 0, 8, 0, NumPrimitives, NumInstance);
			});
	}

private:
	uint32 NumPrimitives;
};
