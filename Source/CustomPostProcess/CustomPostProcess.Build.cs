using UnrealBuildTool;

public class CustomPostProcess : ModuleRules
{
    public CustomPostProcess(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicIncludePaths.AddRange(
            new string[] {
                // ... add public include paths required here ...
                EngineDirectory + "/Source/Runtime/Renderer/Private"
            }
        );
        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core", "Renderer",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "Renderer",
                "RenderCore",
                "RHI",
                "RHICore"
            }
        );
    }
}