#pragma once
#include "MeshPassProcessor.h"
#include "Renderer/Private/ScenePrivate.h"

struct FPSElementData : public FMeshMaterialShaderElementData
{
	FShaderResourceViewRHIRef InTexture1SRV;
};

BEGIN_SHADER_PARAMETER_STRUCT(FPSShaderParametersPS,)
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
	SHADER_PARAMETER_RDG_BUFFER_SRV(Texture2D, InTexture1)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FInstanceCullingGlobalUniforms, InstanceCulling)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

class FPhysicalSimulationVS : public FMeshMaterialShader
{
public:
	DECLARE_SHADER_TYPE(FPhysicalSimulationVS, MeshMaterial);

	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		// Compile if supported by the hardware.
		const bool bIsFeatureSupported = IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);

		return
			bIsFeatureSupported &&
			FMeshMaterialShader::ShouldCompilePermutation(Parameters);
	}

	FPhysicalSimulationVS() = default;

	FPhysicalSimulationVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMeshMaterialShader(Initializer)
	{
	}
};

class FPhysicalSimulationPS : public FMeshMaterialShader
{
public:
	DECLARE_SHADER_TYPE(FPhysicalSimulationPS, MeshMaterial);

	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		return FPhysicalSimulationPS::ShouldCompilePermutation(Parameters);
	}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

	FPhysicalSimulationPS() = default;

	void GetShaderBindings(const FScene* Scene,
	                       ERHIFeatureLevel::Type FeatureLevel,
	                       const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	                       const FMaterialRenderProxy& MaterialRenderProxy,
	                       const FMaterial& Material,
	                       const FMeshPassProcessorRenderState& DrawRenderState,
	                       const FPSElementData& ShaderElementData,
	                       FMeshDrawSingleShaderBindings& ShaderBindings) const
	{
		FMeshMaterialShader::GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, Material, DrawRenderState, ShaderElementData, ShaderBindings);
		ShaderBindings.Add(InTexture1, ShaderElementData.InTexture1SRV);
	}

	FPhysicalSimulationPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMeshMaterialShader(Initializer)
	{
		InTexture1.Bind(Initializer.ParameterMap,TEXT("InTexture1"));
	}

	LAYOUT_FIELD(FShaderResourceParameter, InTexture1);
};

IMPLEMENT_SHADER_TYPE(, FPhysicalSimulationVS, TEXT("/PluginShader/PhysicalSimulationPassShader.usf"), TEXT("MainVS"), SF_Vertex);
IMPLEMENT_SHADER_TYPE(, FPhysicalSimulationPS, TEXT("/PluginShader/PhysicalSimulationPassShader.usf"), TEXT("MainPS"), SF_Pixel);
IMPLEMENT_SHADERPIPELINE_TYPE_VSPS(AnisotropyPipeline, FPhysicalSimulationVS, FPhysicalSimulationPS, true);

class PhysicalSimulationMeshProcessor : public FMeshPassProcessor
{
public:
	PhysicalSimulationMeshProcessor(
		const FScene* Scene,
		const FSceneView* InView,
		FMeshPassDrawListContext* InDrawListContext,
		FPSElementData&& ShaderElementData)
		: FMeshPassProcessor(Scene, Scene->GetFeatureLevel(), InView, InDrawListContext),
		  PSShaderElementData(ShaderElementData)

	{
		PassDrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_InverseSourceAlpha, BO_Add, BF_One, BF_InverseSourceAlpha>::GetRHI()); // premultiplied alpha blending
		PassDrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI());
	}

	virtual void AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId = -1) override final
	{
		const FMaterialRenderProxy* MaterialRenderProxy = MeshBatch.MaterialRenderProxy;

		const FMaterial* Material = MaterialRenderProxy->GetMaterialNoFallback(FeatureLevel);
		if (Material)
		{
			const FMeshDrawingPolicyOverrideSettings OverrideSettings = ComputeMeshOverrideSettings(MeshBatch);
			const ERasterizerFillMode MeshFillMode = ComputeMeshFillMode(*Material, OverrideSettings);
			const ERasterizerCullMode MeshCullMode = ComputeMeshCullMode(*Material, OverrideSettings);
			Process(MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, *MaterialRenderProxy, *Material, MeshFillMode, MeshCullMode);
		}

	}

private:
	bool GetPassShaders(
		const FMaterial& Material,
		FVertexFactoryType* VertexFactoryType,
		TShaderRef<FPhysicalSimulationVS>& VertexShader,
		TShaderRef<FPhysicalSimulationPS>& PixelShader
	)
	{
		FMaterialShaderTypes ShaderTypes;
		ShaderTypes.AddShaderType<FPhysicalSimulationVS>();
		ShaderTypes.AddShaderType<FPhysicalSimulationPS>();

		FMaterialShaders Shaders;
		if (!Material.TryGetShaders(ShaderTypes, VertexFactoryType, Shaders))
		{
			return false;
		}

		Shaders.TryGetVertexShader(VertexShader);
		Shaders.TryGetPixelShader(PixelShader);

		return VertexShader.IsValid() && PixelShader.IsValid();
	}


	void Process(
		const FMeshBatch& MeshBatch,
		uint64 BatchElementMask,
		int32 StaticMeshId,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
		const FMaterial& RESTRICT MaterialResource,
		ERasterizerFillMode MeshFillMode,
		ERasterizerCullMode MeshCullMode)
	{
		PSShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, StaticMeshId, false);

		const FVertexFactory* VertexFactory = MeshBatch.VertexFactory;

		TMeshProcessorShaders<FPhysicalSimulationVS, FPhysicalSimulationPS> PassShaders;
		if (!GetPassShaders(
			MaterialResource,
			VertexFactory->GetType(),
			PassShaders.VertexShader,
			PassShaders.PixelShader))
		{
			return;
		}


		const FMeshDrawCommandSortKey SortKey = CalculateMeshStaticSortKey(PassShaders.VertexShader, PassShaders.PixelShader);
		BuildMeshDrawCommands(
			MeshBatch,
			BatchElementMask,
			PrimitiveSceneProxy,
			MaterialRenderProxy,
			MaterialResource,
			PassDrawRenderState,
			PassShaders,
			MeshFillMode,
			MeshCullMode,
			SortKey,
			EMeshPassFeatures::Default,
			PSShaderElementData);
	}

	FMeshPassProcessorRenderState PassDrawRenderState;

	FPSElementData PSShaderElementData;
};
