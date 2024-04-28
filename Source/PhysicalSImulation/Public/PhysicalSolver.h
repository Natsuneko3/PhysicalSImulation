// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DynamicMeshBuilder.h"
#include "Engine/TextureRenderTarget.h"
#include "UObject/Object.h"
#include "HAL/IConsoleManager.h"
//#include "PhysicalSolver.generated.h"

/**
 * 
 */


class FPhysicalSimulationSceneProxy;
DECLARE_STATS_GROUP(TEXT("Physical Simulation"), STATGROUP_PS, STATCAT_Advanced)

BEGIN_SHADER_PARAMETER_STRUCT(FSolverBaseParameter, PHYSICALSIMULATION_API)
SHADER_PARAMETER(FVector3f,GridSize)
SHADER_PARAMETER(int32,Time)
SHADER_PARAMETER(float,dt)
SHADER_PARAMETER(float,dx)
SHADER_PARAMETER_SAMPLER(SamplerState, WarpSampler)
SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FFluidParameter, PHYSICALSIMULATION_API)
	SHADER_PARAMETER_STRUCT_INCLUDE(FSolverBaseParameter, SolverBaseParameter)
	SHADER_PARAMETER(float,VorticityMult)
	SHADER_PARAMETER(float,NoiseFrequency)
	SHADER_PARAMETER(float,NoiseIntensity)
	SHADER_PARAMETER(float,VelocityDissipate)
	SHADER_PARAMETER(float,DensityDissipate)
	SHADER_PARAMETER(float,GravityScale)
SHADER_PARAMETER(int,UseFFT)
END_SHADER_PARAMETER_STRUCT()


DECLARE_MULTICAST_DELEGATE(FPhysicalSolverInitialed);
UENUM()
enum class ESimulatorType : uint8
{
	PlaneSmokeFluid = 0,
	CubeSmokeFluid = 1,
	Liquid = 2
};
using FPSSpriteVertex = FDynamicMeshVertex;
class FPSSpriteVertexBuffer : public FVertexBuffer
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
	FPSSpriteVertexBuffer()
		: bDynamicUsage(true)
		, NumAllocatedVertices(0)
	{}

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
	void Init(FRHICommandListBase& RHICmdList, const FPSSpriteVertexBuffer* InVertexBuffer);

private:
	/* Vertex buffer used to initialize this factory. */
	const FPSSpriteVertexBuffer* VertexBuffer;
};

class FPSCubeVertexBuffer : public FRenderResource
{
public:
	FStaticMeshVertexBuffers Buffers;

	FPSCubeVertexBuffer()
	{

		TArray<FDynamicMeshVertex> Vertices;

		// Vertex position constructed in the shader
		Vertices.Add(FDynamicMeshVertex(FVector3f(-3.0f,  1.0f, 0.5f)));
		Vertices.Add(FDynamicMeshVertex(FVector3f( 1.0f, -3.0f, 0.5f)));
		Vertices.Add(FDynamicMeshVertex(FVector3f( 1.0f,  1.0f, 0.5f)));

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

class FPSCubeVertexFactory final : public FLocalVertexFactory
{
public:
	FPSCubeVertexFactory(ERHIFeatureLevel::Type InFeatureLevel,FPSCubeVertexBuffer* InVertexBuffer)
		: FLocalVertexFactory(InFeatureLevel, "FSingleTriangleMeshVertexFactory"),
	VertexBuffer(InVertexBuffer)
	{}

	~FPSCubeVertexFactory()
	{
		ReleaseResource();
	}

	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
	{
		//FPSCubeVertexBuffer* VertexBuffer = &GPSCubeVertexBuffer;
		FLocalVertexFactory::FDataType NewData;
		VertexBuffer->Buffers.PositionVertexBuffer.BindPositionVertexBuffer(this, NewData);
		VertexBuffer->Buffers.StaticMeshVertexBuffer.BindTangentVertexBuffer(this, NewData);
		VertexBuffer->Buffers.StaticMeshVertexBuffer.BindPackedTexCoordVertexBuffer(this, NewData);
		VertexBuffer->Buffers.StaticMeshVertexBuffer.BindLightMapVertexBuffer(this, NewData, 0);
		FColorVertexBuffer::BindDefaultColorVertexBuffer(this, NewData, FColorVertexBuffer::NullBindStride::ZeroForDefaultBufferBind);
		// Don't call SetData(), because that ends up calling UpdateRHI(), and if the resource has already been initialized
		// (e.g. when switching the feature level in the editor), that calls InitRHI(), resulting in an infinite loop.
		Data = NewData;
		//FLocalVertexFactory::InitRHI(RHICmdList);
	}

	bool HasIncompatibleFeatureLevel(ERHIFeatureLevel::Type InFeatureLevel)
	{
		return InFeatureLevel != GetFeatureLevel();
	}

	FPSCubeVertexBuffer* VertexBuffer;
};

class FPhysicalSolverBase
{

public:
	FPhysicalSolverBase(FPhysicalSimulationSceneProxy* InSceneProxy):SceneProxy(InSceneProxy){}
	int Frame = 0;
	//virtual void SetParameter(FPhysicalSimulationSceneProxy* InSceneProxy){}
	virtual void Initial(FRHICommandListImmediate& RHICmdList){}
	virtual void Release(){}
	virtual void PreRenderView_RenderThread(FRDGBuilder& GraphBuilder,FSceneView& InView){}
	virtual void PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) {}
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector,const FPhysicalSimulationSceneProxy* InSceneProxy){}
	virtual void Render_RenderThread(FPostOpaqueRenderParameters& Parameters){}
	FPhysicalSolverInitialed InitialedDelegate;
protected:
	FPhysicalSimulationSceneProxy* SceneProxy;
	void SetupSolverBaseParameters(FSolverBaseParameter& Parameter,FSceneView& InView);
	TUniquePtr<FPSCubeVertexBuffer> SpriteVertexBuffer;
	TUniquePtr<FPSCubeVertexFactory> CubeVertexFactory;
};
