using UnrealBuildTool;

public class CustomRenderFeature : ModuleRules
{
    public CustomRenderFeature(ReadOnlyTargetRules Target) : base(Target)
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
                "RenderCore",
                "RHI",
                "RHICore",
                "Projects"
            }
        );
    }
}