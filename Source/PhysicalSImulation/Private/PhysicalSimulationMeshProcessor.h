#pragma once
#include "DataDrivenShaderPlatformInfo.h"
#include "MeshPassProcessor.h"
#include "Materials/MaterialRenderProxy.h"
#include "Runtime/Renderer/Private/ScenePrivate.h"


struct FPSElementData : public FMeshMaterialShaderElementData
{
	FShaderResourceViewRHIRef InTexture1SRV;
};

class FPhysicalSimulationMeshProcessor : public FMeshPassProcessor
{
public:
	FPhysicalSimulationMeshProcessor(
		const FScene* Scene,
		const FSceneView* InView,
		FMeshPassDrawListContext* InDrawListContext);


	virtual void AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId = -1) override final;

private:
	void Process(
		const FMeshBatch& MeshBatch,
		uint64 BatchElementMask,
		int32 StaticMeshId,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
		const FMaterial& RESTRICT MaterialResource,
		ERasterizerFillMode MeshFillMode,
		ERasterizerCullMode MeshCullMode);


	FMeshPassProcessorRenderState PassDrawRenderState;

	//FPSElementData PSShaderElementData;
};
