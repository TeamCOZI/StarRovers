#pragma once

#include "CoreMinimal.h"
#include "Logistics/SRSpaceLogisticsTypes.h"
#include "UI/SRResourceGlyph.h"
#include "UI/SRUITheme.h"

enum class ESRHubNetworkCondition : uint8
{
	Unavailable,
	Isolated,
	Ready,
	AtCapacity,
	Queued,
};

struct STARROVERS_API FSRHubNetworkPresentationInput
{
	bool bLogisticsAvailable = true;
	FSRFleetCapacityReportV2 FleetCapacity;
	int32 DestinationCount = 0;
	int32 ConnectedRouteCount = 0;
	int32 ActiveMissileCount = 0;
	int32 AutoLaunchSlotCount = 0;
};

struct STARROVERS_API FSRHubNetworkPresentation
{
	ESRHubNetworkCondition Condition = ESRHubNetworkCondition::Unavailable;
	ESRUIVisualState VisualState = ESRUIVisualState::Neutral;
	FText StatusLabel;
	FText FleetValue;
	FText FleetDetail;
	FText QueueValue;
	FText QueueDetail;
	FText MissileValue;
	FText MissileDetail;
};

enum class ESRHubRouteCardActivity : uint8
{
	NewDestination,
	Disabled,
	Idle,
	WaitingForCargo,
	WaitingForFleet,
	Traveling,
	Conditioning,
	Unloading,
	Blocked,
};

struct STARROVERS_API FSRHubRouteCardPresentationInput
{
	bool bHasRoute = false;
	bool bSelected = false;
	bool bSelectedHubIsSource = true;
	FString SourceName;
	FString DestinationName;
	FName RouteId = NAME_None;
	bool bEnabled = true;
	bool bReturnEmptyWhenNoCargo = true;
	int32 MaxCargoStackCount = 1;
	FName CargoResourceId = NAME_None;
	ESRSpaceLogisticsRouteProfileV2 RouteProfile = ESRSpaceLogisticsRouteProfileV2::NeutralShuttle;
	ESRConditionedTransitModuleV2 ConditionedTransitModule = ESRConditionedTransitModuleV2::None;
	ESRSpaceLogisticsHubRoutePhase Phase = ESRSpaceLogisticsHubRoutePhase::Idle;
	ESRSpaceLogisticsHubRouteDockSide CurrentDockSide = ESRSpaceLogisticsHubRouteDockSide::Source;
	int32 FleetQueuePosition = 0;
	float TravelProgressSeconds = 0.0f;
	float TravelDurationSeconds = 0.0f;
	float TravelProgressRatio = 0.0f;
	float ConditioningProgressSeconds = 0.0f;
	float ConditioningDurationSeconds = 0.0f;
	FSRResourceInstance Cargo;
};

struct STARROVERS_API FSRHubRouteCardPresentation
{
	ESRHubRouteCardActivity Activity = ESRHubRouteCardActivity::NewDestination;
	ESRUIVisualState VisualState = ESRUIVisualState::Neutral;
	FText DirectionLabel;
	FText LaneTitle;
	FText StatusLabel;
	FText PhaseDetail;
	FText CargoDetail;
	FText ProfileDetail;
	FText ModuleDetail;
	bool bShowCargoGlyph = false;
	FSRResourceGlyphPresentation CargoGlyph;
	float ProgressRatio = 0.0f;
	bool bShowProgress = false;
};

class STARROVERS_API FSRHubRoutePresentationBuilder
{
public:
	static FSRHubNetworkPresentation BuildNetwork(
		const FSRHubNetworkPresentationInput& Input);
	static FSRHubRouteCardPresentation BuildRoute(
		const FSRHubRouteCardPresentationInput& Input);
};
