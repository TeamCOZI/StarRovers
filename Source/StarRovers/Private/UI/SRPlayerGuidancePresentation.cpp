#include "UI/SRPlayerGuidancePresentation.h"

namespace
{
	FSRPlayerGuidanceMessage MakeMessage(
		const TCHAR* MessageId,
		const FText& Category,
		const FText& Title,
		const FText& Detail,
		const FText& Action,
		ESRUIVisualState VisualState,
		int32 Priority,
		ESRPlayerGuidanceActionKind ActionKind = ESRPlayerGuidanceActionKind::None,
		const FText& ToolTip = FText::GetEmpty(),
		const FSRResourceGlyphPresentation* ResourceGlyph = nullptr)
	{
		FSRPlayerGuidanceMessage Message;
		Message.MessageId = FName(MessageId);
		Message.CategoryText = Category;
		Message.TitleText = Title;
		Message.DetailText = Detail;
		Message.ActionText = Action;
		Message.VisualState = VisualState;
		Message.Priority = Priority;
		Message.ActionKind = ActionKind;
		Message.ToolTipText = ToolTip;
		if (ResourceGlyph && ResourceGlyph->bHasResource)
		{
			Message.bShowResourceGlyph = true;
			Message.ResourceGlyph = *ResourceGlyph;
		}
		return Message;
	}

	FText ResolveFamilyLabel(ESRResourceFamily Family)
	{
		if (Family == ESRResourceFamily::None)
		{
			return NSLOCTEXT("StarRoversGuidance", "AnyFamilyLabel", "Family");
		}
		return StaticEnum<ESRResourceFamily>()->GetDisplayNameTextByValue(
			static_cast<int64>(Family));
	}

	FText ResolveProximityLabel(float NormalizedProximity)
	{
		if (NormalizedProximity >= 0.67f)
		{
			return NSLOCTEXT("StarRoversGuidance", "NearStarLabel", "가까움");
		}
		if (NormalizedProximity >= 0.34f)
		{
			return NSLOCTEXT("StarRoversGuidance", "MediumStarDistanceLabel", "보통");
		}
		return NSLOCTEXT("StarRoversGuidance", "FarStarLabel", "멂");
	}

	FText BuildSystemScanToolTip(
		const FSRSystemScanSnapshot& Scan,
		const FSRSystemScanCandidate& Candidate)
	{
		return FText::Format(
			NSLOCTEXT(
				"StarRoversGuidance",
				"SystemScanRecommendationTooltip",
				"{0}개 건설 천체의 Card 광맥 {1}개를 비교했습니다.\n"
				"추천 점수 {2}/100 = 자원 Energy {3}/35 + 항성 접근성 {4}/25 + Capacity 여유 {5}/20 + Family 가공 {6}/10 + 인접 건설 공간 {7}/10.\n"
				"광맥 잔량 {8} · 사용 가능한 연료 Card 종류 {9}/{10}. Grade와 Spectrum은 족보 전략 정보이며 높고 낮음만으로 추천 점수를 주지 않습니다."),
			FText::AsNumber(Scan.ScannedConstructibleBodyCount),
			FText::AsNumber(Scan.ScannedCardDepositCount),
			FText::AsNumber(Candidate.Score.TotalScore),
			FText::AsNumber(Candidate.Score.ResourceQuality),
			FText::AsNumber(Candidate.Score.StarProximity),
			FText::AsNumber(Candidate.Score.CapacityHeadroom),
			FText::AsNumber(Candidate.Score.FamilyReadiness),
			FText::AsNumber(Candidate.Score.BuildAccess),
			FText::AsNumber(Candidate.DepositRemainingAmount),
			FText::AsNumber(Scan.AvailableRequiredCardResourceCount),
			FText::AsNumber(Scan.RequiredCardResourceCount));
	}

	FSRPlayerGuidanceMessage BuildFirstFuelMilestoneMessage(
		const FSRPlayerGuidanceSnapshot& Snapshot)
	{
		const FSRFirstFuelMilestoneSnapshot& Milestone = Snapshot.FirstFuelMilestone;
		const int32 DisplayStep = FMath::Clamp(
			Milestone.CompletedMilestoneCount + 1,
			1,
			Milestone.TotalMilestoneCount);
		const FText Category = FText::Format(
			NSLOCTEXT("StarRoversGuidance", "FirstFuelProgress", "첫 연료  {0}/{1}"),
			FText::AsNumber(DisplayStep),
			FText::AsNumber(Milestone.TotalMilestoneCount));
		const bool bHasFacilityTarget = IsValid(Milestone.TargetFacilityBodyActor.Get())
			&& !Milestone.TargetFacilityOccupantId.IsNone();
		const FText WaitingForWorld = NSLOCTEXT(
			"StarRoversGuidance",
			"WaitingForWorldAction",
			"천체 생성 완료를 기다리는 중");

		switch (Milestone.CurrentMilestone)
		{
		case ESRFirstFuelMilestone::PlaceExtractor:
			if (!Milestone.InitialSystemScan.bScanComplete)
			{
				return MakeMessage(
					TEXT("FirstFuel.SystemScanPending"),
					NSLOCTEXT("StarRoversGuidance", "SystemScanCategory", "SYSTEM SCAN"),
					NSLOCTEXT("StarRoversGuidance", "SystemScanPendingTitle", "시작 후보를 스캔하는 중입니다"),
					NSLOCTEXT("StarRoversGuidance", "SystemScanPendingDetail", "천체 표면과 Card 광맥 생성이 완료되면 첫 Line 후보를 바로 표시합니다."),
					WaitingForWorld,
					ESRUIVisualState::Neutral,
					50);
			}
			if (const FSRSystemScanCandidate* RecommendedCandidate =
				Milestone.InitialSystemScan.GetRecommendedCandidate())
			{
				const FSRResourceGlyphPresentation ResourceGlyph =
					FSRResourceGlyphPresentationBuilder::BuildIdentity(
						RecommendedCandidate->ResourceDisplayName,
						RecommendedCandidate->ResourceId,
						ESRResourceClass::Card,
						RecommendedCandidate->Family,
						RecommendedCandidate->SeedEnergy,
						RecommendedCandidate->Spectrum,
						RecommendedCandidate->Grade);
				return MakeMessage(
					TEXT("FirstFuel.PlaceExtractor"),
					FText::Format(
						NSLOCTEXT("StarRoversGuidance", "SystemScanProgress", "SYSTEM SCAN  ·  첫 연료 {0}/{1}"),
						FText::AsNumber(DisplayStep),
						FText::AsNumber(Milestone.TotalMilestoneCount)),
					FText::Format(
						NSLOCTEXT("StarRoversGuidance", "SystemScanRecommendationTitle", "추천: {0} · {1}"),
						RecommendedCandidate->BodyDisplayName,
						RecommendedCandidate->ResourceDisplayName),
					FText::Format(
						NSLOCTEXT(
							"StarRoversGuidance",
							"SystemScanRecommendationDetail",
							"Capacity +{0} · 항성 {1} · 적합도 {2}/100"),
						FText::AsNumber(RecommendedCandidate->OperationalHeadroom),
						ResolveProximityLabel(RecommendedCandidate->StarProximityNormalized),
						FText::AsNumber(RecommendedCandidate->Score.TotalScore)),
					NSLOCTEXT("StarRoversGuidance", "SystemScanRecommendationAction", "추천 광맥에서 시작"),
					ESRUIVisualState::Positive,
					50,
					ESRPlayerGuidanceActionKind::BuildExtractor,
					BuildSystemScanToolTip(
						Milestone.InitialSystemScan,
						*RecommendedCandidate),
					&ResourceGlyph);
			}
			if (Milestone.InitialProgressRecovery.bAvailable)
			{
				return MakeMessage(
					TEXT("FirstFuel.EmergencyProspecting"),
					NSLOCTEXT("StarRoversGuidance", "RecoveryCategory", "RECOVERY"),
					NSLOCTEXT("StarRoversGuidance", "RecoveryTitle", "초기 채굴 경로를 복구할 수 있습니다"),
					FText::Format(
						NSLOCTEXT(
							"StarRoversGuidance",
							"RecoveryDetail",
							"System Scan: 광맥 {0} · 고갈 {1} · 접근 불가 {2}. 첫 Card 전에만 1회 비상 광맥을 확보합니다."),
						FText::AsNumber(Milestone.InitialSystemScan.ScannedCardDepositCount),
						FText::AsNumber(Milestone.InitialSystemScan.DepletedCardDepositCount),
						FText::AsNumber(Milestone.InitialSystemScan.InaccessibleCardDepositCount)),
					NSLOCTEXT("StarRoversGuidance", "RecoveryAction", "비상 재탐사 1회 사용"),
					ESRUIVisualState::Warning,
					50,
					ESRPlayerGuidanceActionKind::ActivateEmergencyProspecting);
			}
			return MakeMessage(
				TEXT("FirstFuel.SystemScanNoCandidate"),
				NSLOCTEXT("StarRoversGuidance", "SystemScanAlertCategory", "SYSTEM SCAN ALERT"),
				Milestone.InitialProgressRecovery.bAttempted
					? NSLOCTEXT("StarRoversGuidance", "RecoveryFailedTitle", "비상 재탐사로도 채굴 경로를 만들지 못했습니다")
					: NSLOCTEXT("StarRoversGuidance", "SystemScanNoCandidateTitle", "접근 가능한 Card 광맥을 찾지 못했습니다"),
				NSLOCTEXT("StarRoversGuidance", "SystemScanNoCandidateDetail", "광맥 인접 건설 공간, 유한 잔량, 해당 Family 가공 설비 접근성을 확인해야 합니다."),
				NSLOCTEXT("StarRoversGuidance", "SystemScanNoCandidateAction", "현재 Seed의 천체 생성 오류를 확인하세요"),
				ESRUIVisualState::Danger,
				50);

		case ESRFirstFuelMilestone::ExtractFirstCard:
			if (!bHasFacilityTarget)
			{
				return MakeMessage(
					TEXT("FirstFuel.RebuildExtractor"),
					Category,
					NSLOCTEXT("StarRoversGuidance", "RebuildExtractorTitle", "Card 채굴 경로가 끊겼습니다"),
					NSLOCTEXT("StarRoversGuidance", "RebuildExtractorDetail", "추천 광맥의 잔량과 인접 공간을 다시 확인하고 채굴기를 배치하세요."),
					NSLOCTEXT("StarRoversGuidance", "RebuildExtractorAction", "채굴기 다시 배치"),
					ESRUIVisualState::Warning,
					50,
					ESRPlayerGuidanceActionKind::BuildExtractor);
			}
			return MakeMessage(
				TEXT("FirstFuel.ExtractFirstCard"),
				Category,
				NSLOCTEXT("StarRoversGuidance", "ExtractFirstCardTitle", "첫 자원 카드 생산을 확인하세요"),
				Snapshot.bSimulationPaused
					? NSLOCTEXT("StarRoversGuidance", "ExtractPausedDetail", "Simulation이 정지되어 채굴 Cycle이 진행되지 않습니다.")
					: NSLOCTEXT("StarRoversGuidance", "ExtractRunningDetail", "채굴기의 첫 출력이 생기면 다음 단계가 자동으로 열립니다."),
				Snapshot.bSimulationPaused
					? NSLOCTEXT("StarRoversGuidance", "ResumeExtractionAction", "가동 시작")
					: NSLOCTEXT("StarRoversGuidance", "InspectExtractorAction", "채굴기 보기"),
				ESRUIVisualState::Info,
				50,
				Snapshot.bSimulationPaused
					? ESRPlayerGuidanceActionKind::ResumeSimulation
					: bHasFacilityTarget
						? ESRPlayerGuidanceActionKind::InspectExtractor
						: ESRPlayerGuidanceActionKind::BuildExtractor);

		case ESRFirstFuelMilestone::PlaceFamilyProcessor:
			return MakeMessage(
				TEXT("FirstFuel.PlaceFamilyProcessor"),
				Category,
				FText::Format(
					NSLOCTEXT("StarRoversGuidance", "PlaceFamilyProcessorTitle", "{0} 가공 설비를 연결하세요"),
					ResolveFamilyLabel(Milestone.FirstResourceFamily)),
				NSLOCTEXT("StarRoversGuidance", "PlaceFamilyProcessorDetail", "일반 가공은 카드의 Energy를 합연산으로 성장시킵니다."),
				NSLOCTEXT("StarRoversGuidance", "PlaceFamilyProcessorAction", "가공 설비 배치"),
				ESRUIVisualState::Positive,
				50,
				ESRPlayerGuidanceActionKind::BuildFamilyProcessor);

		case ESRFirstFuelMilestone::ProcessFirstCard:
			if (!bHasFacilityTarget)
			{
				return MakeMessage(
					TEXT("FirstFuel.RebuildFamilyProcessor"),
					Category,
					FText::Format(
						NSLOCTEXT("StarRoversGuidance", "RebuildProcessorTitle", "{0} 가공 경로가 끊겼습니다"),
						ResolveFamilyLabel(Milestone.FirstResourceFamily)),
					NSLOCTEXT("StarRoversGuidance", "RebuildProcessorDetail", "다른 Family 설비는 이 첫 Card Milestone을 충족하지 않습니다."),
					NSLOCTEXT("StarRoversGuidance", "RebuildProcessorAction", "맞는 가공 설비 배치"),
					ESRUIVisualState::Warning,
					50,
					ESRPlayerGuidanceActionKind::BuildFamilyProcessor);
			}
			return MakeMessage(
				TEXT("FirstFuel.ProcessFirstCard"),
				Category,
				NSLOCTEXT("StarRoversGuidance", "ProcessFirstCardTitle", "첫 카드의 Energy를 한 번 높이세요"),
				Snapshot.bSimulationPaused
					? NSLOCTEXT("StarRoversGuidance", "ProcessPausedDetail", "Simulation을 가동해야 연결된 가공 Cycle이 진행됩니다.")
					: NSLOCTEXT("StarRoversGuidance", "ProcessRunningDetail", "채굴기 출력과 Family 가공 설비의 입력을 연결하세요."),
				Snapshot.bSimulationPaused
					? NSLOCTEXT("StarRoversGuidance", "ResumeProcessingAction", "가동 시작")
					: NSLOCTEXT("StarRoversGuidance", "InspectProcessorAction", "가공 설비 보기"),
				ESRUIVisualState::Info,
				50,
				Snapshot.bSimulationPaused
					? ESRPlayerGuidanceActionKind::ResumeSimulation
					: bHasFacilityTarget
						? ESRPlayerGuidanceActionKind::InspectFamilyProcessor
						: ESRPlayerGuidanceActionKind::BuildFamilyProcessor);

		case ESRFirstFuelMilestone::PlaceStellarFuelFabricator:
			return MakeMessage(
				TEXT("FirstFuel.PlaceFabricator"),
				Category,
				NSLOCTEXT("StarRoversGuidance", "PlaceFabricatorTitle", "항성 연료 제작기를 배치하세요"),
				NSLOCTEXT("StarRoversGuidance", "PlaceFabricatorDetail", "카드 5장의 Grade와 Spectrum 족보를 마지막 한 번의 곱연산으로 합성합니다."),
				NSLOCTEXT("StarRoversGuidance", "PlaceFabricatorAction", "제작기 배치"),
				ESRUIVisualState::Positive,
				50,
				ESRPlayerGuidanceActionKind::BuildStellarFuelFabricator);

		case ESRFirstFuelMilestone::FabricateFirstStellarFuel:
			if (!bHasFacilityTarget)
			{
				return MakeMessage(
					TEXT("FirstFuel.RebuildFabricator"),
					Category,
					NSLOCTEXT("StarRoversGuidance", "RebuildFabricatorTitle", "항성 연료 제작 경로가 끊겼습니다"),
					NSLOCTEXT("StarRoversGuidance", "RebuildFabricatorDetail", "이전 배치 이력은 유지되며 제작기만 다시 연결하면 됩니다."),
					NSLOCTEXT("StarRoversGuidance", "RebuildFabricatorAction", "제작기 다시 배치"),
					ESRUIVisualState::Warning,
					50,
					ESRPlayerGuidanceActionKind::BuildStellarFuelFabricator);
			}
			return MakeMessage(
				TEXT("FirstFuel.FabricateFuel"),
				Category,
				NSLOCTEXT("StarRoversGuidance", "FabricateFuelTitle", "카드 5장으로 첫 항성 연료를 만드세요"),
				Snapshot.bSimulationPaused
					? NSLOCTEXT("StarRoversGuidance", "FabricatePausedDetail", "Simulation을 가동해야 제작기의 합성 Cycle이 진행됩니다.")
					: NSLOCTEXT("StarRoversGuidance", "FabricateRunningDetail", "제작기 Inspector에서 다섯 입력과 예상 족보를 확인하세요."),
				Snapshot.bSimulationPaused
					? NSLOCTEXT("StarRoversGuidance", "ResumeFabricationAction", "가동 시작")
					: NSLOCTEXT("StarRoversGuidance", "InspectFabricatorAction", "제작기 보기"),
				ESRUIVisualState::Info,
				50,
				Snapshot.bSimulationPaused
					? ESRPlayerGuidanceActionKind::ResumeSimulation
					: bHasFacilityTarget
						? ESRPlayerGuidanceActionKind::InspectStellarFuelFabricator
						: ESRPlayerGuidanceActionKind::BuildStellarFuelFabricator);

		case ESRFirstFuelMilestone::PlaceHub:
			return MakeMessage(
				TEXT("FirstFuel.PlaceHub"),
				Category,
				NSLOCTEXT("StarRoversGuidance", "PlaceHubTitle", "발사용 Hub를 준비하세요"),
				NSLOCTEXT("StarRoversGuidance", "PlaceHubDetail", "완성된 항성 연료는 Hub 입력에서 Missile로 발사됩니다."),
				NSLOCTEXT("StarRoversGuidance", "PlaceHubAction", "Hub 배치"),
				ESRUIVisualState::Positive,
				50,
				ESRPlayerGuidanceActionKind::BuildHub);

		case ESRFirstFuelMilestone::LaunchFirstStellarFuel:
			if (!bHasFacilityTarget)
			{
				return MakeMessage(
					TEXT("FirstFuel.RebuildHub"),
					Category,
					NSLOCTEXT("StarRoversGuidance", "RebuildHubTitle", "항성 발사 경로가 끊겼습니다"),
					NSLOCTEXT("StarRoversGuidance", "RebuildHubDetail", "완성된 연료는 보존됩니다. Hub를 다시 배치해 발사 경로를 복구하세요."),
					NSLOCTEXT("StarRoversGuidance", "RebuildHubAction", "Hub 다시 배치"),
					ESRUIVisualState::Warning,
					50,
					ESRPlayerGuidanceActionKind::BuildHub);
			}
			return MakeMessage(
				TEXT("FirstFuel.LaunchFuel"),
				Category,
				NSLOCTEXT("StarRoversGuidance", "LaunchFuelTitle", "완성된 연료를 항성으로 발사하세요"),
				NSLOCTEXT("StarRoversGuidance", "LaunchFuelDetail", "Hub Inspector에서 연료 입력을 확인하고 수동 발사 또는 자동 발사를 설정하세요."),
				NSLOCTEXT("StarRoversGuidance", "InspectHubAction", "Hub 열기"),
				ESRUIVisualState::Positive,
				50,
				bHasFacilityTarget
					? ESRPlayerGuidanceActionKind::InspectHub
					: ESRPlayerGuidanceActionKind::BuildHub);

		case ESRFirstFuelMilestone::DeliverFirstStellarFuel:
			return MakeMessage(
				TEXT("FirstFuel.DeliverFuel"),
				Category,
				NSLOCTEXT("StarRoversGuidance", "DeliverFuelTitle", "첫 연료 Missile이 항성으로 이동 중입니다"),
				Snapshot.bSimulationPaused
					? NSLOCTEXT("StarRoversGuidance", "DeliveryPausedDetail", "Simulation이 정지되어 Missile의 ETA가 줄어들지 않습니다.")
					: NSLOCTEXT("StarRoversGuidance", "DeliveryRunningDetail", "상단 생존 Rail의 도착량과 ETA를 확인하세요."),
				Snapshot.bSimulationPaused
					? NSLOCTEXT("StarRoversGuidance", "ResumeDeliveryAction", "가동 시작")
					: NSLOCTEXT("StarRoversGuidance", "FocusStarAction", "항성 보기"),
				ESRUIVisualState::Info,
				50,
				Snapshot.bSimulationPaused
					? ESRPlayerGuidanceActionKind::ResumeSimulation
					: ESRPlayerGuidanceActionKind::FocusPrimaryStar);

		case ESRFirstFuelMilestone::Complete:
		default:
			return FSRPlayerGuidanceMessage();
		}
	}
}

FSRPlayerGuidanceMessage FSRPlayerGuidancePresentationBuilder::Evaluate(
	const FSRPlayerGuidanceSnapshot& Snapshot)
{
	// A blocking choice already owns attention and explains its own next action.
	if (Snapshot.bBlockingChoiceVisible)
	{
		FSRPlayerGuidanceMessage SuppressedMessage;
		SuppressedMessage.MessageId = FName(TEXT("BlockingChoice"));
		SuppressedMessage.Priority = MAX_int32;
		return SuppressedMessage;
	}

	if (Snapshot.BlockedRouteCount > 0)
	{
		return MakeMessage(
			TEXT("BlockedRoutes"),
			NSLOCTEXT("StarRoversGuidance", "LogisticsAlertCategory", "LOGISTICS ALERT"),
			FText::Format(
				NSLOCTEXT("StarRoversGuidance", "BlockedRoutesTitle", "{0} inter-body route(s) are blocked"),
				FText::AsNumber(Snapshot.BlockedRouteCount)),
			NSLOCTEXT(
				"StarRoversGuidance",
				"BlockedRoutesDetail",
				"A dock, path, cargo contract, fleet reservation, or conditioned Module is unavailable."),
			NSLOCTEXT(
				"StarRoversGuidance",
				"BlockedRoutesAction",
				"NEXT  ·  Select a Hub and inspect the ROUTE BLOCKED card."),
			ESRUIVisualState::Danger,
			100);
	}

	if (Snapshot.OperationalPressure == ESRCelestialBodyOperationsPressure::OverCapacity)
	{
		return MakeMessage(
			TEXT("OverOperationalCapacity"),
			NSLOCTEXT("StarRoversGuidance", "CapacityAlertCategory", "CAPACITY ALERT"),
			NSLOCTEXT("StarRoversGuidance", "OverCapacityTitle", "Operational Load exceeds Capacity"),
			FText::Format(
				NSLOCTEXT("StarRoversGuidance", "OverCapacityDetail", "Active Load {0} / {1}; lower-priority Facilities are throttled."),
				FText::AsNumber(Snapshot.OperationalLoad),
				FText::AsNumber(Snapshot.OperationalCapacity)),
			NSLOCTEXT(
				"StarRoversGuidance",
				"OverCapacityAction",
				"NEXT  ·  Supply a Service Core, change priority, or disable an optional process."),
			ESRUIVisualState::Danger,
			95);
	}

	if (Snapshot.ThrottledFacilityCount > 0)
	{
		return MakeMessage(
			TEXT("CapacityThrottling"),
			NSLOCTEXT("StarRoversGuidance", "CapacityPressureCategory", "CAPACITY PRESSURE"),
			FText::Format(
				NSLOCTEXT("StarRoversGuidance", "CapacityPressureTitle", "{0} Facility(s) are running below full speed"),
				FText::AsNumber(Snapshot.ThrottledFacilityCount)),
			FText::Format(
				NSLOCTEXT("StarRoversGuidance", "CapacityPressureDetail", "Active Load {0} / {1}; Critical work receives Capacity first."),
				FText::AsNumber(Snapshot.OperationalLoad),
				FText::AsNumber(Snapshot.OperationalCapacity)),
			NSLOCTEXT(
				"StarRoversGuidance",
				"CapacityPressureAction",
				"NEXT  ·  Inspect priority in a Facility or add supplied Capacity."),
			ESRUIVisualState::Warning,
			85);
	}

	if (Snapshot.OperationalPressure == ESRCelestialBodyOperationsPressure::AtCapacity)
	{
		return MakeMessage(
			TEXT("NoCapacityHeadroom"),
			NSLOCTEXT("StarRoversGuidance", "NoHeadroomCategory", "CAPACITY PRESSURE"),
			NSLOCTEXT("StarRoversGuidance", "NoHeadroomTitle", "Operational Capacity has no headroom"),
			FText::Format(
				NSLOCTEXT("StarRoversGuidance", "NoHeadroomDetail", "Active Load {0} / {1}; one more simultaneous process will cause throttling."),
				FText::AsNumber(Snapshot.OperationalLoad),
				FText::AsNumber(Snapshot.OperationalCapacity)),
			NSLOCTEXT(
				"StarRoversGuidance",
				"NoHeadroomAction",
				"NEXT  ·  Add supplied Capacity before expanding the active Line."),
			ESRUIVisualState::Warning,
			82);
	}

	if (Snapshot.FleetQueuedDepartureCount > 0 && Snapshot.FleetAvailableCapacity <= 0)
	{
		return MakeMessage(
			TEXT("FleetQueueFull"),
			NSLOCTEXT("StarRoversGuidance", "FleetAlertCategory", "FLEET ALERT"),
			FText::Format(
				NSLOCTEXT("StarRoversGuidance", "FleetQueueTitle", "{0} departure(s) are waiting for Fleet Capacity"),
				FText::AsNumber(Snapshot.FleetQueuedDepartureCount)),
			NSLOCTEXT(
				"StarRoversGuidance",
				"FleetQueueDetail",
				"The busiest Hub has no free Fleet load for another departure."),
			NSLOCTEXT(
				"StarRoversGuidance",
				"FleetQueueAction",
				"NEXT  ·  Supply a Fleet Berth, reduce cargo stack size, or wait for a route to return."),
			ESRUIVisualState::Warning,
			80);
	}

	if (Snapshot.FirstFuelMilestone.bIsTracking
		&& !Snapshot.FirstFuelMilestone.IsComplete())
	{
		return BuildFirstFuelMilestoneMessage(Snapshot);
	}

	if (!Snapshot.bHasFocusedActor)
	{
		return MakeMessage(
			TEXT("ChooseWorld"),
			NSLOCTEXT("StarRoversGuidance", "FirstStepsCategory", "FIRST STEPS"),
			NSLOCTEXT("StarRoversGuidance", "ChooseWorldTitle", "Choose a world for the first automation Line"),
			NSLOCTEXT(
				"StarRoversGuidance",
				"ChooseWorldDetail",
				"Planets and moons expose their surface, resources, Load, and construction workspace when focused."),
			NSLOCTEXT(
				"StarRoversGuidance",
				"ChooseWorldAction",
				"NEXT  ·  Select a planet or moon from the Overview."),
			ESRUIVisualState::Info,
			30);
	}

	if (!Snapshot.bCanConstructOnFocusedActor)
	{
		return MakeMessage(
			TEXT("ChooseConstructibleWorld"),
			NSLOCTEXT("StarRoversGuidance", "FirstLineCategory", "FIRST LINE"),
			NSLOCTEXT("StarRoversGuidance", "ChooseConstructibleTitle", "This body is not a construction workspace"),
			NSLOCTEXT(
				"StarRoversGuidance",
				"ChooseConstructibleDetail",
				"The Star receives completed fuel; automation Facilities are placed on planets and moons."),
			NSLOCTEXT(
				"StarRoversGuidance",
				"ChooseConstructibleAction",
				"NEXT  ·  Focus a planet or moon with an accessible resource deposit."),
			ESRUIVisualState::Info,
			30);
	}

	if (!Snapshot.bOperationsAvailable)
	{
		return MakeMessage(
			TEXT("OperationsInitializing"),
			NSLOCTEXT("StarRoversGuidance", "OperationsCategory", "WORLD OPERATIONS"),
			NSLOCTEXT("StarRoversGuidance", "OperationsInitializingTitle", "Surface operations are initializing"),
			NSLOCTEXT(
				"StarRoversGuidance",
				"OperationsInitializingDetail",
				"Construction and Capacity data are not available for this focused body yet."),
			NSLOCTEXT(
				"StarRoversGuidance",
				"OperationsInitializingAction",
				"NEXT  ·  Wait for generation to finish or focus another world."),
			ESRUIVisualState::Neutral,
			25);
	}

	if (Snapshot.FacilityCount <= 0)
	{
		return MakeMessage(
			TEXT("BuildFirstLine"),
			NSLOCTEXT("StarRoversGuidance", "FirstLineBuildCategory", "FIRST LINE"),
			NSLOCTEXT("StarRoversGuidance", "BuildFirstLineTitle", "Start with a local extraction Line"),
			NSLOCTEXT(
				"StarRoversGuidance",
				"BuildFirstLineDetail",
				"A Miner creates the resource Card; a matching Family Facility begins additive Energy growth."),
			NSLOCTEXT(
				"StarRoversGuidance",
				"BuildFirstLineAction",
				"NEXT  ·  In the Build Dock choose EXTRACTION, place a Miner, then connect a Family process."),
			ESRUIVisualState::Positive,
			40);
	}

	if (Snapshot.HubCount > 0 && Snapshot.ConnectedRouteCount <= 0)
	{
		return MakeMessage(
			TEXT("ConnectFirstHub"),
			NSLOCTEXT("StarRoversGuidance", "NetworkGrowthCategory", "NETWORK GROWTH"),
			NSLOCTEXT("StarRoversGuidance", "ConnectFirstHubTitle", "This Hub is not connected to another world"),
			NSLOCTEXT(
				"StarRoversGuidance",
				"ConnectFirstHubDetail",
				"Inter-body Lines begin only after a source Hub and destination Hub share a route."),
			NSLOCTEXT(
				"StarRoversGuidance",
				"ConnectFirstHubAction",
				"NEXT  ·  Select the Hub, choose a destination, then review cargo and Fleet load."),
			ESRUIVisualState::Info,
			35);
	}

	if (Snapshot.ProcessingFacilityCount <= 0 && !Snapshot.bHasSelectedFacility)
	{
		return MakeMessage(
			TEXT("InspectIdleLine"),
			NSLOCTEXT("StarRoversGuidance", "LineCheckCategory", "LINE CHECK"),
			NSLOCTEXT("StarRoversGuidance", "InspectIdleLineTitle", "No Facility on this world is processing"),
			NSLOCTEXT(
				"StarRoversGuidance",
				"InspectIdleLineDetail",
				"The Facility Inspector identifies missing input, Recipe mismatch, disabled processing, and blocked output separately."),
			NSLOCTEXT(
				"StarRoversGuidance",
				"InspectIdleLineAction",
				"NEXT  ·  Select a Facility and follow INPUT → PROCESS → OUTPUT."),
			ESRUIVisualState::Info,
			20);
	}

	return FSRPlayerGuidanceMessage();
}
