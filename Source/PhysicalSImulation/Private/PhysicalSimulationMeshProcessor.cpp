#include "PhysicalSimulationMeshProcessor.h"
#include "MeshPassProcessor.inl"



class FPhysicalSimulationMeshVS : public FMeshMaterialShader
{
public:
	DECLARE_SHADER_TYPE(FPhysicalSimulationMeshVS, MeshMaterial);

	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		// Compile if supported by the hardware.
		const bool bIsFeatureSupported = IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);

		return
			bIsFeatureSupported &&
			FMeshMaterialShader::ShouldCompilePermutation(Parameters);
	}

	FPhysicalSimulationMeshVS() = default;

	FPhysicalSimulationMeshVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMeshMaterialShader(Initializer)
	{
	}
};

class FPhysicalSimulationMeshPS : public FMeshMaterialShader
{
public:
	DECLARE_SHADER_TYPE(FPhysicalSimulationMeshPS, MeshMaterial);

	FPhysicalSimulationMeshPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
			: FMeshMaterialShader(Initializer)
	{
		//InTexture1.Bind(Initializer.ParameterMap,TEXT("InTexture1"));
	}

	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		return FPhysicalSimulationMeshVS::ShouldCompilePermutation(Parameters);
	}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

	FPhysicalSimulationMeshPS() = default;

	/*void GetShaderBindings(const FScene* Scene,
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

	*/

	//LAYOUT_FIELD(FShaderResourceParameter, InTexture1);
};

IMPLEMENT_SHADER_TYPE(, FPhysicalSimulationMeshVS, TEXT("/Plugin/PhysicalSimulation/PhysicalSimulationPassShader.usf"), TEXT("MainVS"), SF_Vertex);
IMPLEMENT_SHADER_TYPE(, FPhysicalSimulationMeshPS, TEXT("/Plugin/PhysicalSimulation/PhysicalSimulationPassShader.usf"), TEXT("MainPS"), SF_Pixel);
IMPLEMENT_SHADERPIPELINE_TYPE_VSPS(PShaderPipeline, FPhysicalSimulationMeshVS, FPhysicalSimulationMeshPS, true);


FPhysicalSimulationMeshProcessor::FPhysicalSimulationMeshProcessor(
	const FScene* Scene,
	const FSceneView* InView,
	FMeshPassDrawListContext* InDrawListContext
	): FMeshPassProcessor(Scene, Scene->GetFeatureLevel(), InView, InDrawListContext)
{
	PassDrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_InverseSourceAlpha, BO_Add, BF_One, BF_InverseSourceAlpha>::GetRHI()); // premultiplied alpha blending
	PassDrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI());
}

void FPhysicalSimulationMeshProcessor::AddMeshBatch(const FMeshBatch& MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* PrimitiveSceneProxy, int32 StaticMeshId)
{
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
}

bool GetPassShaders(const FMaterial& Material, FVertexFactoryType* VertexFactoryType, TShaderRef<FPhysicalSimulationMeshVS>& VertexShader, TShaderRef<FPhysicalSimulationMeshPS>& PixelShader)
{
	FMaterialShaderTypes ShaderTypes;
	ShaderTypes.PipelineType = &PShaderPipeline;
	ShaderTypes.AddShaderType<FPhysicalSimulationMeshVS>();
	ShaderTypes.AddShaderType<FPhysicalSimulationMeshPS>();

	FMaterialShaders Shaders;
	if (!Material.TryGetShaders(ShaderTypes, VertexFactoryType, Shaders))
	{
		return false;
	}

	Shaders.TryGetVertexShader(VertexShader);
	Shaders.TryGetPixelShader(PixelShader);

	return VertexShader.IsValid() && PixelShader.IsValid();
}

void FPhysicalSimulationMeshProcessor::Process(const FMeshBatch& MeshBatch, uint64 BatchElementMask, int32 StaticMeshId, const FPrimitiveSceneProxy* PrimitiveSceneProxy, const FMaterialRenderProxy& MaterialRenderProxy, const FMaterial& MaterialResource,
                                               ERasterizerFillMode MeshFillMode, ERasterizerCullMode MeshCullMode)
{
	{
		FMeshMaterialShaderElementData EmptyShaderElementData;
		EmptyShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, StaticMeshId, false);
		const FVertexFactory* VertexFactory = MeshBatch.VertexFactory;

		TMeshProcessorShaders<FPhysicalSimulationMeshVS, FPhysicalSimulationMeshPS> PassShaders;
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
			EmptyShaderElementData);
	}
}
