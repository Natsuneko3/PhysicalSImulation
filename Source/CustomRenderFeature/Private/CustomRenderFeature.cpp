#include "CustomRenderFeature.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"
#include "Interfaces/IPluginManager.h"
#include "Modules/ModuleInterface.h"
#define LOCTEXT_NAMESPACE "FCustomRenderFeatureModule"

void FCustomRenderFeatureModule::StartupModule()
{

	FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("CustomRenderFeature"))->GetBaseDir(), TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/Plugin/CustomRenderFeature"), PluginShaderDir);

}

void FCustomRenderFeatureModule::ShutdownModule()
{
    
}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FCustomRenderFeatureModule, CustomRenderFeature)