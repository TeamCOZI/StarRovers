using UnrealBuildTool;

public class StarRovers : ModuleRules
{
    public StarRovers(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "EnhancedInput",
                "InputCore",
                "UMG",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new[]
            {
                "Slate",
                "SlateCore",
            }
        );
    }
}
