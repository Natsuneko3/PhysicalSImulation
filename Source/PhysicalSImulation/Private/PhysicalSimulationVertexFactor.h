/*
// Copyright Natsu Neko, Inc. All Rights Reserved.

#pragma once

/*BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT( FParticleUniformParameters, )
	SHADER_PARAMETER_SRV(Buffer<float>, InstanceParticle)

END_GLOBAL_SHADER_PARAMETER_STRUCT()#1#


class PHYSICALSIMULATION_API FPhysicalSimulationVertexFactory : public FLocalVertexFactory
{
public:
	DECLARE_VERTEX_FACTORY_TYPE(FPhysicalSimulationVertexFactor);
	FPhysicalSimulationVertexFactory(ERHIFeatureLevel::Type InFeatureLevel)
	: FLocalVertexFactory(InFeatureLevel, "FPhysicalSimulationVertexFactory"){}
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;


	/**
	 * Retrieve the uniform buffer for this vertex factory.
	 #1#
	/*FORCEINLINE FRHIUniformBuffer* GetSpriteUniformBuffer()
	{
		return UniformBuffer;
	}#1#

	/**
	 * Should we cache the material's shadertype on this platform with this vertex factory?
	 #1#
	static  bool ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters);

	/**
	 * Can be overridden by FVertexFactory subclasses to modify their compile environment just before compilation occurs.
	 #1#
	static  void ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);

	/**
	 * Get vertex elements used when during PSO precaching materials using this vertex factory type
	 #1#
	static  void GetPSOPrecacheVertexFetchElements(EVertexInputStreamType VertexInputStreamType, FVertexDeclarationElementList& Elements);
	//static  void GetVertexElements(ERHIFeatureLevel::Type FeatureLevel, bool bSupportsManualVertexFetch, FStaticMeshDataType& Data, FVertexDeclarationElementList& Elements);



	inline FRHIShaderResourceView* GetInstancePositionSRV() const
	{
		return InstanceParticleSRV;
	}

	void SetInstanceBuffer(FRHIShaderResourceView* InInstancePositionSRV)
	{
		InstanceParticleSRV = InInstancePositionSRV;
	}

	void SetUpVertexBuffer(const FStaticMeshLODResources& LODResources);
protected:
	//static  void GetVertexElements(ERHIFeatureLevel::Type FeatureLevel, bool bSupportsManualVertexFetch, FStaticMeshDataType& Data, FVertexDeclarationElementList& Elements, FVertexStreamList& InOutStreams);
	FStaticMeshDataType Data;
private:
	//TUniformBufferRef<FParticleUniformParameters> UniformBuffer;

	FRHIShaderResourceView* InstanceParticleSRV;
};
*/
