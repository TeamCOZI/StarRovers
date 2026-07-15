#pragma once

#include "CoreMinimal.h"

namespace StarRovers::Logging
{
	enum class ESRLogChannel : uint8
	{
		Assembly,
		Augment,
		Camera,
		Celestial,
		Conveyor,
		DynamicMesh,
		EditorCommandlet,
		FacilityNetwork,
		Gravity,
		Memory,
		SolarSystem,
		SpaceLogistics,
		Structure,
		Surface,
		Timing,
		UIClickTrace,
	};

	STARROVERS_API bool ShouldLog(ESRLogChannel Channel);
	STARROVERS_API const TCHAR* GetLogChannelName(ESRLogChannel Channel);
}

#define SR_LOG(ChannelName, CategoryName, Verbosity, Format, ...) \
	do \
	{ \
		if (::StarRovers::Logging::ShouldLog(::StarRovers::Logging::ESRLogChannel::ChannelName)) \
		{ \
			UE_LOG(CategoryName, Verbosity, Format, ##__VA_ARGS__); \
		} \
	} while (false)

#define SR_LOG_ENABLED(ChannelName) \
	(::StarRovers::Logging::ShouldLog(::StarRovers::Logging::ESRLogChannel::ChannelName))
