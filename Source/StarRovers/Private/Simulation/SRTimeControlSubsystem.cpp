#include "Simulation/SRTimeControlSubsystem.h"

#include "Simulation/SRSimulationSettings.h"

void USRTimeControlSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (const USRSimulationSettings* SimulationSettings = GetDefault<USRSimulationSettings>())
	{
		SetSecondsPerPeriod(SimulationSettings->SecondsPerPeriod);
		SetSimulationPaused(SimulationSettings->bPauseSimulationOnRunStart);
	}
}

void USRTimeControlSubsystem::Tick(float DeltaTime)
{
	const float EffectiveTimeScale = GetEffectiveTimeScale();
	if (EffectiveTimeScale <= 0.0f || SecondsPerPeriod <= UE_SMALL_NUMBER)
	{
		return;
	}

	CycleProgressSeconds += FMath::Max(0.0f, DeltaTime) * EffectiveTimeScale;
	const int32 CyclesToAdvance = FMath::FloorToInt(CycleProgressSeconds / SecondsPerPeriod);
	if (CyclesToAdvance <= 0)
	{
		return;
	}

	CycleProgressSeconds -= SecondsPerPeriod * static_cast<float>(CyclesToAdvance);
	AdvanceGameCycles(CyclesToAdvance);
}

TStatId USRTimeControlSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USRTimeControlSubsystem, STATGROUP_Tickables);
}

void USRTimeControlSubsystem::PauseSimulation()
{
	SetSimulationPaused(true);
}

void USRTimeControlSubsystem::ResumeSimulation()
{
	bSimulationPaused = false;
	TimeScale = 1.0f;
}

void USRTimeControlSubsystem::SetSimulationPaused(bool bPaused)
{
	bSimulationPaused = bPaused;
}

void USRTimeControlSubsystem::SetTimeScale(float NewTimeScale)
{
	TimeScale = FMath::Max(0.0f, NewTimeScale);
}

void USRTimeControlSubsystem::SetSimulationSpeedPreset(float NewTimeScale)
{
	SetTimeScale(NewTimeScale);
	bSimulationPaused = false;
}

void USRTimeControlSubsystem::SetSecondsPerPeriod(float NewSecondsPerPeriod)
{
	SecondsPerPeriod = FMath::Max(0.0f, NewSecondsPerPeriod);
	if (SecondsPerPeriod <= UE_SMALL_NUMBER)
	{
		CycleProgressSeconds = 0.0f;
	}
	else
	{
		CycleProgressSeconds = FMath::Min(CycleProgressSeconds, SecondsPerPeriod);
	}
}

void USRTimeControlSubsystem::AdvanceGameCycles(int32 CycleCount)
{
	const int32 SafeCycleCount = FMath::Max(0, CycleCount);
	for (int32 CycleIndex = 0; CycleIndex < SafeCycleCount; ++CycleIndex)
	{
		++CurrentCycleIndex;
		OnGameCycleAdvanced.Broadcast(CurrentCycleIndex);
	}
}

void USRTimeControlSubsystem::ResetGameCycle(int32 NewCurrentCycleIndex)
{
	CurrentCycleIndex = FMath::Max(0, NewCurrentCycleIndex);
	CycleProgressSeconds = 0.0f;
}

float USRTimeControlSubsystem::GetTimeScale() const
{
	return TimeScale;
}

float USRTimeControlSubsystem::GetEffectiveTimeScale() const
{
	return bSimulationPaused ? 0.0f : TimeScale;
}

float USRTimeControlSubsystem::GetSecondsPerPeriod() const
{
	return SecondsPerPeriod;
}

bool USRTimeControlSubsystem::IsSimulationPaused() const
{
	return bSimulationPaused;
}

int32 USRTimeControlSubsystem::GetCurrentCycleIndex() const
{
	return CurrentCycleIndex;
}

float USRTimeControlSubsystem::GetCycleProgressSeconds() const
{
	return CycleProgressSeconds;
}

float USRTimeControlSubsystem::GetCycleProgressRatio() const
{
	return SecondsPerPeriod > UE_SMALL_NUMBER
		? FMath::Clamp(CycleProgressSeconds / SecondsPerPeriod, 0.0f, 1.0f)
		: 0.0f;
}

void USRTimeControlSubsystem::ExportSaveData(FSRTimeControlSaveData& OutSaveData) const
{
	OutSaveData = FSRTimeControlSaveData();
	OutSaveData.TimeScale = TimeScale;
	OutSaveData.SecondsPerPeriod = SecondsPerPeriod;
	OutSaveData.CycleProgressSeconds = CycleProgressSeconds;
	OutSaveData.CurrentCycleIndex = CurrentCycleIndex;
	OutSaveData.bSimulationPaused = bSimulationPaused;
}

bool USRTimeControlSubsystem::ImportSaveData(
	const FSRTimeControlSaveData& SaveData,
	FString& OutFailureReason)
{
	OutFailureReason.Reset();
	if (!SaveData.IsSupportedVersion())
	{
		OutFailureReason = FString::Printf(
			TEXT("Unsupported Time Control save version %d."),
			SaveData.Version);
		return false;
	}
	if (!FMath::IsFinite(SaveData.TimeScale)
		|| SaveData.TimeScale < 0.0f
		|| !FMath::IsFinite(SaveData.SecondsPerPeriod)
		|| SaveData.SecondsPerPeriod <= UE_SMALL_NUMBER
		|| !FMath::IsFinite(SaveData.CycleProgressSeconds)
		|| SaveData.CycleProgressSeconds < 0.0f
		// SetSecondsPerPeriod can legitimately clamp progress to the exact
		// boundary. The next simulation tick advances that pending cycle.
		|| SaveData.CycleProgressSeconds > SaveData.SecondsPerPeriod
		|| SaveData.CurrentCycleIndex < 0)
	{
		OutFailureReason = TEXT("Time Control save contains an invalid clock value.");
		return false;
	}

	TimeScale = SaveData.TimeScale;
	SecondsPerPeriod = SaveData.SecondsPerPeriod;
	CycleProgressSeconds = SaveData.CycleProgressSeconds;
	CurrentCycleIndex = SaveData.CurrentCycleIndex;
	bSimulationPaused = SaveData.bSimulationPaused;
	return true;
}
