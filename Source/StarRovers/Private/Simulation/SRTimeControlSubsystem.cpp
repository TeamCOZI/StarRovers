#include "Simulation/SRTimeControlSubsystem.h"

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
