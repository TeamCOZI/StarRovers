using UnrealBuildTool;
using System.Collections.Generic;

public class StarRoversEditorTarget : TargetRules
{
    public StarRoversEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V6;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("StarRovers");
    }
}
