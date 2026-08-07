using UnrealBuildTool;

public class RebuildOrchestrator : ModuleRules
{
    public RebuildOrchestrator(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Slate",
                "SlateCore",
                "UnrealEd",
                "ToolMenus",
                "Projects",
                "EditorStyle",
                "InputCore"
            }
        );
    }
}
