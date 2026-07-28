#include "Automation/SRResourceSystemContent.h"

#include "Automation/SRResourceInstanceOperations.h"

namespace
{
	constexpr double MaximumProcessTagEnergyDelta = 8.0;

	bool IsSupportedProcessTagTrigger(ESRProcessTagTriggerV2 Trigger)
	{
		switch (Trigger)
		{
		case ESRProcessTagTriggerV2::PositiveFamilyStateActivated:
		case ESRProcessTagTriggerV2::NegativeFamilyStateCleared:
		case ESRProcessTagTriggerV2::ProcessArchetypeChanged:
		case ESRProcessTagTriggerV2::FirstEnergyChangeAfterImport:
		case ESRProcessTagTriggerV2::FirstValidProcessOutsideOrigin:
			return true;
		default:
			return false;
		}
	}

	FSRProcessTagDefinitionV2 MakeProcessTag(
		const TCHAR* TagId,
		const FText& DisplayName,
		ESRProcessTagTriggerV2 Trigger,
		double EnergyDelta)
	{
		FSRProcessTagDefinitionV2 Definition;
		Definition.TagId = FName(TagId);
		Definition.DisplayName = DisplayName;
		Definition.Trigger = Trigger;
		Definition.EnergyDelta = EnergyDelta;
		Definition.TriggerCount = 1;
		return Definition;
	}

	const TArray<FSRProcessTagDefinitionV2>& GetProcessTags()
	{
		static const TArray<FSRProcessTagDefinitionV2> Definitions =
		{
			MakeProcessTag(TEXT("Overtone"), NSLOCTEXT("StarRoversResourceV2", "Overtone", "Overtone"), ESRProcessTagTriggerV2::PositiveFamilyStateActivated, 5.0),
			MakeProcessTag(TEXT("Reclamation"), NSLOCTEXT("StarRoversResourceV2", "Reclamation", "Reclamation"), ESRProcessTagTriggerV2::NegativeFamilyStateCleared, 7.0),
			MakeProcessTag(TEXT("Crosslink"), NSLOCTEXT("StarRoversResourceV2", "Crosslink", "Crosslink"), ESRProcessTagTriggerV2::ProcessArchetypeChanged, 4.0),
			MakeProcessTag(TEXT("LandingCharge"), NSLOCTEXT("StarRoversResourceV2", "LandingCharge", "Landing Charge"), ESRProcessTagTriggerV2::FirstEnergyChangeAfterImport, 5.0),
			MakeProcessTag(TEXT("PilgrimCharge"), NSLOCTEXT("StarRoversResourceV2", "PilgrimCharge", "Pilgrim Charge"), ESRProcessTagTriggerV2::FirstValidProcessOutsideOrigin, 6.0),
		};
		return Definitions;
	}

	FSRFuelImprintDefinitionV2 MakeFuelImprint(const TCHAR* ImprintId, const FText& DisplayName)
	{
		FSRFuelImprintDefinitionV2 Definition;
		Definition.ImprintId = FName(ImprintId);
		Definition.DisplayName = DisplayName;
		return Definition;
	}

	const TArray<FSRFuelImprintDefinitionV2>& GetFuelImprints()
	{
		static const TArray<FSRFuelImprintDefinitionV2> Definitions =
		{
			MakeFuelImprint(TEXT("TwinSeal"), NSLOCTEXT("StarRoversResourceV2", "TwinSeal", "Twin Seal")),
			MakeFuelImprint(TEXT("ConvergenceSeal"), NSLOCTEXT("StarRoversResourceV2", "ConvergenceSeal", "Convergence Seal")),
			MakeFuelImprint(TEXT("FoundrySeal"), NSLOCTEXT("StarRoversResourceV2", "FoundrySeal", "Foundry Seal")),
			MakeFuelImprint(TEXT("PilgrimSeal"), NSLOCTEXT("StarRoversResourceV2", "PilgrimSeal", "Pilgrim Seal")),
			MakeFuelImprint(TEXT("PrismaticCatalyst"), NSLOCTEXT("StarRoversResourceV2", "PrismaticCatalyst", "Prismatic Catalyst")),
		};
		return Definitions;
	}

	FSRReferenceResourceDefinitionV2 MakeResource(
		ESRResourceContentPresetV2 Preset,
		const TCHAR* ResourceId,
		const FText& DisplayName,
		const FText& Description,
		ESRResourceFamily Family,
		double SeedEnergy,
		ESRResourceSpectrum Spectrum,
		int32 Grade,
		int32 DepositTotalAmount)
	{
		FSRReferenceResourceDefinitionV2 Definition;
		Definition.Preset = Preset;
		Definition.ResourceId = FName(ResourceId);
		Definition.DisplayName = DisplayName;
		Definition.Description = Description;
		Definition.Family = Family;
		Definition.SeedEnergy = SeedEnergy;
		Definition.Spectrum = Spectrum;
		Definition.Grade = Grade;
		Definition.DepositTotalAmount = FMath::Max(1, DepositTotalAmount);
		return Definition;
	}

	const TArray<FSRReferenceResourceDefinitionV2>& GetResources()
	{
		static const TArray<FSRReferenceResourceDefinitionV2> Definitions =
		{
			MakeResource(ESRResourceContentPresetV2::HeliosIron, TEXT("HeliosIron"), NSLOCTEXT("StarRoversResourceV2", "HeliosIron", "Helios Iron"), NSLOCTEXT("StarRoversResourceV2", "HeliosIronDescription", "A red Metal card refined through thermal cycling."), ESRResourceFamily::Metal, 5.0, ESRResourceSpectrum::Red, 2, 120),
			MakeResource(ESRResourceContentPresetV2::EchoQuartz, TEXT("EchoQuartz"), NSLOCTEXT("StarRoversResourceV2", "EchoQuartz", "Echo Quartz"), NSLOCTEXT("StarRoversResourceV2", "EchoQuartzDescription", "A blue Crystal card that rewards controlled repetition."), ESRResourceFamily::Crystal, 4.0, ESRResourceSpectrum::Blue, 2, 120),
			MakeResource(ESRResourceContentPresetV2::VerdantSpore, TEXT("VerdantSpore"), NSLOCTEXT("StarRoversResourceV2", "VerdantSpore", "Verdant Spore"), NSLOCTEXT("StarRoversResourceV2", "VerdantSporeDescription", "A green Organic card renewed by growth cycles."), ESRResourceFamily::Organic, 3.0, ESRResourceSpectrum::Green, 4, 120),
			MakeResource(ESRResourceContentPresetV2::AuroraPlasma, TEXT("AuroraPlasma"), NSLOCTEXT("StarRoversResourceV2", "AuroraPlasma", "Aurora Plasma"), NSLOCTEXT("StarRoversResourceV2", "AuroraPlasmaDescription", "A yellow Plasma card balanced between amplification and grounding."), ESRResourceFamily::Plasma, 6.0, ESRResourceSpectrum::Yellow, 4, 120),
			MakeResource(ESRResourceContentPresetV2::NullPearl, TEXT("NullPearl"), NSLOCTEXT("StarRoversResourceV2", "NullPearl", "Null Pearl"), NSLOCTEXT("StarRoversResourceV2", "NullPearlDescription", "A red Void card that converts deliberate sacrifice into an echo."), ESRResourceFamily::Void, 2.0, ESRResourceSpectrum::Red, 4, 120),
		};
		return Definitions;
	}

	FSRUtilityResourceDefinitionV2 MakeUtilityResource(
		ESRResourceContentPresetV2 Preset,
		const TCHAR* ResourceId,
		const FText& DisplayName,
		const FText& Description,
		int32 DepositTotalAmount)
	{
		FSRUtilityResourceDefinitionV2 Definition;
		Definition.Preset = Preset;
		Definition.ResourceId = FName(ResourceId);
		Definition.DisplayName = DisplayName;
		Definition.Description = Description;
		Definition.DepositTotalAmount = FMath::Max(0, DepositTotalAmount);
		return Definition;
	}

	const TArray<FSRUtilityResourceDefinitionV2>& GetUtilityResources()
	{
		static const TArray<FSRUtilityResourceDefinitionV2> Definitions =
		{
			MakeUtilityResource(ESRResourceContentPresetV2::CommonOre, TEXT("CommonOre"), NSLOCTEXT("StarRoversResourceV2", "CommonOre", "Common Ore"), NSLOCTEXT("StarRoversResourceV2", "CommonOreDescription", "A common mineral feedstock used to fabricate Industrial Supply."), 180),
			MakeUtilityResource(ESRResourceContentPresetV2::BiomassFeedstock, TEXT("BiomassFeedstock"), NSLOCTEXT("StarRoversResourceV2", "BiomassFeedstock", "Biomass Feedstock"), NSLOCTEXT("StarRoversResourceV2", "BiomassFeedstockDescription", "A renewable organic feedstock used to fabricate Industrial Supply."), 180),
			MakeUtilityResource(ESRResourceContentPresetV2::IndustrialSupply, TEXT("IndustrialSupply"), NSLOCTEXT("StarRoversResourceV2", "IndustrialSupply", "Industrial Supply"), NSLOCTEXT("StarRoversResourceV2", "IndustrialSupplyDescription", "A utility resource consumed by Service Cores and Fleet Berths to sustain infrastructure capacity."), 0),
		};
		return Definitions;
	}

	FSRFacilityContentDefinitionV2 MakeFacility(
		ESRFacilityContentPresetV2 Preset,
		const TCHAR* ContentId,
		const FText& DisplayName,
		ESRFacilityProcessRoleV2 ProcessRole,
		const TCHAR* ProcessArchetype,
		ESRResourceFamily AcceptedFamily,
		ESRResourceFamilyAction FamilyAction,
		double EnergyDelta,
		float CycleSeconds,
		int32 OperationalLoad,
		ESRFacilityTemperatureState ReferenceTemperature = ESRFacilityTemperatureState::Normal,
		FName DefaultPayloadId = NAME_None,
		ESRFacilityLineRoleV2 LineRole = ESRFacilityLineRoleV2::None)
	{
		FSRFacilityContentDefinitionV2 Definition;
		Definition.Preset = Preset;
		Definition.ContentId = FName(ContentId);
		Definition.DisplayName = DisplayName;
		Definition.ProcessRole = ProcessRole;
		Definition.LineRole = LineRole;
		Definition.ProcessArchetype = ProcessArchetype && *ProcessArchetype != TEXT('\0')
			? FName(ProcessArchetype)
			: NAME_None;
		Definition.AcceptedFamily = AcceptedFamily;
		Definition.FamilyAction = FamilyAction;
		Definition.FacilityEnergyDelta = EnergyDelta;
		Definition.CycleSeconds = CycleSeconds;
		Definition.ReferenceTemperature = ReferenceTemperature;
		Definition.OperationalLoad = OperationalLoad;
		Definition.DefaultPayloadId = DefaultPayloadId;
		return Definition;
	}

	FSRFacilityContentDefinitionV2 MakeStellarFuelFabricator()
	{
		FSRFacilityContentDefinitionV2 Definition = MakeFacility(
			ESRFacilityContentPresetV2::StellarFuelFabricator,
			TEXT("StellarFuelFabricator"),
			NSLOCTEXT("StarRoversFacilityV2", "StellarFuelFabricator", "Stellar Fuel Fabricator"),
			ESRFacilityProcessRoleV2::FamilyProcess,
			TEXT(""),
			ESRResourceFamily::None,
			ESRResourceFamilyAction::None,
			0.0,
			10.0f,
			6);
		Definition.OperationKind = ESRFacilityOperationKind::Synthesize;
		Definition.SynthesisRole = ESRFacilitySynthesisRoleV2::StellarFuelFabricator;
		return Definition;
	}

	FSRFacilityContentDefinitionV2 MakeSupplyFabricator()
	{
		FSRFacilityContentDefinitionV2 Definition = MakeFacility(
			ESRFacilityContentPresetV2::SupplyFabricator,
			TEXT("SupplyFabricator"),
			NSLOCTEXT("StarRoversFacilityV2", "SupplyFabricator", "Supply Fabricator"),
			ESRFacilityProcessRoleV2::FamilyProcess,
			TEXT(""),
			ESRResourceFamily::None,
			ESRResourceFamilyAction::None,
			0.0,
			30.0f,
			4);
		Definition.OperationKind = ESRFacilityOperationKind::Synthesize;
		Definition.SynthesisRole = ESRFacilitySynthesisRoleV2::IndustrialSupplyFabricator;
		Definition.DefaultOperationalPriority = ESROperationalPriorityV2::Critical;
		return Definition;
	}

	FSRFacilityContentDefinitionV2 MakeServiceCore()
	{
		FSRFacilityContentDefinitionV2 Definition = MakeFacility(
			ESRFacilityContentPresetV2::ServiceCore,
			TEXT("ServiceCore"),
			NSLOCTEXT("StarRoversFacilityV2", "ServiceCore", "Service Core"),
			ESRFacilityProcessRoleV2::FamilyProcess,
			TEXT(""),
			ESRResourceFamily::None,
			ESRResourceFamilyAction::None,
			0.0,
			30.0f,
			0);
		Definition.OperationKind = ESRFacilityOperationKind::Synthesize;
		Definition.SynthesisRole = ESRFacilitySynthesisRoleV2::ServiceCore;
		Definition.DefaultOperationalPriority = ESROperationalPriorityV2::Critical;
		return Definition;
	}

	FSRFacilityContentDefinitionV2 MakeFleetBerth()
	{
		FSRFacilityContentDefinitionV2 Definition = MakeFacility(
			ESRFacilityContentPresetV2::FleetBerth,
			TEXT("FleetBerth"),
			NSLOCTEXT("StarRoversFacilityV2", "FleetBerth", "Fleet Berth"),
			ESRFacilityProcessRoleV2::FamilyProcess,
			TEXT(""),
			ESRResourceFamily::None,
			ESRResourceFamilyAction::None,
			0.0,
			60.0f,
			0);
		Definition.OperationKind = ESRFacilityOperationKind::Synthesize;
		Definition.SynthesisRole = ESRFacilitySynthesisRoleV2::FleetBerth;
		Definition.DefaultOperationalPriority = ESROperationalPriorityV2::Critical;
		return Definition;
	}

	const TArray<FSRFacilityContentDefinitionV2>& GetFacilities()
	{
		static const TArray<FSRFacilityContentDefinitionV2> Definitions =
		{
			MakeFacility(ESRFacilityContentPresetV2::PulseProcessor, TEXT("PulseProcessor"), NSLOCTEXT("StarRoversFacilityV2", "PulseProcessor", "Pulse Processor"), ESRFacilityProcessRoleV2::FamilyProcess, TEXT("Pulse"), ESRResourceFamily::None, ESRResourceFamilyAction::None, 1.0, 2.0f, 1, ESRFacilityTemperatureState::Normal, NAME_None, ESRFacilityLineRoleV2::UniversalBridge),
			MakeFacility(ESRFacilityContentPresetV2::CompressionMill, TEXT("CompressionMill"), NSLOCTEXT("StarRoversFacilityV2", "CompressionMill", "Compression Mill"), ESRFacilityProcessRoleV2::FamilyProcess, TEXT("Compression"), ESRResourceFamily::None, ESRResourceFamilyAction::None, 3.0, 5.0f, 3, ESRFacilityTemperatureState::Normal, NAME_None, ESRFacilityLineRoleV2::UniversalBridge),
			MakeFacility(ESRFacilityContentPresetV2::TagImprinter, TEXT("TagImprinter"), NSLOCTEXT("StarRoversFacilityV2", "TagImprinter", "Tag Imprinter"), ESRFacilityProcessRoleV2::ApplyProcessTag, TEXT(""), ESRResourceFamily::None, ESRResourceFamilyAction::None, 0.0, 2.0f, 1, ESRFacilityTemperatureState::Normal, FSRResourceSystemContent::GetProcessTagId(ESRProcessTagContentV2::Overtone)),
			MakeFacility(ESRFacilityContentPresetV2::FuelImprinter, TEXT("FuelImprinter"), NSLOCTEXT("StarRoversFacilityV2", "FuelImprinter", "Fuel Imprinter"), ESRFacilityProcessRoleV2::ApplyFuelImprint, TEXT(""), ESRResourceFamily::None, ESRResourceFamilyAction::None, 0.0, 2.0f, 1, ESRFacilityTemperatureState::Normal, FSRResourceSystemContent::GetFuelImprintId(ESRFuelImprintContentV2::TwinSeal)),
			MakeFacility(ESRFacilityContentPresetV2::TagScrubber, TEXT("TagScrubber"), NSLOCTEXT("StarRoversFacilityV2", "TagScrubber", "Tag Scrubber"), ESRFacilityProcessRoleV2::ClearProcessTag, TEXT(""), ESRResourceFamily::None, ESRResourceFamilyAction::None, 0.0, 2.0f, 1),
			MakeFacility(ESRFacilityContentPresetV2::InductionForge, TEXT("InductionForge"), NSLOCTEXT("StarRoversFacilityV2", "InductionForge", "Induction Forge"), ESRFacilityProcessRoleV2::FamilyProcess, TEXT("Forge"), ESRResourceFamily::Metal, ESRResourceFamilyAction::None, 4.0, 4.0f, 3, ESRFacilityTemperatureState::Hot, NAME_None, ESRFacilityLineRoleV2::Primer),
			MakeFacility(ESRFacilityContentPresetV2::CryoPress, TEXT("CryoPress"), NSLOCTEXT("StarRoversFacilityV2", "CryoPress", "Cryo Press"), ESRFacilityProcessRoleV2::FamilyProcess, TEXT("Press"), ESRResourceFamily::Metal, ESRResourceFamilyAction::None, 3.0, 4.0f, 3, ESRFacilityTemperatureState::Cold, NAME_None, ESRFacilityLineRoleV2::Payoff),
			MakeFacility(ESRFacilityContentPresetV2::AnnealingChamber, TEXT("AnnealingChamber"), NSLOCTEXT("StarRoversFacilityV2", "AnnealingChamber", "Annealing Chamber"), ESRFacilityProcessRoleV2::FamilyProcess, TEXT("Anneal"), ESRResourceFamily::Metal, ESRResourceFamilyAction::Anneal, 0.0, 6.0f, 2, ESRFacilityTemperatureState::Normal, NAME_None, ESRFacilityLineRoleV2::Recovery),
			MakeFacility(ESRFacilityContentPresetV2::ResonanceMill, TEXT("ResonanceMill"), NSLOCTEXT("StarRoversFacilityV2", "ResonanceMill", "Resonance Mill"), ESRFacilityProcessRoleV2::FamilyProcess, TEXT("Resonance"), ESRResourceFamily::Crystal, ESRResourceFamilyAction::None, 3.0, 3.0f, 2, ESRFacilityTemperatureState::Normal, NAME_None, ESRFacilityLineRoleV2::Repeater),
			MakeFacility(ESRFacilityContentPresetV2::FacetShifter, TEXT("FacetShifter"), NSLOCTEXT("StarRoversFacilityV2", "FacetShifter", "Facet Shifter"), ESRFacilityProcessRoleV2::FamilyProcess, TEXT("Facet"), ESRResourceFamily::Crystal, ESRResourceFamilyAction::None, 2.0, 3.0f, 2, ESRFacilityTemperatureState::Normal, NAME_None, ESRFacilityLineRoleV2::Recovery),
			MakeFacility(ESRFacilityContentPresetV2::GrowthVat, TEXT("GrowthVat"), NSLOCTEXT("StarRoversFacilityV2", "GrowthVat", "Growth Vat"), ESRFacilityProcessRoleV2::FamilyProcess, TEXT("Growth"), ESRResourceFamily::Organic, ESRResourceFamilyAction::Growth, 0.0, 5.0f, 1, ESRFacilityTemperatureState::Normal, NAME_None, ESRFacilityLineRoleV2::Primer),
			MakeFacility(ESRFacilityContentPresetV2::EnzymeLoom, TEXT("EnzymeLoom"), NSLOCTEXT("StarRoversFacilityV2", "EnzymeLoom", "Enzyme Loom"), ESRFacilityProcessRoleV2::FamilyProcess, TEXT("Loom"), ESRResourceFamily::Organic, ESRResourceFamilyAction::None, 2.0, 2.0f, 2, ESRFacilityTemperatureState::Normal, NAME_None, ESRFacilityLineRoleV2::Payoff),
			MakeFacility(ESRFacilityContentPresetV2::SporePress, TEXT("SporePress"), NSLOCTEXT("StarRoversFacilityV2", "SporePress", "Spore Press"), ESRFacilityProcessRoleV2::FamilyProcess, TEXT("Press"), ESRResourceFamily::Organic, ESRResourceFamilyAction::None, 5.0, 5.0f, 1, ESRFacilityTemperatureState::Normal, NAME_None, ESRFacilityLineRoleV2::Payoff),
			MakeFacility(ESRFacilityContentPresetV2::ArcAmplifier, TEXT("ArcAmplifier"), NSLOCTEXT("StarRoversFacilityV2", "ArcAmplifier", "Arc Amplifier"), ESRFacilityProcessRoleV2::FamilyProcess, TEXT("Amplification"), ESRResourceFamily::Plasma, ESRResourceFamilyAction::Amplification, 4.0, 2.0f, 5, ESRFacilityTemperatureState::Normal, NAME_None, ESRFacilityLineRoleV2::Burst),
			MakeFacility(ESRFacilityContentPresetV2::GroundingCoil, TEXT("GroundingCoil"), NSLOCTEXT("StarRoversFacilityV2", "GroundingCoil", "Grounding Coil"), ESRFacilityProcessRoleV2::FamilyProcess, TEXT("Discharge"), ESRResourceFamily::Plasma, ESRResourceFamilyAction::Discharge, 1.0, 3.0f, 1, ESRFacilityTemperatureState::Normal, NAME_None, ESRFacilityLineRoleV2::Stabilizer),
			MakeFacility(ESRFacilityContentPresetV2::NullSink, TEXT("NullSink"), NSLOCTEXT("StarRoversFacilityV2", "NullSink", "Null Sink"), ESRFacilityProcessRoleV2::FamilyProcess, TEXT("VoidSacrifice"), ESRResourceFamily::Void, ESRResourceFamilyAction::VoidSacrifice, -3.0, 2.0f, 1, ESRFacilityTemperatureState::Normal, NAME_None, ESRFacilityLineRoleV2::Sacrifice),
			MakeFacility(ESRFacilityContentPresetV2::EchoChamber, TEXT("EchoChamber"), NSLOCTEXT("StarRoversFacilityV2", "EchoChamber", "Echo Chamber"), ESRFacilityProcessRoleV2::FamilyProcess, TEXT("EnergyGain"), ESRResourceFamily::Void, ESRResourceFamilyAction::EnergyGain, 5.0, 5.0f, 3, ESRFacilityTemperatureState::Normal, NAME_None, ESRFacilityLineRoleV2::Payoff),
			MakeStellarFuelFabricator(),
			MakeSupplyFabricator(),
			MakeServiceCore(),
			MakeFleetBerth(),
		};
		return Definitions;
	}
}

FName FSRResourceSystemContent::GetUtilityResourceId(ESRResourceContentPresetV2 UtilityPreset)
{
	switch (UtilityPreset)
	{
	case ESRResourceContentPresetV2::CommonOre: return FName(TEXT("CommonOre"));
	case ESRResourceContentPresetV2::BiomassFeedstock: return FName(TEXT("BiomassFeedstock"));
	case ESRResourceContentPresetV2::IndustrialSupply: return FName(TEXT("IndustrialSupply"));
	default: return NAME_None;
	}
}

FName FSRResourceSystemContent::GetProcessTagId(ESRProcessTagContentV2 ProcessTag)
{
	switch (ProcessTag)
	{
	case ESRProcessTagContentV2::Overtone: return FName(TEXT("Overtone"));
	case ESRProcessTagContentV2::Reclamation: return FName(TEXT("Reclamation"));
	case ESRProcessTagContentV2::Crosslink: return FName(TEXT("Crosslink"));
	case ESRProcessTagContentV2::LandingCharge: return FName(TEXT("LandingCharge"));
	case ESRProcessTagContentV2::PilgrimCharge: return FName(TEXT("PilgrimCharge"));
	case ESRProcessTagContentV2::None:
	default: return NAME_None;
	}
}

FName FSRResourceSystemContent::GetFuelImprintId(ESRFuelImprintContentV2 FuelImprint)
{
	switch (FuelImprint)
	{
	case ESRFuelImprintContentV2::TwinSeal: return FName(TEXT("TwinSeal"));
	case ESRFuelImprintContentV2::ConvergenceSeal: return FName(TEXT("ConvergenceSeal"));
	case ESRFuelImprintContentV2::FoundrySeal: return FName(TEXT("FoundrySeal"));
	case ESRFuelImprintContentV2::PilgrimSeal: return FName(TEXT("PilgrimSeal"));
	case ESRFuelImprintContentV2::PrismaticCatalyst: return FName(TEXT("PrismaticCatalyst"));
	case ESRFuelImprintContentV2::None:
	default: return NAME_None;
	}
}

bool FSRResourceSystemContent::TryGetProcessTagDefinition(
	FName TagId,
	FSRProcessTagDefinitionV2& OutDefinition)
{
	for (const FSRProcessTagDefinitionV2& Definition : GetProcessTags())
	{
		if (Definition.TagId == TagId)
		{
			OutDefinition = Definition;
			return true;
		}
	}
	OutDefinition = FSRProcessTagDefinitionV2();
	return false;
}

bool FSRResourceSystemContent::TryGetFuelImprintDefinition(
	FName ImprintId,
	FSRFuelImprintDefinitionV2& OutDefinition)
{
	for (const FSRFuelImprintDefinitionV2& Definition : GetFuelImprints())
	{
		if (Definition.ImprintId == ImprintId)
		{
			OutDefinition = Definition;
			return true;
		}
	}
	OutDefinition = FSRFuelImprintDefinitionV2();
	return false;
}

bool FSRResourceSystemContent::TryGetReferenceResourceDefinition(
	ESRResourceContentPresetV2 Preset,
	FSRReferenceResourceDefinitionV2& OutDefinition)
{
	for (const FSRReferenceResourceDefinitionV2& Definition : GetResources())
	{
		if (Definition.Preset == Preset)
		{
			OutDefinition = Definition;
			return true;
		}
	}
	OutDefinition = FSRReferenceResourceDefinitionV2();
	return false;
}

bool FSRResourceSystemContent::TryGetReferenceResourceDefinition(
	FName ResourceId,
	FSRReferenceResourceDefinitionV2& OutDefinition)
{
	for (const FSRReferenceResourceDefinitionV2& Definition : GetResources())
	{
		if (Definition.ResourceId == ResourceId)
		{
			OutDefinition = Definition;
			return true;
		}
	}
	OutDefinition = FSRReferenceResourceDefinitionV2();
	return false;
}

bool FSRResourceSystemContent::TryGetUtilityResourceDefinition(
	ESRResourceContentPresetV2 Preset,
	FSRUtilityResourceDefinitionV2& OutDefinition)
{
	for (const FSRUtilityResourceDefinitionV2& Definition : GetUtilityResources())
	{
		if (Definition.Preset == Preset)
		{
			OutDefinition = Definition;
			return true;
		}
	}
	OutDefinition = FSRUtilityResourceDefinitionV2();
	return false;
}

bool FSRResourceSystemContent::TryGetDepositTotalAmount(
	ESRResourceContentPresetV2 Preset,
	int32& OutDepositTotalAmount)
{
	OutDepositTotalAmount = 0;
	FSRReferenceResourceDefinitionV2 CardDefinition;
	if (TryGetReferenceResourceDefinition(Preset, CardDefinition)
		&& CardDefinition.DepositTotalAmount > 0)
	{
		OutDepositTotalAmount = CardDefinition.DepositTotalAmount;
		return true;
	}

	FSRUtilityResourceDefinitionV2 UtilityDefinition;
	if (TryGetUtilityResourceDefinition(Preset, UtilityDefinition)
		&& UtilityDefinition.DepositTotalAmount > 0)
	{
		OutDepositTotalAmount = UtilityDefinition.DepositTotalAmount;
		return true;
	}
	return false;
}

bool FSRResourceSystemContent::TryGetFacilityDefinition(
	ESRFacilityContentPresetV2 Preset,
	FSRFacilityContentDefinitionV2& OutDefinition)
{
	for (const FSRFacilityContentDefinitionV2& Definition : GetFacilities())
	{
		if (Definition.Preset == Preset)
		{
			OutDefinition = Definition;
			return true;
		}
	}
	OutDefinition = FSRFacilityContentDefinitionV2();
	return false;
}

bool FSRResourceSystemContent::ValidateProcessTagDefinition(
	const FSRProcessTagDefinitionV2& Definition,
	FString& OutFailureReason)
{
	OutFailureReason.Reset();
	if (Definition.TagId.IsNone() || Definition.DisplayName.IsEmpty())
	{
		OutFailureReason = TEXT("Process Tag id and display name must be non-empty.");
		return false;
	}
	if (!IsSupportedProcessTagTrigger(Definition.Trigger))
	{
		OutFailureReason = FString::Printf(
			TEXT("Process Tag %s uses an unsupported trigger."),
			*Definition.TagId.ToString());
		return false;
	}
	if (Definition.TriggerCount != 1)
	{
		OutFailureReason = FString::Printf(
			TEXT("Process Tag %s must be one-shot; permanent and stacking Tags are forbidden."),
			*Definition.TagId.ToString());
		return false;
	}
	if (!FMath::IsFinite(Definition.EnergyDelta)
		|| Definition.EnergyDelta <= 0.0
		|| Definition.EnergyDelta > MaximumProcessTagEnergyDelta)
	{
		OutFailureReason = FString::Printf(
			TEXT("Process Tag %s must grant finite additive Energy in the range (0, %.0f]."),
			*Definition.TagId.ToString(),
			MaximumProcessTagEnergyDelta);
		return false;
	}
	return true;
}

bool FSRResourceSystemContent::ValidateProcessTagCatalog(FString& OutFailureReason)
{
	OutFailureReason.Reset();
	TSet<FName> TagIds;
	TSet<ESRProcessTagTriggerV2> Triggers;
	for (const FSRProcessTagDefinitionV2& Definition : GetProcessTags())
	{
		if (!ValidateProcessTagDefinition(Definition, OutFailureReason))
		{
			return false;
		}
		if (TagIds.Contains(Definition.TagId))
		{
			OutFailureReason = FString::Printf(
				TEXT("Process Tag id %s is duplicated."),
				*Definition.TagId.ToString());
			return false;
		}
		if (Triggers.Contains(Definition.Trigger))
		{
			OutFailureReason = FString::Printf(
				TEXT("Process Tag %s duplicates another Tag trigger and would create a strict numeric upgrade."),
				*Definition.TagId.ToString());
			return false;
		}
		TagIds.Add(Definition.TagId);
		Triggers.Add(Definition.Trigger);
	}
	return true;
}

void FSRResourceSystemContent::GetAllProcessTagDefinitions(TArray<FSRProcessTagDefinitionV2>& OutDefinitions)
{
	OutDefinitions = GetProcessTags();
}

void FSRResourceSystemContent::GetAllFuelImprintDefinitions(TArray<FSRFuelImprintDefinitionV2>& OutDefinitions)
{
	OutDefinitions = GetFuelImprints();
}

void FSRResourceSystemContent::GetAllReferenceResourceDefinitions(TArray<FSRReferenceResourceDefinitionV2>& OutDefinitions)
{
	OutDefinitions = GetResources();
}

void FSRResourceSystemContent::GetAllUtilityResourceDefinitions(TArray<FSRUtilityResourceDefinitionV2>& OutDefinitions)
{
	OutDefinitions = GetUtilityResources();
}

void FSRResourceSystemContent::GetAllFacilityDefinitions(TArray<FSRFacilityContentDefinitionV2>& OutDefinitions)
{
	OutDefinitions = GetFacilities();
}

bool FSRResourceSystemContent::ApplyResourcePreset(
	USRResourceDataAsset& ResourceDataAsset,
	ESRResourceContentPresetV2 Preset)
{
	FSRReferenceResourceDefinitionV2 CardDefinition;
	FSRUtilityResourceDefinitionV2 UtilityDefinition;
	const bool bIsCard = TryGetReferenceResourceDefinition(Preset, CardDefinition);
	const bool bIsUtility = !bIsCard && TryGetUtilityResourceDefinition(Preset, UtilityDefinition);
	if (!bIsCard && !bIsUtility)
	{
		return false;
	}

	ResourceDataAsset.ResourceV2Preset = Preset;
	ResourceDataAsset.ResourceDefinitionVersion = StarRovers::Resources::CurrentResourceDefinitionVersion;
	ResourceDataAsset.ResourceId = bIsCard ? CardDefinition.ResourceId : UtilityDefinition.ResourceId;
	ResourceDataAsset.DisplayName = bIsCard ? CardDefinition.DisplayName : UtilityDefinition.DisplayName;
	ResourceDataAsset.Description = bIsCard ? CardDefinition.Description : UtilityDefinition.Description;
	ResourceDataAsset.ResourceClass = bIsCard ? ESRResourceClass::Card : ESRResourceClass::Utility;
	ResourceDataAsset.Family = bIsCard ? CardDefinition.Family : ESRResourceFamily::None;
	ResourceDataAsset.SeedEnergy = bIsCard ? CardDefinition.SeedEnergy : 0.0;
	ResourceDataAsset.NativeSpectrum = bIsCard ? CardDefinition.Spectrum : ESRResourceSpectrum::None;
	ResourceDataAsset.NativeGrade = bIsCard ? CardDefinition.Grade : StarRovers::Resources::MinimumGrade;
	ResourceDataAsset.BaseEnergyValue = ResourceDataAsset.SeedEnergy;
	ResourceDataAsset.BaseProcessLimit = 0;
	ResourceDataAsset.DefaultTags.Reset();
	return true;
}

bool FSRResourceSystemContent::ApplyFacilityPreset(
	USRFacilityDataAsset& FacilityDataAsset,
	ESRFacilityContentPresetV2 Preset)
{
	FSRFacilityContentDefinitionV2 Definition;
	if (!TryGetFacilityDefinition(Preset, Definition))
	{
		return false;
	}

	FacilityDataAsset.ResourceV2Preset = Preset;
	FacilityDataAsset.ResourceV2ContentId = Definition.ContentId;
	FacilityDataAsset.FacilityDefinitionVersion = StarRovers::Facilities::CurrentFacilityDefinitionVersion;
	FacilityDataAsset.FacilityKind = ESRFacilityKind::Standard;
	FacilityDataAsset.Rarity = ESRFacilityRarity::Basic;
	FacilityDataAsset.OperationKind = Definition.OperationKind;
	FacilityDataAsset.BaseProcessSeconds = FMath::Max(0.01f, Definition.CycleSeconds);
	FacilityDataAsset.OperationalLoad = FMath::Max(0, Definition.OperationalLoad);
	FacilityDataAsset.DefaultOperationalPriority = Definition.DefaultOperationalPriority;
	switch (Definition.SynthesisRole)
	{
	case ESRFacilitySynthesisRoleV2::StellarFuelFabricator:
		FacilityDataAsset.InputInventory.SlotCount = StarRovers::StellarFuel::RequiredCardCount;
		FacilityDataAsset.InputInventory.SlotCapacity = 8;
		FacilityDataAsset.OutputInventory.SlotCount = 1;
		break;
	case ESRFacilitySynthesisRoleV2::IndustrialSupplyFabricator:
		FacilityDataAsset.InputInventory.SlotCount = 2;
		FacilityDataAsset.InputInventory.SlotCapacity = 8;
		FacilityDataAsset.OutputInventory.SlotCount = 1;
		break;
	case ESRFacilitySynthesisRoleV2::ServiceCore:
	case ESRFacilitySynthesisRoleV2::FleetBerth:
		FacilityDataAsset.InputInventory.SlotCount = 1;
		FacilityDataAsset.InputInventory.SlotCapacity = 4;
		FacilityDataAsset.OutputInventory.SlotCount = 0;
		break;
	default:
		FacilityDataAsset.InputInventory.SlotCount = 1;
		FacilityDataAsset.InputInventory.SlotCapacity = 8;
		FacilityDataAsset.OutputInventory.SlotCount = 1;
		break;
	}
	FacilityDataAsset.OutputInventory.SlotCapacity = 8;
	// Legacy compatibility fields represent per-slot capacity, not aggregate capacity.
	FacilityDataAsset.InputCapacity = FacilityDataAsset.InputInventory.SlotCapacity;
	FacilityDataAsset.OutputCapacity = FacilityDataAsset.OutputInventory.SlotCapacity;
	FacilityDataAsset.Effects.Reset();

	FacilityDataAsset.ResourceV2Process = FSRFacilityProcessDefinitionV2();
	FacilityDataAsset.ResourceV2Synthesis = FSRFacilitySynthesisDefinitionV2();
	FacilityDataAsset.ResourceV2Synthesis.SynthesisRole = Definition.SynthesisRole;
	FacilityDataAsset.ResourceV2Process.ProcessRole = Definition.ProcessRole;
	FacilityDataAsset.ResourceV2Process.LineRole = Definition.LineRole;
	FacilityDataAsset.ResourceV2Process.ProcessArchetype = Definition.ProcessArchetype;
	FacilityDataAsset.ResourceV2Process.AcceptedFamily = Definition.AcceptedFamily;
	FacilityDataAsset.ResourceV2Process.FamilyAction = Definition.FamilyAction;
	FacilityDataAsset.ResourceV2Process.FacilityEnergyDelta = Definition.FacilityEnergyDelta;
	if (Definition.ProcessRole == ESRFacilityProcessRoleV2::ApplyProcessTag)
	{
		FacilityDataAsset.ResourceV2Process.ProcessTagId = Definition.DefaultPayloadId;
	}
	else if (Definition.ProcessRole == ESRFacilityProcessRoleV2::ApplyFuelImprint)
	{
		FacilityDataAsset.ResourceV2Process.FuelImprintId = Definition.DefaultPayloadId;
	}
	return true;
}

bool FSRResourceSystemContent::MakeReferenceResourceInstance(
	ESRResourceContentPresetV2 Preset,
	FName OriginBodyId,
	FSRResourceInstance& OutResourceInstance)
{
	FSRReferenceResourceDefinitionV2 CardDefinition;
	FSRUtilityResourceDefinitionV2 UtilityDefinition;
	const bool bIsCard = TryGetReferenceResourceDefinition(Preset, CardDefinition);
	const bool bIsUtility = !bIsCard && TryGetUtilityResourceDefinition(Preset, UtilityDefinition);
	if (!bIsCard && !bIsUtility)
	{
		OutResourceInstance = FSRResourceInstance();
		return false;
	}

	OutResourceInstance = FSRResourceInstance();
	OutResourceInstance.ResourceSchemaVersion = StarRovers::Resources::CurrentResourceSchemaVersion;
	OutResourceInstance.ResourceId = bIsCard ? CardDefinition.ResourceId : UtilityDefinition.ResourceId;
	OutResourceInstance.ResourceClass = bIsCard ? ESRResourceClass::Card : ESRResourceClass::Utility;
	OutResourceInstance.Family = bIsCard ? CardDefinition.Family : ESRResourceFamily::None;
	OutResourceInstance.CurrentEnergy = bIsCard ? CardDefinition.SeedEnergy : 0.0;
	OutResourceInstance.SeedEnergySnapshot = bIsCard ? FMath::Max(0.0, CardDefinition.SeedEnergy) : 0.0;
	OutResourceInstance.bHasSeedEnergySnapshot = bIsCard;
	OutResourceInstance.Spectrum = bIsCard ? CardDefinition.Spectrum : ESRResourceSpectrum::None;
	OutResourceInstance.Grade = bIsCard ? CardDefinition.Grade : StarRovers::Resources::MinimumGrade;
	OutResourceInstance.StackCount = 1;
	if (bIsCard)
	{
		StarRovers::Resources::InitializeResourceOrigin(OutResourceInstance, OriginBodyId);
	}
	StarRovers::Resources::SynchronizeResourceV2RuntimeStateToLegacy(OutResourceInstance);
	return true;
}

bool FSRResourceSystemContent::MakeReferenceStellarFuelBatch(
	ESRStellarFuelReferenceTopologyV2 Topology,
	FName FabricatorBodyId,
	TArray<FSRResourceInstance>& OutCards)
{
	OutCards.Reset();
	if (FabricatorBodyId.IsNone())
	{
		return false;
	}

	const ESRResourceContentPresetV2 Presets[StarRovers::StellarFuel::RequiredCardCount] =
	{
		ESRResourceContentPresetV2::HeliosIron,
		ESRResourceContentPresetV2::EchoQuartz,
		ESRResourceContentPresetV2::VerdantSpore,
		ESRResourceContentPresetV2::AuroraPlasma,
		ESRResourceContentPresetV2::NullPearl,
	};
	const FName OriginBodyIds[StarRovers::StellarFuel::RequiredCardCount] =
	{
		FName(TEXT("Cinder")),
		FName(TEXT("Prism")),
		FName(TEXT("Viridia")),
		FName(TEXT("Tempest")),
		FName(TEXT("Nadir")),
	};
	const double DistributedEnergy[StarRovers::StellarFuel::RequiredCardCount] =
	{
		34.0, 40.0, 36.0, 39.0, 27.0,
	};

	OutCards.Reserve(StarRovers::StellarFuel::RequiredCardCount);
	for (int32 CardIndex = 0; CardIndex < StarRovers::StellarFuel::RequiredCardCount; ++CardIndex)
	{
		FSRResourceInstance Card;
		if (!MakeReferenceResourceInstance(Presets[CardIndex], OriginBodyIds[CardIndex], Card))
		{
			OutCards.Reset();
			return false;
		}

		Card.ResourceInstanceId = FName(*FString::Printf(TEXT("ReferenceFuelCard_%d"), CardIndex));
		Card.CurrentEnergy = DistributedEnergy[CardIndex]
			+ (Topology == ESRStellarFuelReferenceTopologyV2::PilgrimCircuit && CardIndex == 0 ? 5.0 : 0.0);
		Card.LogisticsMetadata.LastTransitDestinationBodyId = FabricatorBodyId;
		Card.LogisticsMetadata.TransitCount = 1;
		switch (Topology)
		{
		case ESRStellarFuelReferenceTopologyV2::DistributedConvergence:
			Card.LogisticsMetadata.LastProcessedBodyId = OriginBodyIds[CardIndex];
			Card.LogisticsMetadata.LastTransitSourceBodyId = OriginBodyIds[CardIndex];
			break;
		case ESRStellarFuelReferenceTopologyV2::CentralFoundry:
			Card.LogisticsMetadata.LastProcessedBodyId = FabricatorBodyId;
			Card.LogisticsMetadata.LastTransitSourceBodyId = FName(*FString::Printf(
				TEXT("FoundryIngress_%d"),
				CardIndex));
			break;
		case ESRStellarFuelReferenceTopologyV2::PilgrimCircuit:
			Card.LogisticsMetadata.LastProcessedBodyId = FName(*FString::Printf(
				TEXT("PilgrimRelay_%d"),
				CardIndex));
			Card.LogisticsMetadata.LastTransitSourceBodyId = Card.LogisticsMetadata.LastProcessedBodyId;
			Card.LogisticsMetadata.TransitCount = 2;
			Card.LogisticsMetadata.bHasBeenProcessedOutsideOrigin = true;
			break;
		default:
			OutCards.Reset();
			return false;
		}

		if (CardIndex < 3)
		{
			Card.FuelImprintSlot.ImprintId = GetFuelImprintId(ESRFuelImprintContentV2::TwinSeal);
		}
		else if (CardIndex == 3)
		{
			switch (Topology)
			{
			case ESRStellarFuelReferenceTopologyV2::DistributedConvergence:
				Card.FuelImprintSlot.ImprintId = GetFuelImprintId(ESRFuelImprintContentV2::ConvergenceSeal);
				break;
			case ESRStellarFuelReferenceTopologyV2::CentralFoundry:
				Card.FuelImprintSlot.ImprintId = GetFuelImprintId(ESRFuelImprintContentV2::FoundrySeal);
				break;
			case ESRStellarFuelReferenceTopologyV2::PilgrimCircuit:
				Card.FuelImprintSlot.ImprintId = GetFuelImprintId(ESRFuelImprintContentV2::PilgrimSeal);
				break;
			default:
				break;
			}
		}
		else
		{
			Card.FuelImprintSlot.ImprintId = GetFuelImprintId(ESRFuelImprintContentV2::PrismaticCatalyst);
		}

		StarRovers::Resources::SynchronizeResourceV2RuntimeStateToLegacy(Card);
		OutCards.Add(MoveTemp(Card));
	}
	return OutCards.Num() == StarRovers::StellarFuel::RequiredCardCount;
}
