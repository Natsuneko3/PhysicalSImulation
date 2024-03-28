// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicalSImulation.h"

#define LOCTEXT_NAMESPACE "FPhysicalSImulationModule"

void FPhysicalSImulationModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	FString PluginShaderDir = FPaths::Combine(FPaths::EnginePluginsDir(), TEXT("PhysicalSImulation/Shaders"));
	if(!FPaths::DirectoryExists(PluginShaderDir))
	{
		PluginShaderDir = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("PhysicalSImulation/Shaders"));
	}

	AddShaderSourceDirectoryMapping(TEXT("/PluginShader"), PluginShaderDir);
}

void FPhysicalSImulationModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FPhysicalSImulationModule, PhysicalSImulation)