using UnrealBuildTool;
using System.Collections.Generic;

public class StarRoversEditorTarget : TargetRules
{
    public StarRoversEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        bOverrideBuildEnvironment = true;
        // UE 5.7 engine SharedPCH headers emit deprecation warnings with VS 2026.
        CppCompileWarningSettings.DeprecationWarningLevel = WarningLevel.Off;
        ExtraModuleNames.Add("StarRovers");
    }
}
