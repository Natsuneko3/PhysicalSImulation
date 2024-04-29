#pragma once
#include "PhysicalSimulationVertexFactor.h"
#include "PhysicalSolver.h"
#include "RHIGPUReadback.h"
#include "Engine/InstancedStaticMesh.h"
#include "PhysicalLiquidSolver.generated.h"

BEGIN_SHADER_PARAMETER_STRUCT(FLiuquidParameter, PHYSICALSIMULATION_API)
	SHADER_PARAMETER_STRUCT_INCLUDE(FSolverBaseParameter, SolverBaseParameter)
	SHADER_PARAMETER(float,GravityScale)
	SHADER_PARAMETER(float,LifeTime)
END_SHADER_PARAMETER_STRUCT()

DEFINE_LOG_CATEGORY_STATIC(LogSimulation, Log, All);
USTRUCT(BlueprintType)
struct FLiquidSolverParameter
{
	GENERATED_BODY()
	float SpawnRate = 60;
	float LifeTime = 2;
	float GravityScale = 20;
};
//class FPhysicalSimulationSceneProxy;
class FPhysicalLiquidSolver:public FPhysicalSolverBase
{
	public:
	FPhysicalLiquidSolver(FPhysicalSimulationSceneProxy* InSceneProxy);
~FPhysicalLiquidSolver();

	void SetLiuquidParameter(FLiuquidParameter& Parameter,FSceneView& InView);

	virtual void PreRenderView_RenderThread(FRDGBuilder& GraphBuilder,FSceneView& InView) override;

	virtual void Initial(FRHICommandListImmediate& RHICmdList) override;
	virtual void Release() override;
	virtual void Render_RenderThread(FPostOpaqueRenderParameters& Parameters) override;
	void PostSimulation();

	bool bIsInitial = false;


	FIntVector GridSize;
	int32 Frame;
	float LastNumParticle;
	uint32 DeadParticle;
private:
	void DrawCube();
	int32 AllocatedInstanceCounts = 0;
	FRDGBufferRef ParticleIDBuffer;
	FRDGBufferRef ParticleAttributeBuffer;
	FRDGTextureRef RasterizeTexture;
	FRHIGPUBufferReadback* ParticleReadback = nullptr;
	FRHIGPUBufferReadback* ParticleIDReadback = nullptr;
	void EnqueueGPUReadback(FRDGBuilder& GraphBuilder);
};
