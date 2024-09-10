#include "CustomPostProcess.h"

#include "Misc/Paths.h"
#include "ShaderCore.h"
#define LOCTEXT_NAMESPACE "FCustomRenderFeatureModule"

void FCustomRenderFeatureModule::StartupModule()
{
	FString PluginShaderDir = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("CustomRenderFeature/Shaders"));
	if(!FPaths::DirectoryExists(PluginShaderDir))
	{
		PluginShaderDir = FPaths::Combine(FPaths::EnginePluginsDir(), TEXT("CustomRenderFeature/Shaders"));
	}

	AddShaderSourceDirectoryMapping(TEXT("/Plugin/CustomRenderFeature"), PluginShaderDir);
}

void FCustomRenderFeatureModule::ShutdownModule()
{
    
}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FCustomRenderFeatureModule, CustomRenderFeature)