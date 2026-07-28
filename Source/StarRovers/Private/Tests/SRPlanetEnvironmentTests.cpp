#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Automation/SRResourceDataAsset.h"
#include "Celestial/SRCelestialBody.h"
#include "Celestial/SRPlanetDataAsset.h"
#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Misc/AutomationTest.h"
#include "Simulation/SRPlanetEnvironmentSelection.h"
#include "Simulation/SRSolarSystemGenerator.h"
#include "Structure/SRStructureDataAsset.h"
#include "Structure/SRStructureInstanceManagerComponent.h"
#include "Surface/SRPlanetTerrainGenerator.h"
#include "Surface/SRPlanetTerrainProfileDataAsset.h"

#if WITH_EDITOR
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#endif

namespace StarRovers::PlanetEnvironmentTests
{
	constexpr TCHAR GeneratorBlueprintPath[] =
		TEXT("/Game/StarRovers/Generation/Blueprints/BP_SolarSystemGenerator.BP_SolarSystemGenerator");

	const TArray<FString> PlanetObjectPaths = {
		TEXT("/Game/StarRovers/Celestial/DataAssets/Planets/DA_Planet_Temperate.DA_Planet_Temperate"),
		TEXT("/Game/StarRovers/Celestial/DataAssets/Planets/DA_Planet_AridDesert.DA_Planet_AridDesert"),
		TEXT("/Game/StarRovers/Celestial/DataAssets/Planets/DA_Planet_FrozenOcean.DA_Planet_FrozenOcean"),
		TEXT("/Game/StarRovers/Celestial/DataAssets/Planets/DA_Planet_BadLands.DA_Planet_BadLands"),
		TEXT("/Game/StarRovers/Celestial/DataAssets/Planets/DA_Planet_LavaOcean.DA_Planet_LavaOcean"),
		TEXT("/Game/StarRovers/Celestial/DataAssets/Planets/DA_Planet_ToxicWetland.DA_Planet_ToxicWetland"),
	};

	double MeasureOceanLevelWaterRatio(const USRPlanetDataAsset& Planet)
	{
		constexpr int32 SampleCount = 768;
		int32 WaterSampleCount = 0;
		for (int32 SampleIndex = 0; SampleIndex < SampleCount; ++SampleIndex)
		{
			const double Unit = (static_cast<double>(SampleIndex) + 0.5)
				/ static_cast<double>(SampleCount);
			const double Z = 1.0 - 2.0 * Unit;
			const double Radius = FMath::Sqrt(FMath::Max(0.0, 1.0 - Z * Z));
			const double Angle = UE_DOUBLE_TWO_PI * Unit * 193.0;
			const FVector Direction(
				Radius * FMath::Cos(Angle),
				Radius * FMath::Sin(Angle),
				Z);
			const FSRPlanetTerrainSample Sample = FSRPlanetTerrainGenerator::SampleTerrain(
				Direction,
				Planet.DynamicMeshGeneration);
			WaterSampleCount += FSRPlanetTerrainGenerator::IsOceanLevelWaterSample(Sample) ? 1 : 0;
		}
		return static_cast<double>(WaterSampleCount) / static_cast<double>(SampleCount);
	}

#if WITH_EDITOR
	UWorld* FindPIEWorld()
	{
		if (!GEngine)
		{
			return nullptr;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && IsValid(Context.World()))
			{
				return Context.World();
			}
		}
		return nullptr;
	}

	class FWaitForDiversePlanetCatalogPIECommand final : public IAutomationLatentCommand
	{
	public:
		explicit FWaitForDiversePlanetCatalogPIECommand(FAutomationTestBase& InTest)
			: Test(InTest)
			, StartTimeSeconds(FPlatformTime::Seconds())
		{
		}

		virtual bool Update() override
		{
			UWorld* PIEWorld = FindPIEWorld();
			if (IsValid(PIEWorld))
			{
				ASRSolarSystemGenerator* RuntimeGenerator = nullptr;
				for (TActorIterator<ASRSolarSystemGenerator> GeneratorIt(PIEWorld); GeneratorIt; ++GeneratorIt)
				{
					RuntimeGenerator = *GeneratorIt;
					break;
				}
				int32 PlanetCount = 0;
				TSet<FString> UniqueEnvironmentNames;
				for (TActorIterator<ASRCelestialBody> BodyIt(PIEWorld); BodyIt; ++BodyIt)
				{
					ASRCelestialBody* Body = *BodyIt;
					if (!IsValid(Body) || Body->GetBodyCategory() != ESRCelestialBodyCategory::Planet)
					{
						continue;
					}
					++PlanetCount;
					UniqueEnvironmentNames.Add(Body->GetData().VariableName.ToString());
				}

				if (PlanetCount >= 5
					&& IsValid(RuntimeGenerator)
					&& !RuntimeGenerator->IsRuntimeSystemGenerationInProgress())
				{
					Test.TestTrue(TEXT("SolarSystem creates between five and seven planets"),
						PlanetCount >= 5 && PlanetCount <= 7);
					Test.TestTrue(TEXT("One runtime system contains at least four planet environments"),
						UniqueEnvironmentNames.Num() >= 4);
					Test.AddInfo(FString::Printf(
						TEXT("PIE generated %d planets across %d environment types: %s"),
						PlanetCount,
						UniqueEnvironmentNames.Num(),
						*FString::Join(UniqueEnvironmentNames.Array(), TEXT(", "))));

					TSet<FName> SystemResourceIds;
					for (TActorIterator<ASRCelestialBody> BodyIt(PIEWorld); BodyIt; ++BodyIt)
					{
						ASRCelestialBody* Body = *BodyIt;
						if (!IsValid(Body)
							|| Body->GetBodyCategory() != ESRCelestialBodyCategory::Planet)
						{
							continue;
						}

						USRStructureInstanceManagerComponent* StructureManager =
							Body->FindComponentByClass<USRStructureInstanceManagerComponent>();
						Test.TestNotNull(TEXT("Every generated planet has a Structure Manager"), StructureManager);
						if (!IsValid(StructureManager))
						{
							continue;
						}

						TArray<FSRPlacedStructureInstance> PlacedStructures;
						StructureManager->GetPlacedStructures(PlacedStructures);
						TMap<FName, int32> DepositCountByResourceId;
						for (const FSRPlacedStructureInstance& PlacedStructure : PlacedStructures)
						{
							const USRStructureDataAsset* Structure = PlacedStructure.StructureDataAsset.Get();
							if (!PlacedStructure.bNaturalStructure || !IsValid(Structure))
							{
								continue;
							}
							const FSRStructureData StructureData = Structure->BuildData();
							const USRResourceDataAsset* Resource =
								StructureData.DepositResourceDataAsset.Get();
							if (!StructureData.bIsResourceDeposit
								|| !IsValid(Resource)
								|| Resource->ResourceDefinitionVersion
									< StarRovers::Resources::CurrentResourceDefinitionVersion)
							{
								continue;
							}
							++DepositCountByResourceId.FindOrAdd(Resource->ResourceId);
							SystemResourceIds.Add(Resource->ResourceId);
						}

						Test.TestEqual(
							*FString::Printf(TEXT("%s exposes exactly two fuel cards and one utility resource"), *Body->GetData().VariableName.ToString()),
							DepositCountByResourceId.Num(),
							3);
						for (const TPair<FName, int32>& DepositCount : DepositCountByResourceId)
						{
							Test.TestTrue(
								*FString::Printf(TEXT("%s guarantees at least four %s deposits"), *Body->GetData().VariableName.ToString(), *DepositCount.Key.ToString()),
								DepositCount.Value >= 4);
							Test.TestTrue(
								*FString::Printf(TEXT("%s caps %s deposits at ten"), *Body->GetData().VariableName.ToString(), *DepositCount.Key.ToString()),
								DepositCount.Value <= 10);
						}
					}

					for (const FName RequiredRuleId : RuntimeGenerator->GetRequiredSystemResourceRuleIds())
					{
						FString RequiredResourceId = RequiredRuleId.ToString();
						RequiredResourceId.RemoveFromStart(TEXT("ResourceV2."));
						Test.TestTrue(
							*FString::Printf(TEXT("PIE system contains required resource %s"), *RequiredResourceId),
							SystemResourceIds.Contains(FName(*RequiredResourceId)));
					}
					return true;
				}
			}

			constexpr double TimeoutSeconds = 90.0;
			if (FPlatformTime::Seconds() - StartTimeSeconds < TimeoutSeconds)
			{
				return false;
			}
			Test.AddError(TEXT("SolarSystem PIE did not create the configured planet set within 90 seconds."));
			return true;
		}

	private:
		FAutomationTestBase& Test;
		double StartTimeSeconds = 0.0;
	};
#endif
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRPlanetEnvironmentWeightedSelectionTest,
	"StarRovers.SolarSystem.PlanetEnvironment.WeightedSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRPlanetEnvironmentWeightedSelectionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TArray<const USRPlanetDataAsset*> Candidates;
	for (int32 Index = 0; Index < 6; ++Index)
	{
		USRPlanetDataAsset* Candidate = NewObject<USRPlanetDataAsset>();
		Candidate->VariableName = FText::FromString(FString::Printf(TEXT("Environment%d"), Index));
		Candidate->GenerationWeight = 1.0f + static_cast<float>(Index) * 0.1f;
		Candidates.Add(Candidate);
	}
	USRPlanetDataAsset* DisabledCandidate = NewObject<USRPlanetDataAsset>();
	DisabledCandidate->VariableName = FText::FromString(TEXT("Disabled"));
	DisabledCandidate->GenerationWeight = 0.0f;
	Candidates.Add(DisabledCandidate);
	const USRPlanetDataAsset* DuplicateCandidate = Candidates[0];
	Candidates.Add(DuplicateCandidate);

	FRandomStream FirstRandom(8128);
	FRandomStream SecondRandom(8128);
	TArray<const USRPlanetDataAsset*> FirstSelection;
	TArray<const USRPlanetDataAsset*> SecondSelection;
	FSRPlanetEnvironmentSelector::Select(Candidates, 7, 4, FirstRandom, FirstSelection);
	FSRPlanetEnvironmentSelector::Select(Candidates, 7, 4, SecondRandom, SecondSelection);

	TestEqual(TEXT("The selector fills the requested seven planet slots"), FirstSelection.Num(), 7);
	TestEqual(TEXT("Equal seeds produce equally sized selections"),
		FirstSelection.Num(), SecondSelection.Num());
	TSet<const USRPlanetDataAsset*> UniqueSelection;
	for (int32 Index = 0; Index < FirstSelection.Num(); ++Index)
	{
		TestTrue(TEXT("Equal seeds produce the same ordered environment selection"),
			SecondSelection.IsValidIndex(Index) && FirstSelection[Index] == SecondSelection[Index]);
		TestTrue(TEXT("A zero-weight environment is never selected"),
			FirstSelection[Index] != DisabledCandidate);
		UniqueSelection.Add(FirstSelection[Index]);
	}
	TestTrue(TEXT("The first runtime set guarantees at least four unique environments"),
		UniqueSelection.Num() >= 4);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRPlanetEnvironmentResourceCoverageSelectionTest,
	"StarRovers.SolarSystem.PlanetEnvironment.ResourceCoverageSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRPlanetEnvironmentResourceCoverageSelectionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const TArray<FName> RequiredRuleIds = {
		TEXT("ResourceV2.HeliosIron"),
		TEXT("ResourceV2.EchoQuartz"),
		TEXT("ResourceV2.VerdantSpore"),
		TEXT("ResourceV2.AuroraPlasma"),
		TEXT("ResourceV2.NullPearl"),
		TEXT("ResourceV2.CommonOre"),
		TEXT("ResourceV2.BiomassFeedstock"),
	};

	auto MakeEnvironment = [](const TCHAR* Name, const TArray<FName>& RuleIds)
	{
		USRPlanetDataAsset* Planet = NewObject<USRPlanetDataAsset>();
		Planet->VariableName = FText::FromString(Name);
		Planet->GenerationWeight = 1.0f;
		USRPlanetTerrainProfileDataAsset* Profile =
			NewObject<USRPlanetTerrainProfileDataAsset>(Planet);
		for (const FName RuleId : RuleIds)
		{
			FSRProfileNaturalStructureSpawnRule& Rule =
				Profile->ProfileNaturalStructureSpawnRules.AddDefaulted_GetRef();
			Rule.RuleId = RuleId;
			Rule.bEnabled = true;
			Rule.SpawnChancePerCell = 0.03f;
			Rule.MinimumGuaranteedCount = 1;
		}
		Planet->TerrainProfileDataAsset = Profile;
		return Planet;
	};

	TArray<const USRPlanetDataAsset*> Candidates = {
		MakeEnvironment(TEXT("Temperate"), { RequiredRuleIds[2], RequiredRuleIds[0], RequiredRuleIds[6] }),
		MakeEnvironment(TEXT("Arid"), { RequiredRuleIds[0], RequiredRuleIds[1], RequiredRuleIds[5] }),
		MakeEnvironment(TEXT("Frozen"), { RequiredRuleIds[1], RequiredRuleIds[4], RequiredRuleIds[5] }),
		MakeEnvironment(TEXT("Badlands"), { RequiredRuleIds[4], RequiredRuleIds[2], RequiredRuleIds[5] }),
		MakeEnvironment(TEXT("Lava"), { RequiredRuleIds[3], RequiredRuleIds[0], RequiredRuleIds[5] }),
		MakeEnvironment(TEXT("Toxic"), { RequiredRuleIds[2], RequiredRuleIds[3], RequiredRuleIds[6] }),
	};

	for (int32 Seed = 1; Seed <= 128; ++Seed)
	{
		FRandomStream FirstRandom(Seed);
		FRandomStream SecondRandom(Seed);
		TArray<const USRPlanetDataAsset*> FirstSelection;
		TArray<const USRPlanetDataAsset*> SecondSelection;
		FSRPlanetEnvironmentSelectionReport FirstReport;
		FSRPlanetEnvironmentSelectionReport SecondReport;
		FSRPlanetEnvironmentSelector::SelectWithResourceCoverage(
			Candidates, 5 + Seed % 3, 4, RequiredRuleIds,
			FirstRandom, FirstSelection, FirstReport);
		FSRPlanetEnvironmentSelector::SelectWithResourceCoverage(
			Candidates, 5 + Seed % 3, 4, RequiredRuleIds,
			SecondRandom, SecondSelection, SecondReport);

		TestTrue(TEXT("Every feasible seed satisfies the complete resource portfolio"),
			FirstReport.bResourceCoverageSatisfied);
		TestEqual(TEXT("The resource-complete selector fills the requested planet count"),
			FirstSelection.Num(), 5 + Seed % 3);
		TSet<const USRPlanetDataAsset*> UniqueSelection;
		for (const USRPlanetDataAsset* SelectedPlanet : FirstSelection)
		{
			UniqueSelection.Add(SelectedPlanet);
		}
		TestTrue(TEXT("Resource coverage retains at least four distinct environments"),
			UniqueSelection.Num() >= 4);
		TestEqual(TEXT("Equal seeds retain deterministic selection sizes"),
			FirstSelection.Num(), SecondSelection.Num());
		for (int32 Index = 0; Index < FirstSelection.Num(); ++Index)
		{
			TestTrue(TEXT("Equal seeds retain deterministic resource-complete ordering"),
				SecondSelection.IsValidIndex(Index)
				&& FirstSelection[Index] == SecondSelection[Index]);
		}
	}

	TArray<FName> ImpossibleRequirements = RequiredRuleIds;
	ImpossibleRequirements.Add(TEXT("ResourceV2.Unobtainium"));
	FRandomStream ImpossibleRandom(77);
	TArray<const USRPlanetDataAsset*> ImpossibleSelection;
	FSRPlanetEnvironmentSelectionReport ImpossibleReport;
	FSRPlanetEnvironmentSelector::SelectWithResourceCoverage(
		Candidates, 5, 4, ImpossibleRequirements,
		ImpossibleRandom, ImpossibleSelection, ImpossibleReport);
	TestFalse(TEXT("An impossible catalog reports a failed resource portfolio"),
		ImpossibleReport.bResourceCoverageSatisfied);
	TestTrue(TEXT("The report names the missing resource rule"),
		ImpossibleReport.MissingResourceRuleIds.Contains(TEXT("ResourceV2.Unobtainium")));
	TestEqual(TEXT("An impossible catalog still returns a best-effort playable planet set"),
		ImpossibleSelection.Num(), 5);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRPlanetEnvironmentAuthoredCatalogTest,
	"StarRovers.SolarSystem.PlanetEnvironment.AuthoredCatalog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRPlanetEnvironmentAuthoredCatalogTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace StarRovers::PlanetEnvironmentTests;
	TSet<const USRPlanetTerrainProfileDataAsset*> UniqueProfiles;
	TSet<FString> UniqueClimateSignatures;
	TMap<FString, double> WaterRatioByName;
	TMap<FName, int32> ResourceSourceCountByRuleId;
	for (const FString& PlanetPath : PlanetObjectPaths)
	{
		USRPlanetDataAsset* Planet = LoadObject<USRPlanetDataAsset>(nullptr, *PlanetPath);
		TestNotNull(*FString::Printf(TEXT("Authored planet loads: %s"), *PlanetPath), Planet);
		if (!IsValid(Planet))
		{
			continue;
		}
		TestNotNull(TEXT("Every planet environment has a cube-sphere shape"), Planet->ShapeDataAsset.Get());
		TestNotNull(TEXT("Every planet environment has a terrain profile"), Planet->TerrainProfileDataAsset.Get());
		TestTrue(TEXT("Every planet environment has positive generation weight"),
			Planet->GenerationWeight > 0.0f);
		if (IsValid(Planet->TerrainProfileDataAsset.Get()))
		{
			UniqueProfiles.Add(Planet->TerrainProfileDataAsset.Get());
			TestTrue(TEXT("Every environment profile exposes at least three biomes"),
				Planet->TerrainProfileDataAsset->Biomes.Num() >= 3);
			TestTrue(TEXT("Every environment preserves the seven Resource V2 deposit rules"),
				Planet->TerrainProfileDataAsset->ProfileNaturalStructureSpawnRules.Num() >= 7);
		}
		TSet<FName> EnabledResourceRuleIds;
		FSRPlanetEnvironmentSelector::GetEnabledResourceRuleIds(Planet, EnabledResourceRuleIds);
		TestEqual(TEXT("Every environment enables exactly two fuel cards and one utility resource"),
			EnabledResourceRuleIds.Num(), 3);
		for (const FName RuleId : EnabledResourceRuleIds)
		{
			++ResourceSourceCountByRuleId.FindOrAdd(RuleId);
			const FSRNaturalStructureSpawnRuleOverride* Override =
				Planet->ProfileNaturalStructureSpawnRuleOverrides.FindByPredicate(
					[RuleId](const FSRNaturalStructureSpawnRuleOverride& Candidate)
					{
						return Candidate.RuleId == RuleId;
					});
			TestTrue(TEXT("Every enabled environment resource has a positive guaranteed count"),
				Override && Override->MinimumGuaranteedCount > 0);
		}
		UniqueClimateSignatures.Add(FString::Printf(
			TEXT("%.3f:%.3f:%.3f:%.3f:%.3f"),
			Planet->DynamicMeshGeneration.OceanThreshold,
			Planet->DynamicMeshGeneration.TemperatureBias,
			Planet->DynamicMeshGeneration.MoistureBias,
			Planet->DynamicMeshGeneration.MountainStrength,
			Planet->DynamicMeshGeneration.RiverStrength));
		WaterRatioByName.Add(Planet->VariableName.ToString(), MeasureOceanLevelWaterRatio(*Planet));
	}

	TestEqual(TEXT("Six authored planet environments exist"), UniqueProfiles.Num(), 6);
	TestEqual(TEXT("The six environments have unique terrain/climate signatures"),
		UniqueClimateSignatures.Num(), 6);
	TestTrue(TEXT("Arid Desert remains effectively dry"),
		WaterRatioByName.FindRef(TEXT("Arid Desert")) < 0.02);
	TestTrue(TEXT("Badlands remains effectively dry"),
		WaterRatioByName.FindRef(TEXT("Badlands")) < 0.02);
	TestTrue(TEXT("Toxic Wetland reserves a substantial water network"),
		WaterRatioByName.FindRef(TEXT("Toxic Wetland")) > 0.25);
	TestTrue(TEXT("Frozen Ocean is predominantly water or ice shelf"),
		WaterRatioByName.FindRef(TEXT("Frozen Ocean")) > 0.55);
	TestTrue(TEXT("Lava Ocean is predominantly magma ocean"),
		WaterRatioByName.FindRef(TEXT("Lava Ocean")) > 0.55);
	const TArray<FName> RequiredResourceRuleIds = {
		TEXT("ResourceV2.HeliosIron"),
		TEXT("ResourceV2.EchoQuartz"),
		TEXT("ResourceV2.VerdantSpore"),
		TEXT("ResourceV2.AuroraPlasma"),
		TEXT("ResourceV2.NullPearl"),
		TEXT("ResourceV2.CommonOre"),
		TEXT("ResourceV2.BiomassFeedstock"),
	};
	for (const FName RuleId : RequiredResourceRuleIds)
	{
		TestTrue(
			*FString::Printf(TEXT("The catalog provides at least two source environments for %s"), *RuleId.ToString()),
			ResourceSourceCountByRuleId.FindRef(RuleId) >= 2);
	}

	UBlueprint* GeneratorBlueprint = LoadObject<UBlueprint>(nullptr, GeneratorBlueprintPath);
	ASRSolarSystemGenerator* GeneratorCDO = IsValid(GeneratorBlueprint)
		&& IsValid(GeneratorBlueprint->GeneratedClass)
		? Cast<ASRSolarSystemGenerator>(GeneratorBlueprint->GeneratedClass->GetDefaultObject())
		: nullptr;
	TestNotNull(TEXT("BP_SolarSystemGenerator loads"), GeneratorCDO);
	if (IsValid(GeneratorCDO))
	{
		TestEqual(TEXT("The Blueprint default catalog contains all six environments"),
			GeneratorCDO->GetPlanetEnvironmentCatalog().Num(), 6);
		TestEqual(TEXT("The Blueprint default guarantees four unique planet types"),
			GeneratorCDO->GetMinimumUniquePlanetTypes(), 4);
		TestEqual(TEXT("The Blueprint default requires all five cards and both utility resources"),
			GeneratorCDO->GetRequiredSystemResourceRuleIds().Num(), 7);
	}
	return !HasAnyErrors();
}

#if WITH_EDITOR

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRPlanetEnvironmentSolarSystemPIETest,
	"StarRovers.SolarSystem.PlanetEnvironment.PIE.CatalogDiversity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRPlanetEnvironmentSolarSystemPIETest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Levels/SolarSystem")));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(
		StarRovers::PlanetEnvironmentTests::FWaitForDiversePlanetCatalogPIECommand(*this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif

#endif
