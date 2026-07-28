#pragma once

#include "CoreMinimal.h"
#include "Logistics/SRSpaceLogisticsTypes.h"
#include "UI/SRCelestialBodyOperationsSummary.h"
#include "UI/SRUITheme.h"

class AActor;
class UWorld;

enum class ESRStrategicBottleneckKind : uint8
{
	None,
	RouteBlocked,
	OperationalOverload,
	OperationalThrottled,
	FleetQueue,
	FleetAtCapacity,
	OperationalAtCapacity,
	OperationalNearCapacity,
};

enum class ESRStrategicRouteCondition : uint8
{
	Disabled,
	Ready,
	Moving,
	Conditioning,
	FleetQueue,
	Blocked,
};

/** Pure input used by tests and by the world snapshot adapter. */
struct STARROVERS_API FSRStrategicBodySnapshot
{
	FName BodyKey = NAME_None;
	TWeakObjectPtr<AActor> BodyActor;
	FText BodyName;
	FSRCelestialBodyOperationsSummary Operations;
};

/** Only route facts required by the command overlay. */
struct STARROVERS_API FSRStrategicRouteSnapshot
{
	FName RouteId = NAME_None;
	FName SourceBodyKey = NAME_None;
	FName DestinationBodyKey = NAME_None;
	TWeakObjectPtr<AActor> SourceBodyActor;
	TWeakObjectPtr<AActor> DestinationBodyActor;
	FText SourceBodyName;
	FText DestinationBodyName;
	bool bEnabled = true;
	ESRSpaceLogisticsHubRoutePhase Phase = ESRSpaceLogisticsHubRoutePhase::Idle;
	ESRSpaceLogisticsHubRouteDockSide CurrentDockSide = ESRSpaceLogisticsHubRouteDockSide::Source;
	int32 FleetQueuePosition = 0;
};

struct STARROVERS_API FSRStrategicOverlayInput
{
	TArray<FSRStrategicBodySnapshot> Bodies;
	TArray<FSRStrategicRouteSnapshot> Routes;
};

struct STARROVERS_API FSRStrategicBodyPresentation
{
	FName BodyKey = NAME_None;
	TWeakObjectPtr<AActor> BodyActor;
	FText BodyName;
	FSRCelestialBodyOperationsSummary Operations;
	ESRStrategicBottleneckKind BottleneckKind = ESRStrategicBottleneckKind::None;
	ESRUIVisualState VisualState = ESRUIVisualState::Neutral;
	FText StatusLabel;
	FText ShortBadgeText;
	FText IssueDetailText;
	FText ToolTipText;
	int32 Priority = 0;
	int32 BlockedRouteCount = 0;
	int32 OutboundBlockedRouteCount = 0;
	int32 QueuedRouteCount = 0;
	bool bHasBottleneck = false;
};

struct STARROVERS_API FSRStrategicRoutePresentation
{
	FName RouteId = NAME_None;
	FName SourceBodyKey = NAME_None;
	FName DestinationBodyKey = NAME_None;
	TWeakObjectPtr<AActor> SourceBodyActor;
	TWeakObjectPtr<AActor> DestinationBodyActor;
	FText SourceBodyName;
	FText DestinationBodyName;
	ESRStrategicRouteCondition Condition = ESRStrategicRouteCondition::Ready;
	ESRUIVisualState VisualState = ESRUIVisualState::Neutral;
	FText StatusLabel;
	FText ToolTipText;
	int32 FleetQueuePosition = 0;
	bool bEnabled = true;
};

struct STARROVERS_API FSRStrategicOverlayPresentation
{
	TArray<FSRStrategicBodyPresentation> Bodies;
	TArray<FSRStrategicRoutePresentation> Routes;
	FName RecommendedBodyKey = NAME_None;
	TWeakObjectPtr<AActor> RecommendedBodyActor;
	ESRUIVisualState SummaryState = ESRUIVisualState::Positive;
	FText SummaryLabel;
	FText SummaryDetailText;
	FText FocusActionText;
	int32 CriticalBodyCount = 0;
	int32 WarningBodyCount = 0;
	int32 BlockedRouteCount = 0;
	int32 QueuedRouteCount = 0;
	bool bHasRecommendation = false;

	const FSRStrategicBodyPresentation* FindBody(FName BodyKey) const;
	const FSRStrategicBodyPresentation* FindBody(const AActor* BodyActor) const;
	const FSRStrategicRoutePresentation* FindRoute(FName RouteId) const;
};

/**
 * Read-only strategic scan. It ranks active hard stops before capacity risks,
 * but never changes facilities, priorities, routes, or simulation state.
 */
class STARROVERS_API FSRStrategicOverlayPresentationBuilder final
{
public:
	static FSRStrategicOverlayPresentation Build(const FSRStrategicOverlayInput& Input);
	static FSRStrategicOverlayPresentation BuildFromWorld(
		UWorld* World,
		const TArray<AActor*>& CelestialBodies);
};
