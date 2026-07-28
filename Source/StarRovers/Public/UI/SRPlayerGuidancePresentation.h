#pragma once

#include "CoreMinimal.h"
#include "Simulation/SRRunMilestoneSubsystem.h"
#include "UI/SRResourceGlyph.h"
#include "UI/SRCelestialBodyOperationsSummary.h"
#include "UI/SRUITheme.h"

enum class ESRPlayerGuidanceActionKind : uint8
{
	None,
	ResumeSimulation,
	ActivateEmergencyProspecting,
	BuildExtractor,
	InspectExtractor,
	BuildFamilyProcessor,
	InspectFamilyProcessor,
	BuildStellarFuelFabricator,
	InspectStellarFuelFabricator,
	BuildHub,
	InspectHub,
	FocusPrimaryStar,
};

/** Runtime facts used to choose one actionable, non-modal player message. */
struct STARROVERS_API FSRPlayerGuidanceSnapshot
{
	bool bBlockingChoiceVisible = false;
	bool bHasFocusedActor = false;
	bool bCanConstructOnFocusedActor = false;
	bool bOperationsAvailable = false;
	bool bHasSelectedFacility = false;
	bool bSimulationPaused = false;
	FSRFirstFuelMilestoneSnapshot FirstFuelMilestone;

	int32 FacilityCount = 0;
	int32 ProcessingFacilityCount = 0;
	int32 ThrottledFacilityCount = 0;
	ESRCelestialBodyOperationsPressure OperationalPressure = ESRCelestialBodyOperationsPressure::Idle;
	int32 OperationalLoad = 0;
	int32 OperationalCapacity = 0;

	int32 HubCount = 0;
	int32 ConnectedRouteCount = 0;
	int32 BlockedRouteCount = 0;
	int32 FleetAvailableCapacity = 0;
	int32 FleetQueuedDepartureCount = 0;
};

/** Fully formatted banner state. An empty MessageId means no banner. */
struct STARROVERS_API FSRPlayerGuidanceMessage
{
	FName MessageId = NAME_None;
	FText CategoryText;
	FText TitleText;
	FText DetailText;
	FText ActionText;
	FText ToolTipText;
	bool bShowResourceGlyph = false;
	FSRResourceGlyphPresentation ResourceGlyph;
	ESRUIVisualState VisualState = ESRUIVisualState::Neutral;
	int32 Priority = 0;
	bool bTransient = false;
	ESRPlayerGuidanceActionKind ActionKind = ESRPlayerGuidanceActionKind::None;

	bool IsVisible() const
	{
		return !MessageId.IsNone() && !TitleText.IsEmpty();
	}
};

/** Deterministic priority rules shared by runtime UI and automation tests. */
class STARROVERS_API FSRPlayerGuidancePresentationBuilder final
{
public:
	static FSRPlayerGuidanceMessage Evaluate(const FSRPlayerGuidanceSnapshot& Snapshot);
};
