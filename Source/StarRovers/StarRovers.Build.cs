using UnrealBuildTool;

public class StarRovers : ModuleRules
{
	// Keep source discovery module-wide so nested runtime automation tests build with the Editor target.
	public StarRovers(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        // UE 5.7 engine headers emit deprecation warnings with VS 2026; keep project builds readable.
        CppCompileWarningSettings.DeprecationWarningLevel = WarningLevel.Off;

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
            PrivateDependencyModuleNames.Add("AssetRegistry");
            PrivateDependencyModuleNames.Add("NiagaraEditor");
            PrivateDependencyModuleNames.Add("UnrealEd");
        }
    }
}
