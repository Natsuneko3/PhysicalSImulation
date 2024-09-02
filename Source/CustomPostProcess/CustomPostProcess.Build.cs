using UnrealBuildTool;

public class CustomPostProcess : ModuleRules
{
    public CustomPostProcess(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
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