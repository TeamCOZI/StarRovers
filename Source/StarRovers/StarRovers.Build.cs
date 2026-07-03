using UnrealBuildTool;

using System.IO;

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
                "DeveloperSettings",
                "Engine",
                "EnhancedInput",
                "GeometryCore",
                "GeometryFramework",
                "InputCore",
                "Niagara",
                "PCG",
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

        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(
                new[]
                {
                    "NiagaraEditor",
                    "UnrealEd",
                }
            );

            PrivateIncludePaths.AddRange(
                new[]
                {
                    Path.Combine(EngineDirectory, "Plugins/FX/Niagara/Source/NiagaraEditor/Private"),
                }
            );
        }
    }
}
