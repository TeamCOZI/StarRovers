#include "Utility/SRLog.h"

#include "HAL/IConsoleManager.h"

namespace StarRovers::Logging
{
	namespace
	{
		TAutoConsoleVariable<int32> CVarSRLogAll(
			TEXT("sr.Log.All"),
			0,
			TEXT("Enables all Star Rovers C++ logs. 0=disabled, 1=enabled."));

		TAutoConsoleVariable<int32> CVarSRLogAssembly(
			TEXT("sr.Log.Assembly"),
			0,
			TEXT("Enables Star Rovers assembly/build-mode logs. 0=disabled, 1=enabled."));

		TAutoConsoleVariable<int32> CVarSRLogAugment(
			TEXT("sr.Log.Augment"),
			0,
			TEXT("Enables Star Rovers augment system logs. 0=disabled, 1=enabled."));

		TAutoConsoleVariable<int32> CVarSRLogCamera(
			TEXT("sr.Log.Camera"),
			0,
			TEXT("Enables Star Rovers camera and player-controller logs. 0=disabled, 1=enabled."));

		TAutoConsoleVariable<int32> CVarSRLogCelestial(
			TEXT("sr.Log.Celestial"),
			0,
			TEXT("Enables Star Rovers celestial body logs. 0=disabled, 1=enabled."));

		TAutoConsoleVariable<int32> CVarSRLogConveyor(
			TEXT("sr.Log.Conveyor"),
			0,
			TEXT("Enables Star Rovers conveyor logs. 0=disabled, 1=enabled."));

		TAutoConsoleVariable<int32> CVarSRLogDynamicMesh(
			TEXT("sr.Log.DynamicMesh"),
			0,
			TEXT("Enables Star Rovers dynamic mesh generation logs. 0=disabled, 1=enabled."));

		TAutoConsoleVariable<int32> CVarSRLogEditorCommandlet(
			TEXT("sr.Log.EditorCommandlet"),
			0,
			TEXT("Enables Star Rovers editor commandlet logs. 0=disabled, 1=enabled."));

		TAutoConsoleVariable<int32> CVarSRLogFacilityNetwork(
			TEXT("sr.Log.FacilityNetwork"),
			0,
			TEXT("Enables Star Rovers facility network logs. 0=disabled, 1=enabled."));

		TAutoConsoleVariable<int32> CVarSRLogGravity(
			TEXT("sr.Log.Gravity"),
			0,
			TEXT("Enables Star Rovers gravity and orbit visual logs. 0=disabled, 1=enabled."));

		TAutoConsoleVariable<int32> CVarSRLogMemory(
			TEXT("sr.Log.Memory"),
			0,
			TEXT("Enables Star Rovers memory diagnostic logs. 0=disabled, 1=enabled."));

		TAutoConsoleVariable<int32> CVarSRLogSolarSystem(
			TEXT("sr.Log.SolarSystem"),
			0,
			TEXT("Enables Star Rovers solar system generation logs. 0=disabled, 1=enabled."));

		TAutoConsoleVariable<int32> CVarSRLogSpaceLogistics(
			TEXT("sr.Log.SpaceLogistics"),
			0,
			TEXT("Enables Star Rovers space logistics logs. 0=disabled, 1=enabled."));

		TAutoConsoleVariable<int32> CVarSRLogStructure(
			TEXT("sr.Log.Structure"),
			0,
			TEXT("Enables Star Rovers structure placement logs. 0=disabled, 1=enabled."));

		TAutoConsoleVariable<int32> CVarSRLogSurface(
			TEXT("sr.Log.Surface"),
			0,
			TEXT("Enables Star Rovers surface grid and terrain logs. 0=disabled, 1=enabled."));

		TAutoConsoleVariable<int32> CVarSRLogTiming(
			TEXT("sr.Log.Timing"),
			0,
			TEXT("Enables Star Rovers timing summary logs. 0=disabled, 1=enabled."));

		TAutoConsoleVariable<int32> CVarSRLogUIClickTrace(
			TEXT("sr.Log.UIClickTrace"),
			0,
			TEXT("Enables Star Rovers UI click trace logs. 0=disabled, 1=enabled."));

		bool IsEnabled(const TAutoConsoleVariable<int32>& CVar)
		{
			return CVar.GetValueOnAnyThread() != 0;
		}
	}

	bool ShouldLog(ESRLogChannel Channel)
	{
		if (IsEnabled(CVarSRLogAll))
		{
			return true;
		}

		switch (Channel)
		{
		case ESRLogChannel::Assembly:
			return IsEnabled(CVarSRLogAssembly);
		case ESRLogChannel::Augment:
			return IsEnabled(CVarSRLogAugment);
		case ESRLogChannel::Camera:
			return IsEnabled(CVarSRLogCamera);
		case ESRLogChannel::Celestial:
			return IsEnabled(CVarSRLogCelestial);
		case ESRLogChannel::Conveyor:
			return IsEnabled(CVarSRLogConveyor);
		case ESRLogChannel::DynamicMesh:
			return IsEnabled(CVarSRLogDynamicMesh);
		case ESRLogChannel::EditorCommandlet:
			return IsEnabled(CVarSRLogEditorCommandlet);
		case ESRLogChannel::FacilityNetwork:
			return IsEnabled(CVarSRLogFacilityNetwork);
		case ESRLogChannel::Gravity:
			return IsEnabled(CVarSRLogGravity);
		case ESRLogChannel::Memory:
			return IsEnabled(CVarSRLogMemory);
		case ESRLogChannel::SolarSystem:
			return IsEnabled(CVarSRLogSolarSystem);
		case ESRLogChannel::SpaceLogistics:
			return IsEnabled(CVarSRLogSpaceLogistics);
		case ESRLogChannel::Structure:
			return IsEnabled(CVarSRLogStructure);
		case ESRLogChannel::Surface:
			return IsEnabled(CVarSRLogSurface);
		case ESRLogChannel::Timing:
			return IsEnabled(CVarSRLogTiming);
		case ESRLogChannel::UIClickTrace:
			return IsEnabled(CVarSRLogUIClickTrace);
		default:
			return false;
		}
	}

	const TCHAR* GetLogChannelName(ESRLogChannel Channel)
	{
		switch (Channel)
		{
		case ESRLogChannel::Assembly:
			return TEXT("Assembly");
		case ESRLogChannel::Augment:
			return TEXT("Augment");
		case ESRLogChannel::Camera:
			return TEXT("Camera");
		case ESRLogChannel::Celestial:
			return TEXT("Celestial");
		case ESRLogChannel::Conveyor:
			return TEXT("Conveyor");
		case ESRLogChannel::DynamicMesh:
			return TEXT("DynamicMesh");
		case ESRLogChannel::EditorCommandlet:
			return TEXT("EditorCommandlet");
		case ESRLogChannel::FacilityNetwork:
			return TEXT("FacilityNetwork");
		case ESRLogChannel::Gravity:
			return TEXT("Gravity");
		case ESRLogChannel::Memory:
			return TEXT("Memory");
		case ESRLogChannel::SolarSystem:
			return TEXT("SolarSystem");
		case ESRLogChannel::SpaceLogistics:
			return TEXT("SpaceLogistics");
		case ESRLogChannel::Structure:
			return TEXT("Structure");
		case ESRLogChannel::Surface:
			return TEXT("Surface");
		case ESRLogChannel::Timing:
			return TEXT("Timing");
		case ESRLogChannel::UIClickTrace:
			return TEXT("UIClickTrace");
		default:
			return TEXT("Unknown");
		}
	}
}
