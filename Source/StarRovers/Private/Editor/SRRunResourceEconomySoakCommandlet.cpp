#include "Editor/SRRunResourceEconomySoakCommandlet.h"

#if WITH_EDITOR

#include "Engine/Blueprint.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Simulation/SRResourceEconomySoak.h"
#include "Simulation/SRSolarSystemGenerator.h"

namespace
{
	constexpr TCHAR ResourceEconomyGeneratorBlueprintPath[] =
		TEXT("/Game/StarRovers/Generation/Blueprints/BP_SolarSystemGenerator.BP_SolarSystemGenerator");
}

USRRunResourceEconomySoakCommandlet::USRRunResourceEconomySoakCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	ShowErrorCount = true;
}

int32 USRRunResourceEconomySoakCommandlet::Main(const FString& Params)
{
	UBlueprint* GeneratorBlueprint = LoadObject<UBlueprint>(nullptr, ResourceEconomyGeneratorBlueprintPath);
	const ASRSolarSystemGenerator* GeneratorCDO = IsValid(GeneratorBlueprint)
		&& IsValid(GeneratorBlueprint->GeneratedClass)
		? Cast<ASRSolarSystemGenerator>(GeneratorBlueprint->GeneratedClass->GetDefaultObject())
		: nullptr;
	if (!IsValid(GeneratorCDO))
	{
		UE_LOG(LogTemp, Error, TEXT("[ResourceEconomySoak] Could not load BP_SolarSystemGenerator defaults."));
		return 1;
	}

	FSRResourceEconomySoakRules Rules;
	Rules.MinimumPlanetCount = GeneratorCDO->GetMinimumPlanetCount();
	Rules.MaximumPlanetCount = GeneratorCDO->GetMaximumPlanetCount();
	Rules.MinimumUniquePlanetTypes = GeneratorCDO->GetMinimumUniquePlanetTypes();
	FParse::Value(*Params, TEXT("FirstSeed="), Rules.FirstSeed);
	FParse::Value(*Params, TEXT("SeedCount="), Rules.SeedCount);

	FString OutputPath;
	FParse::Value(*Params, TEXT("Output="), OutputPath);
	if (OutputPath.IsEmpty())
	{
		OutputPath = FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Balance/ResourceV2SeedSoak.csv"));
	}
	else if (FPaths::IsRelative(OutputPath))
	{
		OutputPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), OutputPath);
	}
	FPaths::NormalizeFilename(OutputPath);

	TArray<const USRPlanetDataAsset*> PlanetCandidates;
	PlanetCandidates.Reserve(GeneratorCDO->GetPlanetEnvironmentCatalog().Num());
	for (const TObjectPtr<USRPlanetDataAsset>& Planet : GeneratorCDO->GetPlanetEnvironmentCatalog())
	{
		PlanetCandidates.Add(Planet.Get());
	}

	const FSRResourceEconomySoakReport Report = FSRResourceEconomySoakModel::Run(
		PlanetCandidates,
		GeneratorCDO->GetRequiredSystemResourceRuleIds(),
		Rules);
	UE_LOG(LogTemp, Display, TEXT("[ResourceEconomySoak] %s"), *Report.BuildSummaryString());
	for (const TPair<FName, FSRResourceEconomyRuleSoakRange>& Pair : Report.RuleRanges)
	{
		UE_LOG(LogTemp, Display,
			TEXT("[ResourceEconomySoak] %s Sources=%d-%d Guaranteed=%d-%d Potential=%d-%d"),
			*Pair.Key.ToString(),
			Pair.Value.MinimumSourcePlanetCount,
			Pair.Value.MaximumSourcePlanetCount,
			Pair.Value.MinimumGuaranteedDepositCount,
			Pair.Value.MaximumGuaranteedDepositCount,
			Pair.Value.MinimumPotentialDepositCount,
			Pair.Value.MaximumPotentialDepositCount);
	}
	for (const TPair<FString, int32>& Pair : Report.EnvironmentAppearanceCount)
	{
		UE_LOG(LogTemp, Display,
			TEXT("[ResourceEconomySoak] Environment %s Systems=%d/%d"),
			*Pair.Key,
			Pair.Value,
			Report.SeedResults.Num());
	}

	IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutputPath), true);
	const bool bSaved = FFileHelper::SaveStringToFile(
		Report.BuildCsv(),
		*OutputPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	if (!bSaved)
	{
		UE_LOG(LogTemp, Error, TEXT("[ResourceEconomySoak] Could not write %s"), *OutputPath);
		return 1;
	}
	UE_LOG(LogTemp, Display, TEXT("[ResourceEconomySoak] CSV=%s"), *OutputPath);
	if (!Report.bPassed)
	{
		UE_LOG(LogTemp, Error, TEXT("[ResourceEconomySoak] FAILED: %s"), *Report.FailureReason);
		return 1;
	}

	UE_LOG(LogTemp, Display, TEXT("[ResourceEconomySoak] PASSED"));
	return 0;
}

#endif
