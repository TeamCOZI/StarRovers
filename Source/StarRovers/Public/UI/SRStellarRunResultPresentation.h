#pragma once

#include "CoreMinimal.h"
#include "Simulation/SRStellarRunContract.h"

/** Data-only result snapshot so terminal UI can be tested without a PIE world. */
struct STARROVERS_API FSRStellarRunResultSnapshot
{
	bool bHasStar = false;
	FSRStellarRunProgress RunProgress;
	double StoredFuel = 0.0;
	double ReferenceFuel = 0.0;
};

/** Text and palette selected for a terminal stellar Run outcome. */
struct STARROVERS_API FSRStellarRunResultPresentation
{
	FText TitleText;
	FText SubtitleText;
	FText DetailText;
	FLinearColor BackgroundColor = FLinearColor(0.010f, 0.006f, 0.008f, 0.92f);
	FLinearColor TitleColor = FLinearColor(1.0f, 0.30f, 0.20f, 1.0f);
	FLinearColor SubtitleColor = FLinearColor(0.96f, 0.90f, 0.82f, 1.0f);
	FLinearColor DetailColor = FLinearColor(0.74f, 0.78f, 0.82f, 1.0f);
	bool bVictory = false;
};

class STARROVERS_API FSRStellarRunResultPresentationBuilder final
{
public:
	static FSRStellarRunResultPresentation Build(
		const FSRStellarRunResultSnapshot& Snapshot);
};
