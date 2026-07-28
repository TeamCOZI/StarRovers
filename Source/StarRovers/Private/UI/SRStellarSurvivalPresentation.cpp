#include "UI/SRStellarSurvivalPresentation.h"

#include "Logistics/SRSpaceLogisticsSubsystem.h"
#include "Logistics/SRSpaceLogisticsStarFuelMissileProcessor.h"
#include "Simulation/SRCelestialBodyRegistrySubsystem.h"
#include "Simulation/SRTimeControlSubsystem.h"

namespace
{
	FString FormatCompactNumber(double Value)
	{
		const double AbsoluteValue = FMath::Abs(Value);
		if (AbsoluteValue >= 1000000.0)
		{
			return FString::Printf(TEXT("%.1fM"), Value / 1000000.0);
		}
		if (AbsoluteValue >= 1000.0)
		{
			return FString::Printf(TEXT("%.1fK"), Value / 1000.0);
		}
		return FString::Printf(TEXT("%lld"), FMath::RoundToInt64(Value));
	}

	FString FormatDuration(double Seconds)
	{
		const int32 RoundedSeconds = FMath::Max(0, FMath::CeilToInt(Seconds));
		const int32 Hours = RoundedSeconds / 3600;
		const int32 Minutes = (RoundedSeconds % 3600) / 60;
		const int32 RemainingSeconds = RoundedSeconds % 60;
		return Hours > 0
			? FString::Printf(TEXT("%dh %02dm"), Hours, Minutes)
			: FString::Printf(TEXT("%02d:%02d"), Minutes, RemainingSeconds);
	}

	ESRUIVisualState ResolveSurvivalState(const FSRStellarSurvivalSnapshot& Snapshot)
	{
		if (!Snapshot.bIsValid)
		{
			return ESRUIVisualState::Disabled;
		}
		if (Snapshot.RunProgress.Outcome == ESRStellarRunOutcome::Victory)
		{
			return ESRUIVisualState::Positive;
		}
		if (Snapshot.EvolutionStage == ESRStellarEvolutionStage::Supernova
			|| Snapshot.StoredFuel <= UE_DOUBLE_SMALL_NUMBER)
		{
			return ESRUIVisualState::Danger;
		}
		if (!Snapshot.bHasFiniteRunway)
		{
			return ESRUIVisualState::Positive;
		}
		if (Snapshot.SecuredFuelRunwaySeconds <= 60.0)
		{
			return ESRUIVisualState::Danger;
		}
		if (Snapshot.SecuredFuelRunwaySeconds <= 180.0)
		{
			return ESRUIVisualState::Warning;
		}
		return Snapshot.NetFuelPerSecond < -UE_DOUBLE_SMALL_NUMBER
			? ESRUIVisualState::Info
			: ESRUIVisualState::Positive;
	}

	FText BuildObjectiveText(const FSRStellarRunProgress& Progress)
	{
		if (!Progress.bFiniteVictoryEnabled)
		{
			return NSLOCTEXT("StarRoversSurvivalRail", "EndlessObjective", "목표 생존");
		}
		if (Progress.Outcome == ESRStellarRunOutcome::Victory)
		{
			return NSLOCTEXT("StarRoversSurvivalRail", "VictoryObjective", "목표 완료");
		}
		if (Progress.Outcome == ESRStellarRunOutcome::Defeat)
		{
			return NSLOCTEXT("StarRoversSurvivalRail", "DefeatObjective", "목표 실패");
		}

		switch (Progress.Phase)
		{
		case ESRStellarRunPhase::EmergencyIgnition:
			return FText::FromString(FString::Printf(
				TEXT("점화 %s/%s"),
				*FormatCompactNumber(Progress.TotalDeliveredFuel),
				*FormatCompactNumber(Progress.CurrentDeliveryTarget)));
		case ESRStellarRunPhase::SustainedSupply:
			return FText::FromString(FString::Printf(
				TEXT("공급 %s/%s"),
				*FormatCompactNumber(Progress.TotalDeliveredFuel),
				*FormatCompactNumber(Progress.CurrentDeliveryTarget)));
		case ESRStellarRunPhase::FinalStabilization:
			if (!Progress.bDeliveryTargetMet)
			{
				return FText::FromString(FString::Printf(
					TEXT("안정화 %s/%s"),
					*FormatCompactNumber(Progress.TotalDeliveredFuel),
					*FormatCompactNumber(Progress.CurrentDeliveryTarget)));
			}
			if (!Progress.bIncomeRequirementMet)
			{
				return FText::FromString(FString::Printf(
					TEXT("유입 %s/%s/s"),
					*FormatCompactNumber(Progress.RecentIncomePerSecond),
					*FormatCompactNumber(Progress.RequiredIncomePerSecond)));
			}
			return FText::FromString(FString::Printf(
				TEXT("유지 %s/%s"),
				*FormatDuration(Progress.SustainedIncomeProgressSeconds),
				*FormatDuration(Progress.RequiredSustainSeconds)));
		case ESRStellarRunPhase::Complete:
			return NSLOCTEXT("StarRoversSurvivalRail", "CompleteObjective", "목표 완료");
		default:
			return NSLOCTEXT("StarRoversSurvivalRail", "UnknownObjective", "목표 --");
		}
	}
}

FSRStellarSurvivalSnapshot FSRStellarSurvivalPresentationBuilder::BuildSnapshot(
	const FSRStellarFuelState& FuelState,
	const TArray<FSRStellarFuelInboundProjection>& InboundFuel,
	int32 CurrentCycleIndex,
	float SecondsUntilNextCycle,
	bool bSimulationPaused)
{
	FSRStellarSurvivalSnapshot Snapshot;
	Snapshot.bIsValid = true;
	Snapshot.EvolutionStage = FuelState.EvolutionStage;
	Snapshot.StoredFuel = FMath::Max(0.0, FuelState.StoredFuel);
	Snapshot.ReferenceFuelCapacity = FMath::Max(0.0, FuelState.InitialStageFuel);
	Snapshot.FuelProgressRatio = Snapshot.ReferenceFuelCapacity > UE_DOUBLE_SMALL_NUMBER
		? FMath::Clamp(
			static_cast<float>(Snapshot.StoredFuel / Snapshot.ReferenceFuelCapacity),
			0.0f,
			1.0f)
		: 0.0f;
	Snapshot.DemandPhase = FuelState.DemandPhase;
	Snapshot.FuelPressureRatio = FMath::Clamp(FuelState.FuelPressureRatio, 0.0f, 1.0f);
	Snapshot.RecentIncomePerSecond = FMath::Max(0.0, FuelState.RecentFuelIncomePerSecond);
	Snapshot.IncomeWindowSeconds = FMath::Max(1.0, FuelState.FuelIncomeWindowSeconds);
	Snapshot.ConsumptionPerSecond = FMath::Max(0.0, FuelState.RequiredFuelPerCycle);
	Snapshot.NetFuelPerSecond = Snapshot.RecentIncomePerSecond - Snapshot.ConsumptionPerSecond;
	Snapshot.CurrentCycleIndex = FMath::Max(0, CurrentCycleIndex);
	Snapshot.SecondsUntilNextCycle = FMath::Max(0.0f, SecondsUntilNextCycle);
	Snapshot.bSimulationPaused = bSimulationPaused;
	Snapshot.RunProgress = FuelState.RunProgress;

	const int32 NextCycleIndex = Snapshot.CurrentCycleIndex + 1;
	Snapshot.NextCycleConsumptionPerSecond = FuelState.bUsesStellarPressureCurveV2
		? FSRStellarDemandModel::CalculateDemandForCycleV2(
			FuelState.DemandCurveV2,
			NextCycleIndex)
		: FSRStellarDemandModel::CalculateLegacyNextCycleDemand(
			Snapshot.ConsumptionPerSecond,
			NextCycleIndex);
	Snapshot.bNextCycleCreatesDeficit =
		Snapshot.NextCycleConsumptionPerSecond > Snapshot.RecentIncomePerSecond + UE_DOUBLE_SMALL_NUMBER;

	TArray<FSRStellarFuelInboundProjection> ValidInboundFuel;
	ValidInboundFuel.Reserve(InboundFuel.Num());
	for (const FSRStellarFuelInboundProjection& Inbound : InboundFuel)
	{
		if (!FMath::IsFinite(Inbound.FuelAmount)
			|| Inbound.FuelAmount <= UE_DOUBLE_SMALL_NUMBER
			|| !FMath::IsFinite(Inbound.SecondsUntilArrival))
		{
			continue;
		}
		FSRStellarFuelInboundProjection& ValidInbound = ValidInboundFuel.Add_GetRef(Inbound);
		ValidInbound.FuelAmount = FMath::Max(0.0, ValidInbound.FuelAmount);
		ValidInbound.SecondsUntilArrival = FMath::Max(0.0f, ValidInbound.SecondsUntilArrival);
	}
	ValidInboundFuel.Sort(
		[](const FSRStellarFuelInboundProjection& Left, const FSRStellarFuelInboundProjection& Right)
		{
			return Left.SecondsUntilArrival < Right.SecondsUntilArrival;
		});

	Snapshot.InboundMissileCount = ValidInboundFuel.Num();
	for (const FSRStellarFuelInboundProjection& Inbound : ValidInboundFuel)
	{
		Snapshot.TotalInboundFuel += Inbound.FuelAmount;
	}
	if (!ValidInboundFuel.IsEmpty())
	{
		Snapshot.NextInboundFuel = ValidInboundFuel[0].FuelAmount;
		Snapshot.NextInboundSeconds = ValidInboundFuel[0].SecondsUntilArrival;
	}

	const double FuelDeficitPerSecond = FMath::Max(0.0, -Snapshot.NetFuelPerSecond);
	Snapshot.bHasFiniteRunway = FuelDeficitPerSecond > UE_DOUBLE_SMALL_NUMBER;
	if (!Snapshot.bHasFiniteRunway)
	{
		Snapshot.CurrentFuelRunwaySeconds = 0.0;
		Snapshot.SecuredFuelRunwaySeconds = 0.0;
		Snapshot.bNextInboundArrivesBeforeDepletion = !ValidInboundFuel.IsEmpty();
		return Snapshot;
	}

	Snapshot.CurrentFuelRunwaySeconds = Snapshot.StoredFuel / FuelDeficitPerSecond;
	Snapshot.SecuredFuelRunwaySeconds = Snapshot.CurrentFuelRunwaySeconds;
	Snapshot.bNextInboundArrivesBeforeDepletion = !ValidInboundFuel.IsEmpty()
		&& static_cast<double>(ValidInboundFuel[0].SecondsUntilArrival)
			<= Snapshot.CurrentFuelRunwaySeconds;

	double ProjectedFuel = Snapshot.StoredFuel;
	double ProjectionTime = 0.0;
	bool bDepletesBeforeRemainingArrivals = false;
	for (const FSRStellarFuelInboundProjection& Inbound : ValidInboundFuel)
	{
		const double ArrivalTime = FMath::Max(
			ProjectionTime,
			static_cast<double>(Inbound.SecondsUntilArrival));
		const double FuelBeforeArrival =
			ProjectedFuel - FuelDeficitPerSecond * (ArrivalTime - ProjectionTime);
		if (FuelBeforeArrival <= UE_DOUBLE_SMALL_NUMBER)
		{
			Snapshot.SecuredFuelRunwaySeconds =
				ProjectionTime + ProjectedFuel / FuelDeficitPerSecond;
			bDepletesBeforeRemainingArrivals = true;
			break;
		}

		ProjectedFuel = FuelBeforeArrival + Inbound.FuelAmount;
		ProjectionTime = ArrivalTime;
	}
	if (!bDepletesBeforeRemainingArrivals)
	{
		Snapshot.SecuredFuelRunwaySeconds =
			ProjectionTime + ProjectedFuel / FuelDeficitPerSecond;
	}
	return Snapshot;
}

bool FSRStellarSurvivalPresentationBuilder::TryBuildWorldSnapshot(
	const UWorld* World,
	FSRStellarSurvivalSnapshot& OutSnapshot)
{
	OutSnapshot = FSRStellarSurvivalSnapshot();
	if (!World)
	{
		return false;
	}

	const USRCelestialBodyRegistrySubsystem* RegistrySubsystem =
		World->GetSubsystem<USRCelestialBodyRegistrySubsystem>();
	const ASRStar* PrimaryStar = IsValid(RegistrySubsystem)
		? Cast<ASRStar>(RegistrySubsystem->GetPrimaryStarActor())
		: nullptr;
	if (!IsValid(PrimaryStar))
	{
		return false;
	}

	TArray<FSRStellarFuelInboundProjection> InboundFuel;
	if (const USRSpaceLogisticsSubsystem* LogisticsSubsystem =
		World->GetSubsystem<USRSpaceLogisticsSubsystem>())
	{
		TArray<FSRSpaceLogisticsStarFuelMissile> Missiles;
		LogisticsSubsystem->GetStarFuelMissiles(Missiles);
		InboundFuel.Reserve(Missiles.Num());
		for (const FSRSpaceLogisticsStarFuelMissile& Missile : Missiles)
		{
			if (!Missile.bEnabled || Missile.TargetStarActor.Get() != PrimaryStar)
			{
				continue;
			}
			const double FuelAmount =
				StarRovers::SpaceLogistics::StarFuelMissiles::CalculateMissileFuelValue(Missile.Cargo);
			if (FuelAmount <= UE_DOUBLE_SMALL_NUMBER)
			{
				continue;
			}
			FSRStellarFuelInboundProjection& Projection = InboundFuel.AddDefaulted_GetRef();
			Projection.FuelAmount = FuelAmount;
			Projection.SecondsUntilArrival = FMath::Max(
				0.0f,
				Missile.TravelDurationSeconds - Missile.TravelProgressSeconds);
		}
	}

	const USRTimeControlSubsystem* TimeControlSubsystem =
		World->GetSubsystem<USRTimeControlSubsystem>();
	const int32 CurrentCycleIndex = IsValid(TimeControlSubsystem)
		? TimeControlSubsystem->GetCurrentCycleIndex()
		: 0;
	const float SecondsUntilNextCycle = IsValid(TimeControlSubsystem)
		? FMath::Max(
			0.0f,
			TimeControlSubsystem->GetSecondsPerPeriod()
				- TimeControlSubsystem->GetCycleProgressSeconds())
		: 0.0f;
	const bool bSimulationPaused = IsValid(TimeControlSubsystem)
		&& TimeControlSubsystem->IsSimulationPaused();

	OutSnapshot = BuildSnapshot(
		PrimaryStar->GetStellarFuelState(),
		InboundFuel,
		CurrentCycleIndex,
		SecondsUntilNextCycle,
		bSimulationPaused);
	return true;
}

FSRStellarSurvivalPresentation FSRStellarSurvivalPresentationBuilder::BuildPresentation(
	const FSRStellarSurvivalSnapshot& Snapshot)
{
	FSRStellarSurvivalPresentation Presentation;
	if (!Snapshot.bIsValid)
	{
		Presentation.SurvivalText = NSLOCTEXT("StarRoversSurvivalRail", "WaitingForStar", "별 대기");
		Presentation.ObjectiveText = NSLOCTEXT("StarRoversSurvivalRail", "NoObjectiveData", "목표 --");
		Presentation.IncomeText = NSLOCTEXT("StarRoversSurvivalRail", "NoIncomeData", "유입 --");
		Presentation.ConsumptionText = NSLOCTEXT("StarRoversSurvivalRail", "NoConsumptionData", "소비 --");
		Presentation.NetText = NSLOCTEXT("StarRoversSurvivalRail", "NoNetData", "수지 --");
		Presentation.InboundText = NSLOCTEXT("StarRoversSurvivalRail", "NoInboundData", "도착 --");
		Presentation.CycleText = NSLOCTEXT("StarRoversSurvivalRail", "NoCycleData", "주기 대기");
		Presentation.DetailToolTipText = NSLOCTEXT(
			"StarRoversSurvivalRail",
			"WaitingForStarDetail",
			"항성 생성이 완료되면 생존 지표가 표시됩니다.");
		return Presentation;
	}

	Presentation.SurvivalVisualState = ResolveSurvivalState(Snapshot);
	Presentation.ObjectiveText = BuildObjectiveText(Snapshot.RunProgress);
	Presentation.ObjectiveVisualState = Snapshot.RunProgress.Outcome == ESRStellarRunOutcome::Victory
		? ESRUIVisualState::Positive
		: Snapshot.RunProgress.Outcome == ESRStellarRunOutcome::Defeat
			? ESRUIVisualState::Danger
			: Snapshot.RunProgress.Phase == ESRStellarRunPhase::FinalStabilization
				&& Snapshot.RunProgress.bDeliveryTargetMet
				&& !Snapshot.RunProgress.bIncomeRequirementMet
					? ESRUIVisualState::Warning
					: ESRUIVisualState::Info;
	if (Snapshot.RunProgress.Outcome == ESRStellarRunOutcome::Victory)
	{
		Presentation.SurvivalText = NSLOCTEXT("StarRoversSurvivalRail", "Stabilized", "별 안정화");
	}
	else if (Snapshot.EvolutionStage == ESRStellarEvolutionStage::Supernova)
	{
		Presentation.SurvivalText = NSLOCTEXT("StarRoversSurvivalRail", "Supernova", "별 초신성");
	}
	else if (!Snapshot.bHasFiniteRunway)
	{
		Presentation.SurvivalText = FText::FromString(
			Snapshot.bSimulationPaused ? TEXT("별 안정 · 정지") : TEXT("별 안정"));
	}
	else
	{
		const FString RunwayText = FormatDuration(Snapshot.SecuredFuelRunwaySeconds);
		Presentation.SurvivalText = FText::FromString(
			Snapshot.bSimulationPaused
				? FString::Printf(TEXT("별 %s · 정지"), *RunwayText)
				: FString::Printf(TEXT("별 %s"), *RunwayText));
	}

	Presentation.IncomeText = FText::FromString(FString::Printf(
		TEXT("유입 +%s/s"),
		*FormatCompactNumber(Snapshot.RecentIncomePerSecond)));
	Presentation.IncomeVisualState = Snapshot.RecentIncomePerSecond > UE_DOUBLE_SMALL_NUMBER
		? ESRUIVisualState::Positive
		: ESRUIVisualState::Neutral;

	Presentation.ConsumptionText = FText::FromString(FString::Printf(
		TEXT("소비 %s/s"),
		*FormatCompactNumber(Snapshot.ConsumptionPerSecond)));
	Presentation.ConsumptionVisualState = ESRUIVisualState::Warning;

	Presentation.NetText = FText::FromString(FString::Printf(
		TEXT("수지 %s%s/s"),
		Snapshot.NetFuelPerSecond >= 0.0 ? TEXT("+") : TEXT(""),
		*FormatCompactNumber(Snapshot.NetFuelPerSecond)));
	Presentation.NetVisualState = Snapshot.NetFuelPerSecond >= -UE_DOUBLE_SMALL_NUMBER
		? ESRUIVisualState::Positive
		: Presentation.SurvivalVisualState == ESRUIVisualState::Danger
			? ESRUIVisualState::Danger
			: ESRUIVisualState::Warning;

	if (Snapshot.InboundMissileCount > 0)
	{
		Presentation.InboundText = FText::FromString(FString::Printf(
			TEXT("도착 +%s · %s"),
			*FormatCompactNumber(Snapshot.NextInboundFuel),
			*FormatDuration(Snapshot.NextInboundSeconds)));
		Presentation.InboundVisualState = Snapshot.bNextInboundArrivesBeforeDepletion
			? ESRUIVisualState::Positive
			: ESRUIVisualState::Danger;
	}
	else
	{
		Presentation.InboundText = NSLOCTEXT("StarRoversSurvivalRail", "NoInbound", "도착 없음");
		Presentation.InboundVisualState = ESRUIVisualState::Neutral;
	}

	Presentation.CycleText = FText::FromString(FString::Printf(
		TEXT("주기 %d · %s 후 소비 %s/s"),
		Snapshot.CurrentCycleIndex,
		*FormatDuration(Snapshot.SecondsUntilNextCycle),
		*FormatCompactNumber(Snapshot.NextCycleConsumptionPerSecond)));
	Presentation.CycleVisualState = Snapshot.bNextCycleCreatesDeficit
		? ESRUIVisualState::Warning
		: ESRUIVisualState::Positive;

	const FString CurrentRunwayText = Snapshot.bHasFiniteRunway
		? FormatDuration(Snapshot.CurrentFuelRunwaySeconds)
		: FString(TEXT("안정"));
	const FString SecuredRunwayText = Snapshot.bHasFiniteRunway
		? FormatDuration(Snapshot.SecuredFuelRunwaySeconds)
		: FString(TEXT("안정"));
	Presentation.DetailToolTipText = FText::FromString(FString::Printf(
		TEXT("Run 목표: %s\n누적 전달 %s / %s · 요구 유입 %s/s · 유지 %s / %s\n항성 연료 %s / %s\n현재 보유분 생존 %s · 확정 도착 포함 %s\n최근 %.0f초 실제 유입 +%s/s · 현재 소비 %s/s · 순수지 %s%s/s\n다음 주기까지 %s · 예상 소비 %s/s\n비행 중 연료 Missile %d기 · 총 +%s"),
		*Presentation.ObjectiveText.ToString(),
		*FormatCompactNumber(Snapshot.RunProgress.TotalDeliveredFuel),
		*FormatCompactNumber(Snapshot.RunProgress.VictoryDeliveryTarget),
		*FormatCompactNumber(Snapshot.RunProgress.RequiredIncomePerSecond),
		*FormatDuration(Snapshot.RunProgress.SustainedIncomeProgressSeconds),
		*FormatDuration(Snapshot.RunProgress.RequiredSustainSeconds),
		*FormatCompactNumber(Snapshot.StoredFuel),
		*FormatCompactNumber(Snapshot.ReferenceFuelCapacity),
		*CurrentRunwayText,
		*SecuredRunwayText,
		Snapshot.IncomeWindowSeconds,
		*FormatCompactNumber(Snapshot.RecentIncomePerSecond),
		*FormatCompactNumber(Snapshot.ConsumptionPerSecond),
		Snapshot.NetFuelPerSecond >= 0.0 ? TEXT("+") : TEXT(""),
		*FormatCompactNumber(Snapshot.NetFuelPerSecond),
		*FormatDuration(Snapshot.SecondsUntilNextCycle),
		*FormatCompactNumber(Snapshot.NextCycleConsumptionPerSecond),
		Snapshot.InboundMissileCount,
		*FormatCompactNumber(Snapshot.TotalInboundFuel)));
	return Presentation;
}
