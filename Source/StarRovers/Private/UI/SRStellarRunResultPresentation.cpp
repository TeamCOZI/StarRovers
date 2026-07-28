#include "UI/SRStellarRunResultPresentation.h"

namespace
{
	FString FormatRunResultCompactNumber(double Value)
	{
		const double SafeValue = FMath::IsFinite(Value) ? FMath::Max(0.0, Value) : 0.0;
		if (SafeValue >= 1000000.0)
		{
			return FString::Printf(TEXT("%.1fM"), SafeValue / 1000000.0);
		}
		if (SafeValue >= 1000.0)
		{
			return FString::Printf(TEXT("%.1fK"), SafeValue / 1000.0);
		}
		return FString::Printf(TEXT("%lld"), FMath::RoundToInt64(SafeValue));
	}

	FString FormatRunResultDuration(double Seconds)
	{
		const int32 RoundedSeconds = FMath::Max(
			0,
			FMath::RoundToInt(FMath::IsFinite(Seconds) ? Seconds : 0.0));
		const int32 Hours = RoundedSeconds / 3600;
		const int32 Minutes = (RoundedSeconds % 3600) / 60;
		const int32 RemainingSeconds = RoundedSeconds % 60;
		return Hours > 0
			? FString::Printf(TEXT("%d:%02d:%02d"), Hours, Minutes, RemainingSeconds)
			: FString::Printf(TEXT("%02d:%02d"), Minutes, RemainingSeconds);
	}
}

FSRStellarRunResultPresentation FSRStellarRunResultPresentationBuilder::Build(
	const FSRStellarRunResultSnapshot& Snapshot)
{
	FSRStellarRunResultPresentation Presentation;
	if (Snapshot.bHasStar
		&& Snapshot.RunProgress.Outcome == ESRStellarRunOutcome::Victory)
	{
		Presentation.bVictory = true;
		Presentation.TitleText = NSLOCTEXT("StarRoversRunResult", "VictoryTitle", "VICTORY");
		Presentation.SubtitleText = NSLOCTEXT(
			"StarRoversRunResult",
			"VictorySubtitle",
			"항성 안정화에 성공했습니다.");
		Presentation.DetailText = FText::FromString(FString::Printf(
			TEXT("누적 항성 연료: %s / %s\n최종 유입: +%s/s (요구 %s/s)\n완료 시간: %s"),
			*FormatRunResultCompactNumber(Snapshot.RunProgress.TotalDeliveredFuel),
			*FormatRunResultCompactNumber(Snapshot.RunProgress.VictoryDeliveryTarget),
			*FormatRunResultCompactNumber(Snapshot.RunProgress.RecentIncomePerSecond),
			*FormatRunResultCompactNumber(Snapshot.RunProgress.RequiredIncomePerSecond),
			*FormatRunResultDuration(Snapshot.RunProgress.CompletionSimulationSeconds)));
		Presentation.BackgroundColor = FLinearColor(0.004f, 0.018f, 0.022f, 0.94f);
		Presentation.TitleColor = FLinearColor(0.25f, 1.0f, 0.74f, 1.0f);
		Presentation.SubtitleColor = FLinearColor(0.78f, 0.98f, 0.93f, 1.0f);
		Presentation.DetailColor = FLinearColor(0.70f, 0.86f, 0.84f, 1.0f);
		return Presentation;
	}

	Presentation.TitleText = NSLOCTEXT("StarRoversRunResult", "DefeatTitle", "DEFEAT");
	Presentation.SubtitleText = NSLOCTEXT(
		"StarRoversRunResult",
		"DefeatSubtitle",
		"주 항성이 초신성으로 붕괴했습니다.");
	Presentation.DetailText = Snapshot.bHasStar
		? FText::FromString(FString::Printf(
			TEXT("저장 항성 연료: %s / %s\n최종 상태: 초신성\n항성 안정화에 실패했습니다."),
			*FormatRunResultCompactNumber(Snapshot.StoredFuel),
			*FormatRunResultCompactNumber(Snapshot.ReferenceFuel)))
		: NSLOCTEXT(
			"StarRoversRunResult",
			"MissingStarDetail",
			"저장 연료가 고갈되어 항성 상태가 완전히 붕괴했습니다.");
	return Presentation;
}
