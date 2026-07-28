#include "UI/SRStructureBuildPresentation.h"

namespace
{
	FText ResolveDisplayName(const FSRStructureBuildOption& BuildOption)
	{
		return BuildOption.DisplayName.IsEmpty()
			? FText::FromName(BuildOption.StructureId)
			: BuildOption.DisplayName;
	}

	FText BuildCompactMetadata(const FSRStructureBuildOption& BuildOption)
	{
		return FText::Format(
			NSLOCTEXT("StarRoversBuildPresentation", "CompactMetadata", "{0}  ·  {1}x{2}  ·  LOAD {3}"),
			FSRStructureBuildPresentationBuilder::GetFamilyLabel(BuildOption.ResourceFamily),
			FText::AsNumber(BuildOption.FootprintCellsX),
			FText::AsNumber(BuildOption.FootprintCellsY),
			FText::AsNumber(BuildOption.OperationalLoad));
	}

	FText BuildSpecification(const FSRStructureBuildOption& BuildOption)
	{
		TArray<FText> Parts;
		Parts.Add(FText::Format(
			NSLOCTEXT("StarRoversBuildPresentation", "FootprintSpec", "Footprint {0}x{1}"),
			FText::AsNumber(BuildOption.FootprintCellsX),
			FText::AsNumber(BuildOption.FootprintCellsY)));

		if (BuildOption.InputPortCount > 0 || BuildOption.OutputPortCount > 0)
		{
			Parts.Add(FText::Format(
				NSLOCTEXT("StarRoversBuildPresentation", "PortSpec", "Ports {0} in / {1} out"),
				FText::AsNumber(BuildOption.InputPortCount),
				FText::AsNumber(BuildOption.OutputPortCount)));
		}
		if (BuildOption.BaseProcessSeconds > 0.0f)
		{
			Parts.Add(FText::Format(
				NSLOCTEXT("StarRoversBuildPresentation", "CycleSpec", "Base cycle {0}s"),
				FText::AsNumber(BuildOption.BaseProcessSeconds)));
		}
		Parts.Add(FText::Format(
			NSLOCTEXT("StarRoversBuildPresentation", "LoadSpec", "Load +{0} ({1})"),
			FText::AsNumber(BuildOption.OperationalLoad),
			FSRStructureBuildPresentationBuilder::GetPriorityLabel(BuildOption.OperationalPriority)));
		return FText::Join(NSLOCTEXT("StarRoversBuildPresentation", "SpecSeparator", "  ·  "), Parts);
	}

	FText UpperNameOr(FName Name, const FText& Fallback)
	{
		return Name.IsNone()
			? Fallback
			: FText::FromString(Name.ToString().ToUpper());
	}

	FText BuildCardInputLabel(ESRResourceFamily Family)
	{
		return Family == ESRResourceFamily::None
			? NSLOCTEXT("StarRoversBuildPresentation", "FlowAnyCard", "CARD")
			: FText::Format(
				NSLOCTEXT("StarRoversBuildPresentation", "FlowFamilyCard", "{0} CARD"),
				FSRStructureBuildPresentationBuilder::GetFamilyLabel(Family));
	}

	FText FormatSignedEnergy(double EnergyDelta)
	{
		FNumberFormattingOptions NumberFormat;
		NumberFormat.MinimumFractionalDigits = 0;
		NumberFormat.MaximumFractionalDigits = 1;
		return FText::Format(
			EnergyDelta >= 0.0
				? NSLOCTEXT("StarRoversBuildPresentation", "PositiveEnergyDelta", "ENERGY +{0}")
				: NSLOCTEXT("StarRoversBuildPresentation", "NegativeEnergyDelta", "ENERGY {0}"),
			FText::AsNumber(EnergyDelta, &NumberFormat));
	}

	FText BuildFamilyActionEffect(ESRResourceFamilyAction FamilyAction)
	{
		switch (FamilyAction)
		{
		case ESRResourceFamilyAction::Growth:
			return NSLOCTEXT("StarRoversBuildPresentation", "GrowthEffect", "SET MATURED · CLEAR DEPLETED");
		case ESRResourceFamilyAction::Amplification:
			return NSLOCTEXT("StarRoversBuildPresentation", "AmplificationEffect", "BUILD ENERGIZED / OVERLOAD");
		case ESRResourceFamilyAction::Discharge:
			return NSLOCTEXT("StarRoversBuildPresentation", "DischargeEffect", "CLEAR ENERGIZED / OVERLOADED");
		case ESRResourceFamilyAction::VoidSacrifice:
			return NSLOCTEXT("StarRoversBuildPresentation", "VoidSacrificeEffect", "STORE SACRIFICED ENERGY AS ECHO");
		case ESRResourceFamilyAction::EnergyGain:
			return NSLOCTEXT("StarRoversBuildPresentation", "EnergyGainEffect", "CONSUME ECHO FOR ADDITIVE ENERGY");
		case ESRResourceFamilyAction::Anneal:
			return NSLOCTEXT("StarRoversBuildPresentation", "AnnealEffect", "RESET METAL STATES AND FATIGUE HISTORY");
		case ESRResourceFamilyAction::None:
		default:
			return FText::GetEmpty();
		}
	}

	FText BuildLineRoleEffect(ESRFacilityLineRoleV2 LineRole)
	{
		switch (LineRole)
		{
		case ESRFacilityLineRoleV2::UniversalBridge:
			return NSLOCTEXT("StarRoversBuildPresentation", "BridgeRoleEffect", "BRIDGE · NO FAMILY MERIT");
		case ESRFacilityLineRoleV2::Primer:
			return NSLOCTEXT("StarRoversBuildPresentation", "PrimerRoleEffect", "PRIMER");
		case ESRFacilityLineRoleV2::Payoff:
			return NSLOCTEXT("StarRoversBuildPresentation", "PayoffRoleEffect", "PAYOFF");
		case ESRFacilityLineRoleV2::Repeater:
			return NSLOCTEXT("StarRoversBuildPresentation", "RepeaterRoleEffect", "REPEATER");
		case ESRFacilityLineRoleV2::Recovery:
			return NSLOCTEXT("StarRoversBuildPresentation", "RecoveryRoleEffect", "RECOVERY");
		case ESRFacilityLineRoleV2::Burst:
			return NSLOCTEXT("StarRoversBuildPresentation", "BurstRoleEffect", "BURST");
		case ESRFacilityLineRoleV2::Stabilizer:
			return NSLOCTEXT("StarRoversBuildPresentation", "StabilizerRoleEffect", "STABILIZER");
		case ESRFacilityLineRoleV2::Sacrifice:
			return NSLOCTEXT("StarRoversBuildPresentation", "SacrificeRoleEffect", "SACRIFICE");
		case ESRFacilityLineRoleV2::None:
		default:
			return FText::GetEmpty();
		}
	}

	ESRUIVisualState ResolveTargetVisualState(ESRStructurePlacementPreviewStatus Status)
	{
		switch (Status)
		{
		case ESRStructurePlacementPreviewStatus::Ready: return ESRUIVisualState::Positive;
		case ESRStructurePlacementPreviewStatus::Replacement: return ESRUIVisualState::Warning;
		case ESRStructurePlacementPreviewStatus::BlockedTerrain:
		case ESRStructurePlacementPreviewStatus::BlockedOccupancy:
		case ESRStructurePlacementPreviewStatus::OutsideSurface: return ESRUIVisualState::Danger;
		case ESRStructurePlacementPreviewStatus::InvalidDefinition: return ESRUIVisualState::Disabled;
		case ESRStructurePlacementPreviewStatus::ConveyorPath:
		case ESRStructurePlacementPreviewStatus::AwaitingSurface: return ESRUIVisualState::Info;
		case ESRStructurePlacementPreviewStatus::Inactive:
		default: return ESRUIVisualState::Neutral;
		}
	}

	FText BuildTargetLabel(ESRStructurePlacementPreviewStatus Status)
	{
		switch (Status)
		{
		case ESRStructurePlacementPreviewStatus::AwaitingSurface:
			return NSLOCTEXT("StarRoversBuildPresentation", "PlacementTargetAim", "TARGET · AIM");
		case ESRStructurePlacementPreviewStatus::ConveyorPath:
			return NSLOCTEXT("StarRoversBuildPresentation", "PlacementTargetDraw", "TARGET · DRAW");
		case ESRStructurePlacementPreviewStatus::Ready:
			return NSLOCTEXT("StarRoversBuildPresentation", "PlacementTargetClear", "TARGET · CLEAR");
		case ESRStructurePlacementPreviewStatus::Replacement:
			return NSLOCTEXT("StarRoversBuildPresentation", "PlacementTargetReplace", "TARGET · REPLACE");
		case ESRStructurePlacementPreviewStatus::InvalidDefinition:
			return NSLOCTEXT("StarRoversBuildPresentation", "PlacementTargetInvalid", "TARGET · INVALID");
		case ESRStructurePlacementPreviewStatus::OutsideSurface:
			return NSLOCTEXT("StarRoversBuildPresentation", "PlacementTargetOutside", "TARGET · OUTSIDE");
		case ESRStructurePlacementPreviewStatus::BlockedTerrain:
			return NSLOCTEXT("StarRoversBuildPresentation", "PlacementTargetTerrain", "TARGET · TERRAIN");
		case ESRStructurePlacementPreviewStatus::BlockedOccupancy:
			return NSLOCTEXT("StarRoversBuildPresentation", "PlacementTargetOccupied", "TARGET · OCCUPIED");
		case ESRStructurePlacementPreviewStatus::Inactive:
		default:
			return NSLOCTEXT("StarRoversBuildPresentation", "PlacementTargetSelect", "TARGET · SELECT");
		}
	}
}

FSRStructureBuildCardPresentation FSRStructureBuildPresentationBuilder::BuildCard(
	const FSRStructureBuildOption& BuildOption)
{
	FSRStructureBuildCardPresentation Presentation;
	Presentation.StructureId = BuildOption.StructureId;
	Presentation.DisplayName = ResolveDisplayName(BuildOption);
	Presentation.RoleText = GetRoleLabel(BuildOption.Role);
	Presentation.MetadataText = BuildCompactMetadata(BuildOption);
	Presentation.AvailabilityText = GetAvailabilityLabel(BuildOption.Availability);
	Presentation.ResourceFamily = BuildOption.ResourceFamily;
	Presentation.Availability = BuildOption.Availability;
	Presentation.bSelectable = BuildOption.IsSelectable();
	return Presentation;
}

FSRStructureBuildDetailPresentation FSRStructureBuildPresentationBuilder::BuildDetail(
	const FSRStructureBuildOption& BuildOption)
{
	FSRStructureBuildDetailPresentation Presentation;
	Presentation.StructureId = BuildOption.StructureId;
	Presentation.Title = ResolveDisplayName(BuildOption);
	Presentation.ClassificationText = FText::Format(
		NSLOCTEXT("StarRoversBuildPresentation", "Classification", "{0}  ·  {1}  ·  {2}"),
		GetRoleLabel(BuildOption.Role),
		GetFamilyLabel(BuildOption.ResourceFamily),
		GetRarityLabel(BuildOption.Rarity));
	Presentation.SpecificationText = BuildSpecification(BuildOption);
	Presentation.Description = BuildOption.Description;
	Presentation.AvailabilityText = GetAvailabilityLabel(BuildOption.Availability);
	Presentation.AvailabilityDetailText = FSRStructureBuildCatalogBuilder::BuildStatusText(BuildOption);
	Presentation.ResourceFamily = BuildOption.ResourceFamily;
	Presentation.Availability = BuildOption.Availability;
	Presentation.bSelectable = BuildOption.IsSelectable();
	return Presentation;
}

FSRStructureBuildEmptyStatePresentation FSRStructureBuildPresentationBuilder::BuildEmptyState(
	int32 TotalOptionCount,
	int32 VisibleOptionCount,
	int32 SelectableVisibleOptionCount)
{
	FSRStructureBuildEmptyStatePresentation Presentation;
	if (TotalOptionCount <= 0)
	{
		Presentation.bVisible = true;
		Presentation.Title = NSLOCTEXT("StarRoversBuildPresentation", "EmptyCatalogTitle", "Construction catalog unavailable");
		Presentation.ClassificationText = NSLOCTEXT("StarRoversBuildPresentation", "EmptyCatalogClass", "EMPTY STATE  ·  NO REGISTERED CONTENT");
		Presentation.DetailText = NSLOCTEXT(
			"StarRoversBuildPresentation",
			"EmptyCatalogDetail",
			"This Run currently exposes no registered Facility or structure definitions.");
		Presentation.ActionText = NSLOCTEXT(
			"StarRoversBuildPresentation",
			"EmptyCatalogAction",
			"NEXT  ·  Wait for world initialization to finish, then reopen the Build Dock.");
		Presentation.BadgeText = NSLOCTEXT("StarRoversBuildPresentation", "EmptyCatalogBadge", "NO CONTENT");
		Presentation.VisualState = ESRUIVisualState::Warning;
		return Presentation;
	}

	if (VisibleOptionCount <= 0)
	{
		Presentation.bVisible = true;
		Presentation.Title = NSLOCTEXT("StarRoversBuildPresentation", "NoMatchesTitle", "No Facilities match this workspace");
		Presentation.ClassificationText = NSLOCTEXT("StarRoversBuildPresentation", "NoMatchesClass", "EMPTY STATE  ·  FILTERED");
		Presentation.DetailText = NSLOCTEXT(
			"StarRoversBuildPresentation",
			"NoMatchesDetail",
			"The catalog is loaded, but the selected Family view has no matching construction options.");
		Presentation.ActionText = NSLOCTEXT(
			"StarRoversBuildPresentation",
			"NoMatchesAction",
			"NEXT  ·  Select ALL or another Family tab with an available count.");
		Presentation.BadgeText = NSLOCTEXT("StarRoversBuildPresentation", "NoMatchesBadge", "NO MATCHES");
		Presentation.VisualState = ESRUIVisualState::Info;
		return Presentation;
	}

	if (SelectableVisibleOptionCount <= 0)
	{
		Presentation.bVisible = true;
		Presentation.Title = NSLOCTEXT("StarRoversBuildPresentation", "AllLockedTitle", "Visible Facilities are locked");
		Presentation.ClassificationText = NSLOCTEXT("StarRoversBuildPresentation", "AllLockedClass", "BLOCKED STATE  ·  AUGMENT GATED");
		Presentation.DetailText = FText::Format(
			NSLOCTEXT("StarRoversBuildPresentation", "AllLockedDetail", "All {0} matching option(s) require an unavailable Augment or construction permission."),
			FText::AsNumber(VisibleOptionCount));
		Presentation.ActionText = NSLOCTEXT(
			"StarRoversBuildPresentation",
			"AllLockedAction",
			"NEXT  ·  Hover an AUGMENT LOCK card to inspect the required Package, or choose another Family.");
		Presentation.BadgeText = NSLOCTEXT("StarRoversBuildPresentation", "AllLockedBadge", "ALL LOCKED");
		Presentation.VisualState = ESRUIVisualState::Locked;
	}
	return Presentation;
}

FSRStructureBuildRecommendationPresentation FSRStructureBuildPresentationBuilder::BuildRecommendation(
	const FSRStructureBuildOption& BuildOption,
	const FSRStructureBuildRecommendationContext& Context)
{
	FSRStructureBuildRecommendationPresentation Presentation;
	Presentation.bVisible = Context.bActive
		&& !Context.RecommendedStructureId.IsNone()
		&& BuildOption.StructureId == Context.RecommendedStructureId;
	if (!Presentation.bVisible)
	{
		return Presentation;
	}

	Presentation.BadgeText = Context.CurrentStep > 0 && Context.TotalSteps > 0
		? FText::Format(
			NSLOCTEXT("StarRoversBuildPresentation", "RecommendationStepBadge", "NEXT {0}/{1}"),
			FText::AsNumber(Context.CurrentStep),
			FText::AsNumber(Context.TotalSteps))
		: NSLOCTEXT("StarRoversBuildPresentation", "RecommendationBadge", "NEXT");
	Presentation.ReasonText = Context.ObjectiveText.IsEmpty()
		? NSLOCTEXT("StarRoversBuildPresentation", "RecommendationReason", "CURRENT RUN OBJECTIVE")
		: Context.ObjectiveText;
	Presentation.VisualState = ESRUIVisualState::Warning;
	return Presentation;
}

FSRStructureBuildFlowPresentation FSRStructureBuildPresentationBuilder::BuildFlow(
	const FSRStructureBuildOption& BuildOption)
{
	FSRStructureBuildFlowPresentation Presentation;
	Presentation.bVisible = !BuildOption.StructureId.IsNone();
	if (!Presentation.bVisible)
	{
		return Presentation;
	}

	if (BuildOption.Role == ESRStructureBuildRole::Logistics
		|| BuildOption.BuildKind == ESRStructureBuildKind::Conveyor)
	{
		Presentation.InputText = NSLOCTEXT("StarRoversBuildPresentation", "FlowResourceInput", "RESOURCE");
		Presentation.ProcessText = NSLOCTEXT("StarRoversBuildPresentation", "FlowTransfer", "TRANSFER");
		Presentation.OutputText = NSLOCTEXT("StarRoversBuildPresentation", "FlowResourceOutput", "RESOURCE");
		Presentation.EffectText = NSLOCTEXT("StarRoversBuildPresentation", "FlowTransferEffect", "POSITION CHANGES · RESOURCE IDENTITY PRESERVED");
	}
	else if (BuildOption.Role == ESRStructureBuildRole::Extraction)
	{
		Presentation.InputText = NSLOCTEXT("StarRoversBuildPresentation", "FlowDeposit", "DEPOSIT");
		Presentation.ProcessText = NSLOCTEXT("StarRoversBuildPresentation", "FlowExtract", "EXTRACT");
		Presentation.OutputText = NSLOCTEXT("StarRoversBuildPresentation", "FlowExtractedResource", "RESOURCE");
		Presentation.EffectText = NSLOCTEXT("StarRoversBuildPresentation", "FlowExtractionEffect", "CREATES THE DEPOSIT'S AUTHORED RESOURCE");
	}
	else if (BuildOption.Role == ESRStructureBuildRole::Hub)
	{
		Presentation.InputText = NSLOCTEXT("StarRoversBuildPresentation", "FlowHubCargo", "LOCAL CARGO");
		Presentation.ProcessText = NSLOCTEXT("StarRoversBuildPresentation", "FlowHubRoute", "ROUTE / LAUNCH");
		Presentation.OutputText = NSLOCTEXT("StarRoversBuildPresentation", "FlowHubOutbound", "OFF-WORLD CARGO");
		Presentation.EffectText = NSLOCTEXT("StarRoversBuildPresentation", "FlowHubEffect", "USES ROUTE, FLEET, AND LAUNCH RULES");
	}
	else if (BuildOption.OperationKind == ESRFacilityOperationKind::Synthesize)
	{
		switch (BuildOption.SynthesisRole)
		{
		case ESRFacilitySynthesisRoleV2::StellarFuelFabricator:
			Presentation.InputText = NSLOCTEXT("StarRoversBuildPresentation", "FlowFiveCards", "5 CARDS");
			Presentation.ProcessText = NSLOCTEXT("StarRoversBuildPresentation", "FlowFinalHand", "HAND · FINAL B x C");
			Presentation.OutputText = NSLOCTEXT("StarRoversBuildPresentation", "FlowStellarFuel", "STELLAR FUEL");
			Presentation.EffectText = NSLOCTEXT("StarRoversBuildPresentation", "FlowStellarFuelEffect", "THE ONLY FACILITY THAT APPLIES THE FINAL MULTIPLIER");
			break;
		case ESRFacilitySynthesisRoleV2::IndustrialSupplyFabricator:
			Presentation.InputText = NSLOCTEXT("StarRoversBuildPresentation", "FlowTwoFeedstock", "2 FEEDSTOCK");
			Presentation.ProcessText = NSLOCTEXT("StarRoversBuildPresentation", "FlowSynthesizeSupply", "SYNTHESIZE");
			Presentation.OutputText = NSLOCTEXT("StarRoversBuildPresentation", "FlowIndustrialSupply", "INDUSTRIAL SUPPLY");
			Presentation.EffectText = NSLOCTEXT("StarRoversBuildPresentation", "FlowSupplyEffect", "FEEDS SERVICE CORE AND FLEET BERTH");
			break;
		case ESRFacilitySynthesisRoleV2::ServiceCore:
			Presentation.InputText = NSLOCTEXT("StarRoversBuildPresentation", "FlowSupplyInput", "INDUSTRIAL SUPPLY");
			Presentation.ProcessText = NSLOCTEXT("StarRoversBuildPresentation", "FlowServiceCore", "SERVICE CORE");
			Presentation.OutputText = NSLOCTEXT("StarRoversBuildPresentation", "FlowCapacityOutput", "+ CAPACITY");
			Presentation.EffectText = NSLOCTEXT("StarRoversBuildPresentation", "FlowCapacityEffect", "SUSTAIN SUPPLY TO EXPAND PLANETARY CAPACITY");
			break;
		case ESRFacilitySynthesisRoleV2::FleetBerth:
			Presentation.InputText = NSLOCTEXT("StarRoversBuildPresentation", "FlowFleetSupplyInput", "INDUSTRIAL SUPPLY");
			Presentation.ProcessText = NSLOCTEXT("StarRoversBuildPresentation", "FlowFleetBerth", "FLEET BERTH");
			Presentation.OutputText = NSLOCTEXT("StarRoversBuildPresentation", "FlowFleetOutput", "+ FLEET CAPACITY");
			Presentation.EffectText = NSLOCTEXT("StarRoversBuildPresentation", "FlowFleetEffect", "SUSTAIN SUPPLY TO EXPAND INTERPLANETARY LOGISTICS");
			break;
		case ESRFacilitySynthesisRoleV2::None:
		default:
			Presentation.InputText = NSLOCTEXT("StarRoversBuildPresentation", "FlowSynthesisInput", "INPUTS");
			Presentation.ProcessText = NSLOCTEXT("StarRoversBuildPresentation", "FlowSynthesisProcess", "SYNTHESIZE");
			Presentation.OutputText = NSLOCTEXT("StarRoversBuildPresentation", "FlowSynthesisOutput", "OUTPUT");
			break;
		}
	}
	else if (BuildOption.Role == ESRStructureBuildRole::TagProcessing)
	{
		Presentation.InputText = NSLOCTEXT("StarRoversBuildPresentation", "FlowTagInput", "CARD");
		if (BuildOption.ProcessRole == ESRFacilityProcessRoleV2::ClearProcessTag)
		{
			Presentation.ProcessText = NSLOCTEXT("StarRoversBuildPresentation", "FlowClearTag", "CLEAR TAG");
			Presentation.OutputText = NSLOCTEXT("StarRoversBuildPresentation", "FlowClearedCard", "CARD");
			Presentation.EffectText = NSLOCTEXT("StarRoversBuildPresentation", "FlowClearTagEffect", "CLEARS THE PROCESS TAG SLOT");
		}
		else
		{
			Presentation.ProcessText = NSLOCTEXT("StarRoversBuildPresentation", "FlowApplyTag", "APPLY TAG");
			Presentation.OutputText = NSLOCTEXT("StarRoversBuildPresentation", "FlowTaggedCard", "TAGGED CARD");
			Presentation.EffectText = BuildOption.ProcessTagId.IsNone()
				? NSLOCTEXT("StarRoversBuildPresentation", "FlowRecipeTag", "PAYLOAD SELECTED BY RECIPE")
				: FText::Format(
					NSLOCTEXT("StarRoversBuildPresentation", "FlowDefaultTag", "DEFAULT TAG · {0}"),
					UpperNameOr(BuildOption.ProcessTagId, FText::GetEmpty()));
		}
	}
	else if (BuildOption.Role == ESRStructureBuildRole::FuelImprinting)
	{
		Presentation.InputText = NSLOCTEXT("StarRoversBuildPresentation", "FlowImprintInput", "CARD");
		Presentation.ProcessText = NSLOCTEXT("StarRoversBuildPresentation", "FlowApplySeal", "APPLY SEAL");
		Presentation.OutputText = NSLOCTEXT("StarRoversBuildPresentation", "FlowImprintedCard", "IMPRINTED CARD");
		Presentation.EffectText = BuildOption.FuelImprintId.IsNone()
			? NSLOCTEXT("StarRoversBuildPresentation", "FlowRecipeSeal", "SEAL SELECTED BY RECIPE")
			: FText::Format(
				NSLOCTEXT("StarRoversBuildPresentation", "FlowDefaultSeal", "DEFAULT SEAL · {0}"),
				UpperNameOr(BuildOption.FuelImprintId, FText::GetEmpty()));
	}
	else if (BuildOption.Role == ESRStructureBuildRole::FamilyProcessing)
	{
		Presentation.InputText = BuildCardInputLabel(BuildOption.ResourceFamily);
		Presentation.ProcessText = UpperNameOr(
			BuildOption.ProcessArchetype,
			NSLOCTEXT("StarRoversBuildPresentation", "FlowProcess", "PROCESS"));
		Presentation.OutputText = BuildCardInputLabel(BuildOption.ResourceFamily);

		TArray<FText> Effects;
		const FText LineRoleEffect = BuildLineRoleEffect(BuildOption.LineRole);
		if (!LineRoleEffect.IsEmpty())
		{
			Effects.Add(LineRoleEffect);
		}
		if (!FMath::IsNearlyZero(BuildOption.FacilityEnergyDelta))
		{
			Effects.Add(FormatSignedEnergy(BuildOption.FacilityEnergyDelta));
		}
		const FText FamilyEffect = BuildFamilyActionEffect(BuildOption.FamilyAction);
		if (!FamilyEffect.IsEmpty())
		{
			Effects.Add(FamilyEffect);
		}
		Presentation.EffectText = Effects.IsEmpty()
			? NSLOCTEXT("StarRoversBuildPresentation", "FlowHistoryEffect", "FAMILY STATE DEPENDS ON INPUT HISTORY")
			: FText::Join(NSLOCTEXT("StarRoversBuildPresentation", "FlowEffectSeparator", "  ·  "), Effects);
	}
	else
	{
		Presentation.InputText = NSLOCTEXT("StarRoversBuildPresentation", "FlowGenericInput", "INPUT");
		Presentation.ProcessText = GetRoleLabel(BuildOption.Role);
		Presentation.OutputText = NSLOCTEXT("StarRoversBuildPresentation", "FlowGenericOutput", "OUTPUT");
	}

	Presentation.ToolTipText = BuildOption.LineRole == ESRFacilityLineRoleV2::UniversalBridge
		? NSLOCTEXT(
			"StarRoversBuildPresentation",
			"BridgeFlowContractTooltip",
			"A Universal Bridge advances negative Family pressure, but cannot activate or consume positive Family merit. Actual results still depend on input history.")
		: NSLOCTEXT(
			"StarRoversBuildPresentation",
			"FlowContractTooltip",
			"This diagram shows the authored static contract. Actual Family State and Tag results still depend on the incoming resource and its processing history.");
	return Presentation;
}

FSRStructureBuildPlacementPresentation FSRStructureBuildPresentationBuilder::BuildPlacement(
	const FSRStructureBuildOption& BuildOption,
	const FSRStructurePlacementPreview* LivePreview)
{
	FSRStructureBuildPlacementPresentation Presentation;
	if (!LivePreview)
	{
		Presentation.TargetText = BuildOption.IsSelectable()
			? NSLOCTEXT("StarRoversBuildPresentation", "PlacementSelectTarget", "TARGET · SELECT")
			: NSLOCTEXT("StarRoversBuildPresentation", "PlacementLockedTarget", "TARGET · LOCKED");
		Presentation.TargetVisualState = BuildOption.IsSelectable()
			? ESRUIVisualState::Info
			: USRUIThemeLibrary::ResolveBuildAvailabilityVisualState(BuildOption.Availability);
		Presentation.FootprintText = FText::Format(
			NSLOCTEXT("StarRoversBuildPresentation", "PlacementFootprint", "SIZE · {0}x{1}"),
			FText::AsNumber(BuildOption.FootprintCellsX),
			FText::AsNumber(BuildOption.FootprintCellsY));
		Presentation.CapacityText = FText::Format(
			NSLOCTEXT("StarRoversBuildPresentation", "PlacementLoad", "LOAD · +{0}"),
			FText::AsNumber(BuildOption.OperationalLoad));
		Presentation.DetailText = BuildOption.IsSelectable()
			? NSLOCTEXT("StarRoversBuildPresentation", "PlacementSelectDetail", "Select this Facility to inspect its live world target.")
			: FSRStructureBuildCatalogBuilder::BuildStatusText(BuildOption);
		return Presentation;
	}

	const FSRStructurePlacementPreview& Preview = *LivePreview;
	Presentation.TargetText = BuildTargetLabel(Preview.Status);
	Presentation.TargetVisualState = ResolveTargetVisualState(Preview.Status);
	Presentation.FootprintText = FText::Format(
		NSLOCTEXT("StarRoversBuildPresentation", "PlacementLiveFootprint", "SIZE · {0}x{1}"),
		FText::AsNumber(Preview.FootprintCellsX),
		FText::AsNumber(Preview.FootprintCellsY));
	if (Preview.bWillReplace)
	{
		Presentation.FootprintVisualState = ESRUIVisualState::Warning;
	}

	if (Preview.bHasCapacityData)
	{
		Presentation.CapacityText = FText::Format(
			NSLOCTEXT("StarRoversBuildPresentation", "PlacementCapacity", "CAP · {0} > {1} / {2}"),
			FText::AsNumber(Preview.CurrentDemand),
			FText::AsNumber(Preview.ProjectedDemand),
			FText::AsNumber(Preview.TotalCapacity));
		Presentation.CapacityVisualState = Preview.bCapacityWarning
			? ESRUIVisualState::Warning
			: ESRUIVisualState::Positive;
	}
	else
	{
		Presentation.CapacityText = FText::Format(
			NSLOCTEXT("StarRoversBuildPresentation", "PlacementCapacityUnavailable", "LOAD · +{0}"),
			FText::AsNumber(Preview.OperationalLoad));
	}

	TArray<FText> Details;
	if (!Preview.StatusText.IsEmpty())
	{
		Details.Add(Preview.StatusText);
	}
	if (!Preview.DetailText.IsEmpty())
	{
		Details.Add(Preview.DetailText);
	}
	Presentation.DetailText = FText::Join(
		NSLOCTEXT("StarRoversBuildPresentation", "PlacementStatusSeparator", "  ·  "),
		Details);
	return Presentation;
}

FText FSRStructureBuildPresentationBuilder::GetRoleLabel(ESRStructureBuildRole Role)
{
	switch (Role)
	{
	case ESRStructureBuildRole::Logistics: return NSLOCTEXT("StarRoversBuildPresentation", "RoleLogistics", "LOGISTICS");
	case ESRStructureBuildRole::Extraction: return NSLOCTEXT("StarRoversBuildPresentation", "RoleExtraction", "EXTRACTION");
	case ESRStructureBuildRole::FamilyProcessing: return NSLOCTEXT("StarRoversBuildPresentation", "RoleFamilyProcess", "FAMILY PROCESS");
	case ESRStructureBuildRole::TagProcessing: return NSLOCTEXT("StarRoversBuildPresentation", "RoleTagProcess", "TAG PROCESS");
	case ESRStructureBuildRole::FuelImprinting: return NSLOCTEXT("StarRoversBuildPresentation", "RoleFuelImprint", "FUEL IMPRINT");
	case ESRStructureBuildRole::Synthesis: return NSLOCTEXT("StarRoversBuildPresentation", "RoleSynthesis", "SYNTHESIS");
	case ESRStructureBuildRole::StellarFuelFabrication: return NSLOCTEXT("StarRoversBuildPresentation", "RoleStellarFuel", "STELLAR FUEL");
	case ESRStructureBuildRole::Infrastructure: return NSLOCTEXT("StarRoversBuildPresentation", "RoleInfrastructure", "INFRASTRUCTURE");
	case ESRStructureBuildRole::Hub: return NSLOCTEXT("StarRoversBuildPresentation", "RoleHub", "HUB");
	case ESRStructureBuildRole::General:
	default: return NSLOCTEXT("StarRoversBuildPresentation", "RoleGeneral", "GENERAL");
	}
}

FText FSRStructureBuildPresentationBuilder::GetFamilyLabel(ESRResourceFamily ResourceFamily)
{
	switch (ResourceFamily)
	{
	case ESRResourceFamily::Metal: return NSLOCTEXT("StarRoversBuildPresentation", "FamilyMetal", "METAL");
	case ESRResourceFamily::Crystal: return NSLOCTEXT("StarRoversBuildPresentation", "FamilyCrystal", "CRYSTAL");
	case ESRResourceFamily::Organic: return NSLOCTEXT("StarRoversBuildPresentation", "FamilyOrganic", "ORGANIC");
	case ESRResourceFamily::Plasma: return NSLOCTEXT("StarRoversBuildPresentation", "FamilyPlasma", "PLASMA");
	case ESRResourceFamily::Void: return NSLOCTEXT("StarRoversBuildPresentation", "FamilyVoid", "VOID");
	case ESRResourceFamily::None:
	default: return NSLOCTEXT("StarRoversBuildPresentation", "FamilyShared", "SHARED");
	}
}

FText FSRStructureBuildPresentationBuilder::GetRarityLabel(ESRFacilityRarity Rarity)
{
	switch (Rarity)
	{
	case ESRFacilityRarity::Starting: return NSLOCTEXT("StarRoversBuildPresentation", "RarityStarting", "STARTING");
	case ESRFacilityRarity::Basic: return NSLOCTEXT("StarRoversBuildPresentation", "RarityBasic", "BASIC");
	case ESRFacilityRarity::Advanced: return NSLOCTEXT("StarRoversBuildPresentation", "RarityAdvanced", "ADVANCED");
	case ESRFacilityRarity::HighTech: return NSLOCTEXT("StarRoversBuildPresentation", "RarityHighTech", "HIGH-TECH");
	case ESRFacilityRarity::Innovation: return NSLOCTEXT("StarRoversBuildPresentation", "RarityInnovation", "INNOVATION");
	default: return FText::GetEmpty();
	}
}

FText FSRStructureBuildPresentationBuilder::GetPriorityLabel(ESROperationalPriorityV2 Priority)
{
	switch (Priority)
	{
	case ESROperationalPriorityV2::Critical: return NSLOCTEXT("StarRoversBuildPresentation", "PriorityCritical", "Critical");
	case ESROperationalPriorityV2::Background: return NSLOCTEXT("StarRoversBuildPresentation", "PriorityBackground", "Background");
	case ESROperationalPriorityV2::Normal:
	default: return NSLOCTEXT("StarRoversBuildPresentation", "PriorityNormal", "Normal");
	}
}

FText FSRStructureBuildPresentationBuilder::GetAvailabilityLabel(
	ESRStructureBuildAvailability Availability)
{
	switch (Availability)
	{
	case ESRStructureBuildAvailability::LockedByAugment:
		return NSLOCTEXT("StarRoversBuildPresentation", "AvailabilityLocked", "AUGMENT LOCK");
	case ESRStructureBuildAvailability::ConstructionDisabled:
		return NSLOCTEXT("StarRoversBuildPresentation", "AvailabilityDisabled", "UNAVAILABLE");
	case ESRStructureBuildAvailability::Available:
	default:
		return NSLOCTEXT("StarRoversBuildPresentation", "AvailabilityReady", "READY");
	}
}
