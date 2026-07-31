#if WITH_DEV_AUTOMATION_TESTS

#include "Logistics/SRSpaceLogisticsTypes.h"

#include "Misc/AutomationTest.h"

namespace StarRovers::SpaceLogisticsPatternTests
{
	FSRResourceInstance MakeCargo()
	{
		FSRResourceInstance Cargo;
		Cargo.ResourceInstanceId = FName(TEXT("Cargo_1"));
		Cargo.ResourceId = FName(TEXT("StarIron"));
		Cargo.Pattern.SetGlyph(1, 2, ESRGlyphType::Metal);
		Cargo.Pattern.SetGlyph(3, 2, ESRGlyphType::Crystal);
		Cargo.SourcePatternId = FName(TEXT("Deposit_1"));
		Cargo.SourcePatternSeed = 42;
		Cargo.StackCount = 3;
		return Cargo;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSRSpaceLogisticsPatternSaveContractTest,
	"StarRovers.Logistics.Pattern.SaveContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSRSpaceLogisticsPatternSaveContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FSRSpaceLogisticsSaveData SaveData;
	TestEqual(
		TEXT("New space-logistics saves use the Pattern cargo version."),
		SaveData.Version,
		StarRovers::SpaceLogistics::PatternSave::CurrentVersion);
	TestTrue(
		TEXT("The ResourceId-only legacy version remains explicitly supported."),
		StarRovers::SpaceLogistics::PatternSave::IsSupportedVersion(
			StarRovers::SpaceLogistics::PatternSave::LegacyResourceFilterVersion));
	TestTrue(
		TEXT("The current Pattern version is supported."),
		StarRovers::SpaceLogistics::PatternSave::IsSupportedVersion(
			StarRovers::SpaceLogistics::PatternSave::CurrentVersion));
	TestFalse(
		TEXT("Unknown future versions are rejected."),
		StarRovers::SpaceLogistics::PatternSave::IsSupportedVersion(
			StarRovers::SpaceLogistics::PatternSave::CurrentVersion + 1));

	FSRPatternRoutingFilter VersionTwoFilter;
	VersionTwoFilter.ResourceId = FName(TEXT("StarIron"));
	VersionTwoFilter.MatchMode = ESRPatternRoutingMatchMode::ExactPattern;
	VersionTwoFilter.RequiredPattern = StarRovers::SpaceLogisticsPatternTests::MakeCargo().Pattern;
	const FSRPatternRoutingFilter MigratedVersionOneFilter =
		StarRovers::SpaceLogistics::PatternSave::ResolveRouteCargoFilter(
			StarRovers::SpaceLogistics::PatternSave::LegacyResourceFilterVersion,
			VersionTwoFilter,
			FName(TEXT("LegacyOre")));
	TestEqual(
		TEXT("Version 1 ResourceId becomes the version 2 resource criterion."),
		MigratedVersionOneFilter.ResourceId,
		FName(TEXT("LegacyOre")));
	TestTrue(
		TEXT("Version 1 migration keeps its historical any-Pattern behavior."),
		MigratedVersionOneFilter.MatchMode == ESRPatternRoutingMatchMode::AnyPattern);

	const FSRPatternRoutingFilter PreservedVersionTwoFilter =
		StarRovers::SpaceLogistics::PatternSave::ResolveRouteCargoFilter(
			StarRovers::SpaceLogistics::PatternSave::CurrentVersion,
			VersionTwoFilter,
			FName(TEXT("IgnoredLegacyOre")));
	TestTrue(
		TEXT("Version 2 preserves the exact Pattern Manifest."),
		PreservedVersionTwoFilter.ResourceId == VersionTwoFilter.ResourceId
			&& PreservedVersionTwoFilter.MatchMode == VersionTwoFilter.MatchMode
			&& PreservedVersionTwoFilter.RequiredPattern == VersionTwoFilter.RequiredPattern);

	const FSRResourceInstance Cargo = StarRovers::SpaceLogisticsPatternTests::MakeCargo();
	FSRSpaceLogisticsHubRouteSaveData RouteSaveData;
	RouteSaveData.CargoFilter = VersionTwoFilter;
	RouteSaveData.Cargo = Cargo;
	const FSRResourceInstance RestoredCargo = RouteSaveData.Cargo;
	TestTrue(TEXT("Cargo save data preserves all 25 Pattern cells."), RestoredCargo.Pattern == Cargo.Pattern);
	TestEqual(TEXT("Cargo save data preserves stack count."), RestoredCargo.StackCount, Cargo.StackCount);
	TestEqual(TEXT("Cargo save data preserves source Pattern identity."), RestoredCargo.SourcePatternId, Cargo.SourcePatternId);
	TestTrue(
		TEXT("The copied save payload remains transportable."),
		StarRovers::PatternRouting::IsValidPatternPayload(RestoredCargo));

	FSRSpaceLogisticsStarFuelMissileSaveData MissileSaveData;
	MissileSaveData.MissileId = FName(TEXT("Missile_1"));
	MissileSaveData.SourceHub.BodyActorName = FName(TEXT("Planet_1"));
	MissileSaveData.SourceHub.HubOccupantId = FName(TEXT("Hub_1"));
	MissileSaveData.TargetStarActorName = FName(TEXT("Star_1"));
	MissileSaveData.Cargo = Cargo;
	TestTrue(TEXT("A missile save requires a valid Pattern cargo payload."), MissileSaveData.IsValid());
	MissileSaveData.Cargo.Pattern.Reset();
	TestFalse(TEXT("A pre-Pattern missile payload is rejected on import."), MissileSaveData.IsValid());
	return true;
}

#endif
