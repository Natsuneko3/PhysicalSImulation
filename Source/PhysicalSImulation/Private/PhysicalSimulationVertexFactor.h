// Copyright Natsu Neko, Inc. All Rights Reserved.

#pragma once

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT( FParticleUniformParameters, )
	SHADER_PARAMETER_SRV(Buffer<float>, InstanceParticle)

END_GLOBAL_SHADER_PARAMETER_STRUCT()


class PHYSICALSIMULATION_API FPhysicalSimulationVertexFactory : public FVertexFactory
{
public:
	DECLARE_VERTEX_FACTORY_TYPE(FPhysicalSimulationVertexFactor);

	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;


	/**
	 * Retrieve the uniform buffer for this vertex factory.
	 */
	FORCEINLINE FRHIUniformBuffer* GetSpriteUniformBuffer()
	{
		return UniformBuffer;
	}

	/**
	 * Should we cache the material's shadertype on this platform with this vertex factory?
	 */
	static  bool ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters);

	/**
	 * Can be overridden by FVertexFactory subclasses to modify their compile environment just before compilation occurs.
	 */
	static  void ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);

	/**
	 * Get vertex elements used when during PSO precaching materials using this vertex factory type
	 */
	static  void GetPSOPrecacheVertexFetchElements(EVertexInputStreamType VertexInputStreamType, FVertexDeclarationElementList& Elements);
	static  void GetVertexElements(ERHIFeatureLevel::Type FeatureLevel, bool bSupportsManualVertexFetch, FStaticMeshDataType& Data, FVertexDeclarationElementList& Elements);



	inline FRHIShaderResourceView* GetInstancePositionSRV() const
	{
		return InstanceParticleSRV;
	}

	void SetInstanceBuffer(FRHIShaderResourceView* InInstancePositionSRV)
	{
		InstanceParticleSRV = InInstancePositionSRV;
	}
protected:
	static  void GetVertexElements(ERHIFeatureLevel::Type FeatureLevel, bool bSupportsManualVertexFetch, FStaticMeshDataType& Data, FVertexDeclarationElementList& Elements, FVertexStreamList& InOutStreams);
	FStaticMeshDataType Data;
private:
	TUniformBufferRef<FParticleUniformParameters> UniformBuffer;

	FRHIShaderResourceView* InstanceParticleSRV;
};
