using UnrealBuildTool;
using System.Collections.Generic;

public class StarRoversTarget : TargetRules
{
    public StarRoversTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("StarRovers");
    }
}
