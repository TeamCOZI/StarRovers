#pragma once

#include "CoreMinimal.h"
#include "Conveyor/SRConveyorNetworkGeometry.h"
#include "HAL/IConsoleManager.h"

namespace StarRovers::Conveyor
{
	inline bool ShouldForceGCOnConveyorDelete()
	{
		const IConsoleVariable* ForceGCCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("sr.MemoryDiagnostics.ForceGCOnConveyorDelete"));
		return ForceGCCVar && ForceGCCVar->GetInt() != 0;
	}
}
