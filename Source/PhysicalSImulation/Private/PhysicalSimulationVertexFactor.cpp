// Copyright Natsu Neko, Inc. All Rights Reserved.


#include "PhysicalSimulationVertexFactor.h"

#include "MeshDrawShaderBindings.h"
#include "MeshMaterialShader.h"

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FParticleUniformParameters,"ParticleParameter")
class FParticleVertexFactoryShaderParameters : public FLocalVertexFactoryShaderParametersBase
{
	DECLARE_TYPE_LAYOUT(FParticleVertexFactoryShaderParameters, NonVirtual);

	void Bind(const FShaderParameterMap& ParameterMap)
	{
		InstanceParticleSRV.Bind(ParameterMap, TEXT("InstanceParticle"));
	}

	void GetElementShaderBindings(
		const FSceneInterface* Scene,
		const FSceneView* View,
		const FMeshMaterialShader* Shader,
		const EVertexInputStreamType InputStreamType,
		ERHIFeatureLevel::Type FeatureLevel,
		const FVertexFactory* VertexFactory,
		const FMeshBatchElement& BatchElement,
		class FMeshDrawSingleShaderBindings& ShaderBindings,
		FVertexInputStreamArray& VertexStreams) const
	{
		FPhysicalSimulationVertexFactory* SpriteVF = (FPhysicalSimulationVertexFactory*)VertexFactory;
		//FLocalVertexFactoryShaderParametersBase::GetElementShaderBindingsBase(Scene, View, Shader, InputStreamType, FeatureLevel, VertexFactory, BatchElement, VertexFactoryUniformBuffer, ShaderBindings, VertexStreams);


		ShaderBindings.Add(Shader->GetUniformBufferParameter<FParticleUniformParameters>(), SpriteVF->GetSpriteUniformBuffer());
		ShaderBindings.Add(InstanceParticleSRV, SpriteVF->GetInstancePositionSRV());
	}

private:
	LAYOUT_FIELD(FShaderParameter, InstanceParticleSRV);
};


IMPLEMENT_TYPE_LAYOUT(FParticleVertexFactoryShaderParameters);
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FPhysicalSimulationVertexFactory, SF_Vertex, FParticleVertexFactoryShaderParameters);

IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FPhysicalSimulationVertexFactory, SF_Pixel, FParticleVertexFactoryShaderParameters);

IMPLEMENT_VERTEX_FACTORY_TYPE(FPhysicalSimulationVertexFactory, "/PluginShader/PhysicalSimulationVertexFactory.ush",
                              EVertexFactoryFlags::UsedWithMaterials
)

void FPhysicalSimulationVertexFactory::InitRHI(FRHICommandListBase& RHICmdList)
{
	const bool bSupportsManualVertexFetch = SupportsManualVertexFetch(GetFeatureLevel());
	FVertexDeclarationElementList Elements;
	GetVertexElements(GetFeatureLevel(), bSupportsManualVertexFetch, Data, Elements, Streams);
	AddPrimitiveIdStreamElement(EVertexInputStreamType::Default, Elements, 2, 2);
	InitDeclaration(Elements);
}

bool FPhysicalSimulationVertexFactory::ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters)
{
	return Parameters.MaterialParameters.bIsUsedWithParticleSprites || Parameters.MaterialParameters.bIsSpecialEngineMaterial;
}

void FPhysicalSimulationVertexFactory::ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
}

void FPhysicalSimulationVertexFactory::GetPSOPrecacheVertexFetchElements(EVertexInputStreamType VertexInputStreamType, FVertexDeclarationElementList& Elements)
{
	FLocalVertexFactory::GetPSOPrecacheVertexFetchElements(VertexInputStreamType, Elements);
	//Elements.Add(FVertexElement(0, 0, VET_Float2, 0, 0, false));
}

void FPhysicalSimulationVertexFactory::GetVertexElements(ERHIFeatureLevel::Type FeatureLevel, bool bSupportsManualVertexFetch, FStaticMeshDataType& Data, FVertexDeclarationElementList& Elements)
{
	FVertexStreamList InOutStreams;
	GetVertexElements(FeatureLevel, bSupportsManualVertexFetch, Data, Elements, InOutStreams);
}

void FPhysicalSimulationVertexFactory::GetVertexElements(ERHIFeatureLevel::Type FeatureLevel, bool bSupportsManualVertexFetch, FStaticMeshDataType& Data, FVertexDeclarationElementList& Elements,
                                                         FVertexStreamList& InOutStreams)
{
	if (Data.PositionComponent.VertexBuffer != NULL)
	{
		Elements.Add(AccessStreamComponent(Data.PositionComponent, 0, InOutStreams));
	}
	if (Data.TextureCoordinates.Num())
	{
		Elements.Add(AccessStreamComponent(Data.TextureCoordinates[0], 1, InOutStreams));
	}
}
