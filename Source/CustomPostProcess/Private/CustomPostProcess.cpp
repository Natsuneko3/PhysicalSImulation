﻿#include "CustomPostProcess.h"

#include "CustomPostProcessDetalis.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"
#define LOCTEXT_NAMESPACE "FCustomPostProcessModule"

void FCustomPostProcessModule::StartupModule()
{
	/*FString PluginShaderDir = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("OutsidePlugins/PhysicalSImulation/Shaders"));
	if(!FPaths::DirectoryExists(PluginShaderDir))
	{
		PluginShaderDir = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("PhysicalSImulation/Shaders"));
	}
	else
	{
		PluginShaderDir = FPaths::Combine(FPaths::EnginePluginsDir(), TEXT("PhysicalSImulation/Shaders"));
	}

	AddShaderSourceDirectoryMapping(TEXT("/Plugin/CustomPostProcess"), PluginShaderDir);*/
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout(
		"CustomPostProcessVolume",
		FOnGetDetailCustomizationInstance::CreateStatic(&FCustomPostProcessDetail::MakeInstance)
	);
	PropertyModule.NotifyCustomizationModuleChanged();
}

void FCustomPostProcessModule::ShutdownModule()
{
    
}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FCustomPostProcessModule, CustomPostProcess)