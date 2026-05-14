using UnrealBuildTool;
using System.Collections.Generic;

public class StarRoversTarget : TargetRules
{
    public StarRoversTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V6;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("StarRovers");
    }
}
