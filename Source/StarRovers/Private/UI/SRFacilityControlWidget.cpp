#include "UI/SRFacilityControlWidget.h"

#include "UI/SRFacilityInspectorPresentation.h"
#include "UI/SRHubRoutePresentation.h"
#include "UI/SRResourceGlyph.h"
#include "UI/SRUIComponents.h"
#include "UI/SRUILayoutPolicy.h"
#include "UI/SRUITheme.h"

#include "Utility/SRLog.h"
#include "Automation/SRFacilityNetworkComponent.h"
#include "Automation/SRFacilityProcessContextResolver.h"
#include "Automation/SRFacilityProcessingRuleEvaluator.h"
#include "Automation/SRFacilityResourceOperations.h"
#include "Automation/SRFacilityResourceV2Processor.h"
#include "Automation/SROperationalEconomyProcessor.h"
#include "Automation/SRResourceInstanceOperations.h"
#include "Automation/SRResourceSystemContent.h"
#include "Automation/SRStellarFuelBatchPlanner.h"
#include "Automation/SRStellarFuelFabricator.h"
#include "Simulation/SRAugmentSubsystem.h"
#include "Simulation/SRSimulationSettings.h"
#include "Blueprint/WidgetTree.h"
#include "Camera/SRPlayerController.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "Celestial/SRStar.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CheckBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Widget.h"
#include "Fonts/SlateFontInfo.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Logistics/SRFleetCapacityV2.h"
#include "Logistics/SRConditionedTransitV2.h"
#include "Logistics/SRSpaceLogisticsSubsystem.h"
#include "Simulation/SRCelestialBodyRegistrySubsystem.h"
#include "Structure/SRStructureInstanceManagerComponent.h"
#include "Structure/SRStructureDataAsset.h"

namespace
{
	constexpr int32 FacilityInventoryGridColumnCount = 4;

	FString FormatFacilityEnergyDisplayValue(double Value)
	{
		return FString::Printf(TEXT("%.1f"), Value);
	}

	bool UsesResourceV2Display()
	{
		return FSRFacilityResourceV2Processor::IsResourceV2RulesetActive();
	}

	double ResolveDisplayedEnergy(const FSRResourceInstance& ResourceInstance)
	{
		return UsesResourceV2Display()
			? ResourceInstance.CurrentEnergy
			: ResourceInstance.EnergyValue;
	}

	FString GetResourceV2FamilyLabel(ESRResourceFamily Family)
	{
		const UEnum* FamilyEnum = StaticEnum<ESRResourceFamily>();
		return FamilyEnum
			? FamilyEnum->GetDisplayNameTextByValue(static_cast<int64>(Family)).ToString()
			: TEXT("Unknown");
	}

	FString GetResourceV2SpectrumLabel(ESRResourceSpectrum Spectrum)
	{
		const UEnum* SpectrumEnum = StaticEnum<ESRResourceSpectrum>();
		return SpectrumEnum
			? SpectrumEnum->GetDisplayNameTextByValue(static_cast<int64>(Spectrum)).ToString()
			: TEXT("Unknown");
	}

	FString GetResourceV2RecipeDisplayName(FName RecipeId)
	{
		FSRProcessTagDefinitionV2 ProcessTagDefinition;
		if (FSRResourceSystemContent::TryGetProcessTagDefinition(RecipeId, ProcessTagDefinition))
		{
			return ProcessTagDefinition.DisplayName.IsEmpty()
				? RecipeId.ToString()
				: ProcessTagDefinition.DisplayName.ToString();
		}

		FSRFuelImprintDefinitionV2 FuelImprintDefinition;
		if (FSRResourceSystemContent::TryGetFuelImprintDefinition(RecipeId, FuelImprintDefinition))
		{
			return FuelImprintDefinition.DisplayName.IsEmpty()
				? RecipeId.ToString()
				: FuelImprintDefinition.DisplayName.ToString();
		}
		return RecipeId.IsNone() ? TEXT("None") : RecipeId.ToString();
	}

	FString BuildResourceV2StateLabel(const FSRResourceInstance& ResourceInstance, int32 MaximumLabels = 2)
	{
		TArray<FString> Labels;
		const UEnum* StateEnum = StaticEnum<ESRResourceFamilyState>();
		for (int32 StateIndex = 0;
			StateIndex <= static_cast<int32>(ESRResourceFamilyState::Collapsed) && Labels.Num() < MaximumLabels;
			++StateIndex)
		{
			if ((ResourceInstance.ActiveFamilyStateFlags & (1 << StateIndex)) == 0)
			{
				continue;
			}
			Labels.Add(StateEnum
				? StateEnum->GetDisplayNameTextByValue(StateIndex).ToString()
				: FString::FromInt(StateIndex));
		}
		return Labels.IsEmpty() ? TEXT("No State") : FString::Join(Labels, TEXT(" "));
	}

	const TCHAR* GetFacilityTemperatureLabel(ESRFacilityTemperatureState TemperatureState)
	{
		switch (TemperatureState)
		{
		case ESRFacilityTemperatureState::Frozen:
			return TEXT("Frozen");
		case ESRFacilityTemperatureState::Cold:
			return TEXT("Cold");
		case ESRFacilityTemperatureState::Normal:
			return TEXT("Normal");
		case ESRFacilityTemperatureState::Hot:
			return TEXT("Hot");
		case ESRFacilityTemperatureState::Overheated:
			return TEXT("Overheated");
		default:
			return TEXT("Unknown");
		}
	}

	const TCHAR* GetResourceProcessTagLabel(ESRResourceProcessTag ResourceTag)
	{
		switch (ResourceTag)
		{
		case ESRResourceProcessTag::Responsive:
			return TEXT("HeatResponsive");
		case ESRResourceProcessTag::HalfLife:
			return TEXT("HalfLife");
		case ESRResourceProcessTag::Volatile:
			return TEXT("Volatile");
		case ESRResourceProcessTag::Supercooled:
			return TEXT("Supercooled");
		case ESRResourceProcessTag::HyperReactive:
			return TEXT("HyperReactive");
		case ESRResourceProcessTag::Charge:
			return TEXT("Charge");
		default:
			return TEXT("Tag");
		}
	}

	const TCHAR* GetEffectKindLabel(ESRFacilityEffectKind EffectKind)
	{
		switch (EffectKind)
		{
		case ESRFacilityEffectKind::AdjustEnergy:
			return TEXT("Adjust Catalyst");
		case ESRFacilityEffectKind::AdjustProcessLimit:
			return TEXT("Adjust Limit");
		case ESRFacilityEffectKind::RemoveResource:
			return TEXT("Remove Resource");
		case ESRFacilityEffectKind::AttachTag:
			return TEXT("Attach Tag");
		case ESRFacilityEffectKind::ProduceWaste:
			return TEXT("Produce Waste");
		case ESRFacilityEffectKind::AdjustCellTemperature:
			return TEXT("Adjust Cell Temp");
		case ESRFacilityEffectKind::InvertHeat:
			return TEXT("Invert Heat");
		case ESRFacilityEffectKind::InvertTagEffects:
			return TEXT("Invert Catalyst");
		case ESRFacilityEffectKind::DoubleTagEffects:
			return TEXT("Double Catalyst");
		case ESRFacilityEffectKind::DuplicateInputResource:
			return TEXT("Duplicate Input");
		case ESRFacilityEffectKind::OverrideProcessTemperature:
			return TEXT("Override Process Temp");
		case ESRFacilityEffectKind::TriggerTagEffect:
			return TEXT("Trigger Tag Effect");
		case ESRFacilityEffectKind::AdjustProcessTime:
			return TEXT("Adjust Process Time");
		case ESRFacilityEffectKind::RemoveTag:
			return TEXT("Remove Tag");
		case ESRFacilityEffectKind::ChangeResourceType:
			return TEXT("Change Resource Type");
		case ESRFacilityEffectKind::TransferTagsToWaste:
			return TEXT("Transfer Tags To Waste");
		default:
			return TEXT("Effect");
		}
	}

	const TCHAR* GetAttachTagSourceLabel(ESRFacilityAttachTagSource AttachTagSource)
	{
		switch (AttachTagSource)
		{
		case ESRFacilityAttachTagSource::SpecificTag:
			return TEXT("Specific");
		case ESRFacilityAttachTagSource::LastAttachedTag:
			return TEXT("Last Tag");
		case ESRFacilityAttachTagSource::MissingTags:
			return TEXT("Missing Tags");
		case ESRFacilityAttachTagSource::AttachedTags:
			return TEXT("Attached Tags");
		default:
			return TEXT("Tag Source");
		}
	}

	const TCHAR* GetEffectTagTargetLabel(ESRFacilityEffectTagTarget TagTarget)
	{
		switch (TagTarget)
		{
		case ESRFacilityEffectTagTarget::SpecificTag:
			return TEXT("Specific");
		case ESRFacilityEffectTagTarget::LastAttachedTag:
			return TEXT("Last Tag");
		case ESRFacilityEffectTagTarget::AllTags:
			return TEXT("Attached Tags");
		default:
			return TEXT("Tag Target");
		}
	}

	FString BuildRemoveTagAmountSummary(const FSRFacilityEffectSpec& EffectSpec)
	{
		return EffectSpec.RemoveTagAmountMode == ESRFacilityRemoveTagAmountMode::Count
			? FString::Printf(TEXT(" x%d"), FMath::Max(1, EffectSpec.Count))
			: FString(TEXT(" All"));
	}

	const TCHAR* GetEnergyValueSourceLabel(ESRFacilityEnergyAdjustmentValueSource ValueSource)
	{
		switch (ValueSource)
		{
		case ESRFacilityEnergyAdjustmentValueSource::FixedValue:
			return TEXT("Fixed");
		case ESRFacilityEnergyAdjustmentValueSource::RemainingProcessLimit:
			return TEXT("Limit");
		case ESRFacilityEnergyAdjustmentValueSource::TagStackCount:
			return TEXT("Tag Stack");
		case ESRFacilityEnergyAdjustmentValueSource::EnergyChangeCount:
			return TEXT("Energy Changes");
		case ESRFacilityEnergyAdjustmentValueSource::TagEffectEnergyChangeAmount:
			return TEXT("Energy (B)");
		case ESRFacilityEnergyAdjustmentValueSource::ProcessCount:
			return TEXT("Process Count");
		case ESRFacilityEnergyAdjustmentValueSource::TagKindCount:
			return TEXT("Tag Kinds");
		default:
			return TEXT("Value");
		}
	}

	const TCHAR* GetTagStackCountTargetLabel(ESRFacilityTagStackCountTarget TagStackCountTarget)
	{
		return TagStackCountTarget == ESRFacilityTagStackCountTarget::All ? TEXT("All") : TEXT("Specific");
	}

	const TCHAR* GetEnergyAdjustmentModeLabel(ESRFacilityEnergyAdjustmentMode AdjustmentMode)
	{
		switch (AdjustmentMode)
		{
		case ESRFacilityEnergyAdjustmentMode::Multiply:
			return TEXT("*");
		case ESRFacilityEnergyAdjustmentMode::Subtract:
			return TEXT("-");
		case ESRFacilityEnergyAdjustmentMode::Add:
		default:
			return TEXT("+");
		}
	}

	FString BuildProcessLimitAdjustmentSummary(const FSRFacilityEffectSpec& EffectSpec)
	{
		switch (EffectSpec.ProcessLimitMode)
		{
		case ESRFacilityProcessLimitAdjustmentMode::SetValue:
			return FString::Printf(TEXT("= %d"), FMath::RoundToInt(EffectSpec.Value));
		case ESRFacilityProcessLimitAdjustmentMode::Multiply:
			return FString::Printf(TEXT("x%s"), *FormatFacilityEnergyDisplayValue(EffectSpec.Value));
		case ESRFacilityProcessLimitAdjustmentMode::AddValue:
		default:
			return FString::Printf(TEXT("%+d"), FMath::RoundToInt(EffectSpec.Value));
		}
	}

	FString BuildEnergyValueMultiplierSummary(const FSRFacilityEffectSpec& EffectSpec)
	{
		if (EffectSpec.EnergyValueSource == ESRFacilityEnergyAdjustmentValueSource::FixedValue
			|| FMath::IsNearlyEqual(EffectSpec.EnergyValueMultiplier, 1.0))
		{
			return FString();
		}
		return FString::Printf(TEXT(" x%s"), *FormatFacilityEnergyDisplayValue(EffectSpec.EnergyValueMultiplier));
	}

	const TCHAR* GetProcessTimeModeLabel(ESRFacilityProcessTimeAdjustmentMode ProcessTimeMode)
	{
		return ProcessTimeMode == ESRFacilityProcessTimeAdjustmentMode::Multiply ? TEXT("*") : TEXT("+");
	}

	const TCHAR* GetProcessTimeValueSourceLabel(ESRFacilityProcessTimeAdjustmentValueSource ValueSource)
	{
		switch (ValueSource)
		{
		case ESRFacilityProcessTimeAdjustmentValueSource::TagStackCount:
			return TEXT("Tag Stack");
		case ESRFacilityProcessTimeAdjustmentValueSource::FixedValue:
		default:
			return TEXT("Fixed");
		}
	}

	const TCHAR* GetEffectConditionKindLabel(ESRFacilityEffectConditionKind ConditionKind)
	{
		switch (ConditionKind)
		{
		case ESRFacilityEffectConditionKind::EnergyAtLeast:
			return TEXT("Energy >=");
		case ESRFacilityEffectConditionKind::EnergyAtMost:
			return TEXT("Energy <=");
		case ESRFacilityEffectConditionKind::EnergyGreaterThan:
			return TEXT("Energy >");
		case ESRFacilityEffectConditionKind::EnergyLessThan:
			return TEXT("Energy <");
		case ESRFacilityEffectConditionKind::EnergyIncreased:
			return TEXT("Energy Increased");
		case ESRFacilityEffectConditionKind::EnergyDecreased:
			return TEXT("Energy Decreased");
		case ESRFacilityEffectConditionKind::Tag:
			return TEXT("Tag");
		case ESRFacilityEffectConditionKind::TemperatureState:
			return TEXT("Temp");
		case ESRFacilityEffectConditionKind::ProcessCountEquals:
			return TEXT("Process Count >=");
		case ESRFacilityEffectConditionKind::PrimeEnergy:
			return TEXT("Prime Energy");
		default:
			return TEXT("Condition");
		}
	}

	const TCHAR* GetTagConditionModeLabel(ESRFacilityTagConditionMode TagMode)
	{
		switch (TagMode)
		{
		case ESRFacilityTagConditionMode::HasTag:
			return TEXT("Has");
		case ESRFacilityTagConditionMode::MissingTag:
			return TEXT("Missing");
		case ESRFacilityTagConditionMode::StackCountAtLeast:
			return TEXT("Stacks >=");
		default:
			return TEXT("Tag");
		}
	}

	const TCHAR* GetTagConditionTargetLabel(ESRFacilityTagConditionTarget TagTarget)
	{
		switch (TagTarget)
		{
		case ESRFacilityTagConditionTarget::SpecificTag:
			return TEXT("Specific");
		case ESRFacilityTagConditionTarget::AllTags:
			return TEXT("All Tags");
		default:
			return TEXT("Tag Target");
		}
	}

	const TCHAR* GetConditionLogicLabel(ESRFacilityConditionLogic ConditionLogic)
	{
		return ConditionLogic == ESRFacilityConditionLogic::Or ? TEXT("||") : TEXT("&&");
	}

	FString GetHubRouteProfileShortLabel(ESRSpaceLogisticsRouteProfileV2 Profile)
	{
		switch (Profile)
		{
		case ESRSpaceLogisticsRouteProfileV2::CardCourier:
			return TEXT("Courier");
		case ESRSpaceLogisticsRouteProfileV2::BulkRawHold:
			return TEXT("Bulk");
		case ESRSpaceLogisticsRouteProfileV2::ConditionedHold:
			return TEXT("Conditioned");
		case ESRSpaceLogisticsRouteProfileV2::NeutralShuttle:
		default:
			return TEXT("Shuttle");
		}
	}

	FString GetConditionedTransitModuleShortLabel(ESRConditionedTransitModuleV2 Module)
	{
		switch (Module)
		{
		case ESRConditionedTransitModuleV2::CryogenicHold:
			return TEXT("Cryogenic");
		case ESRConditionedTransitModuleV2::BioCultureHold:
			return TEXT("Bio-Culture");
		case ESRConditionedTransitModuleV2::GroundingHold:
			return TEXT("Grounding");
		case ESRConditionedTransitModuleV2::None:
		default:
			return TEXT("Neutral");
		}
	}

	ESRSpaceLogisticsRouteProfileV2 ResolveNextUnlockedRouteProfile(
		ESRSpaceLogisticsRouteProfileV2 CurrentProfile,
		const USRSpaceLogisticsSubsystem* SpaceLogisticsSubsystem)
	{
		TArray<ESRSpaceLogisticsRouteProfileV2> Profiles;
		FSRFleetCapacityV2::GetRouteProfiles(Profiles);
		const int32 CurrentIndex = Profiles.IndexOfByKey(CurrentProfile);
		const int32 StartIndex = CurrentIndex == INDEX_NONE ? 0 : CurrentIndex;
		for (int32 Offset = 1; Offset <= Profiles.Num(); ++Offset)
		{
			const ESRSpaceLogisticsRouteProfileV2 Candidate =
				Profiles[(StartIndex + Offset) % Profiles.Num()];
			if (IsValid(SpaceLogisticsSubsystem)
				&& SpaceLogisticsSubsystem->IsHubRouteProfileUnlocked(Candidate))
			{
				return Candidate;
			}
		}
		return CurrentProfile;
	}

	bool IsConditionedTransitModuleUnlocked(
		ESRConditionedTransitModuleV2 Module,
		const USRAugmentSubsystem* AugmentSubsystem)
	{
		if (Module == ESRConditionedTransitModuleV2::None)
		{
			return true;
		}
		const FSRConditionedTransitModuleRulesV2 Rules = FSRConditionedTransitV2::GetModuleRules(Module);
		return Rules.IsConditionedModule()
			&& IsValid(AugmentSubsystem)
			&& AugmentSubsystem->IsLogisticsModuleUnlockedV2(Rules.UnlockModuleId);
	}

	ESRConditionedTransitModuleV2 ResolveNextUnlockedConditionedTransitModule(
		ESRConditionedTransitModuleV2 CurrentModule,
		const USRAugmentSubsystem* AugmentSubsystem)
	{
		TArray<ESRConditionedTransitModuleV2> Candidates = {
			ESRConditionedTransitModuleV2::None,
		};
		TArray<ESRConditionedTransitModuleV2> ConditionedModules;
		FSRConditionedTransitV2::GetConditionedModules(ConditionedModules);
		Candidates.Append(ConditionedModules);
		const int32 CurrentIndex = Candidates.IndexOfByKey(CurrentModule);
		const int32 StartIndex = CurrentIndex == INDEX_NONE ? 0 : CurrentIndex;
		for (int32 Offset = 1; Offset <= Candidates.Num(); ++Offset)
		{
			const ESRConditionedTransitModuleV2 Candidate =
				Candidates[(StartIndex + Offset) % Candidates.Num()];
			if (IsConditionedTransitModuleUnlocked(Candidate, AugmentSubsystem))
			{
				return Candidate;
			}
		}
		return ESRConditionedTransitModuleV2::None;
	}

	FString BuildConditionedTransitPreviewLabel(
		const FSRSpaceLogisticsHubRoute& Route,
		const USRAugmentSubsystem* AugmentSubsystem)
	{
		const FSRConditionedTransitModuleRulesV2 Rules =
			FSRConditionedTransitV2::GetModuleRules(Route.ConditionedTransitModule);
		if (!Rules.IsConditionedModule())
		{
			return TEXT("Hold: Neutral (state-neutral)");
		}
		const bool bUnlocked = IsConditionedTransitModuleUnlocked(
			Route.ConditionedTransitModule,
			AugmentSubsystem);
		if (!bUnlocked)
		{
			return FString::Printf(TEXT("Hold: %s (locked)"), *GetConditionedTransitModuleShortLabel(Route.ConditionedTransitModule));
		}

		const bool bTraveling =
			Route.Phase == ESRSpaceLogisticsHubRoutePhase::TravelingToDestination
			|| Route.Phase == ESRSpaceLogisticsHubRoutePhase::TravelingToSource;
		const bool bConditioning =
			Route.Phase == ESRSpaceLogisticsHubRoutePhase::ConditioningAtDestination
			|| Route.Phase == ESRSpaceLogisticsHubRoutePhase::ConditioningAtSource;
		if (!Route.Cargo.ResourceId.IsNone()
			&& Route.Cargo.StackCount > 0
			&& (bTraveling || bConditioning))
		{
			const bool bOutbound =
				Route.Phase == ESRSpaceLogisticsHubRoutePhase::TravelingToDestination
				|| Route.Phase == ESRSpaceLogisticsHubRoutePhase::ConditioningAtDestination;
			const FName SourceBodyId = StarRovers::Resources::ResolveCelestialBodyResourceId(
				bOutbound ? Route.SourceHub.BodyActor.Get() : Route.DestinationHub.BodyActor.Get());
			const FName DestinationBodyId = StarRovers::Resources::ResolveCelestialBodyResourceId(
				bOutbound ? Route.DestinationHub.BodyActor.Get() : Route.SourceHub.BodyActor.Get());
			const FSRConditionedTransitResultV2 Preview = FSRConditionedTransitV2::EvaluateArrival(
				Route.Cargo,
				Route.RouteProfile,
				Route.ConditionedTransitModule,
				SourceBodyId,
				DestinationBodyId,
				true);
			if (Preview.bProcessApplied)
			{
				if (bConditioning)
				{
					return FString::Printf(
						TEXT("%s conditioning %.1f/%.1fs | E %.1f -> %.1f (%+.1f)"),
						*GetConditionedTransitModuleShortLabel(Route.ConditionedTransitModule),
						Route.ConditioningProgressSeconds,
						Route.ConditioningDurationSeconds,
						Preview.ProcessResult.InputEnergy,
						Preview.ProcessResult.OutputEnergy,
						Preview.ProcessResult.AppliedEnergyDelta);
				}
				const USRSimulationSettings* Settings = GetDefault<USRSimulationSettings>();
				const FSRConditioningDwellResultV2 Dwell = FSRConditionedTransitV2::EvaluateConditioningDwell(
					Route.RouteProfile,
					Route.ConditionedTransitModule,
					Route.Cargo,
					IsValid(Settings) ? Settings->RefinementResistanceEnergyScaleV2 : 40.0);
				return FString::Printf(
					TEXT("%s preview: %.1fs then E %.1f -> %.1f (%+.1f)"),
					*GetConditionedTransitModuleShortLabel(Route.ConditionedTransitModule),
					Dwell.RefinementResistance.EffectiveProcessSeconds,
					Preview.ProcessResult.InputEnergy,
					Preview.ProcessResult.OutputEnergy,
					Preview.ProcessResult.AppliedEnergyDelta);
			}
			return FString::Printf(
				TEXT("%s preview: %s"),
				*GetConditionedTransitModuleShortLabel(Route.ConditionedTransitModule),
				*Preview.FailureReason);
		}

		switch (Route.ConditionedTransitModule)
		{
		case ESRConditionedTransitModuleV2::CryogenicHold:
			return TEXT("Hold: Cryogenic | Metal Cold +3");
		case ESRConditionedTransitModuleV2::BioCultureHold:
			return TEXT("Hold: Bio-Culture | Organic Growth");
		case ESRConditionedTransitModuleV2::GroundingHold:
			return TEXT("Hold: Grounding | Plasma Discharge +1");
		case ESRConditionedTransitModuleV2::None:
		default:
			return TEXT("Hold: Neutral (state-neutral)");
		}
	}

	bool AreHubEndpointKeysEqual(const FSRSpaceLogisticsHubEndpoint& Left, const FSRSpaceLogisticsHubEndpoint& Right)
	{
		return Left.BodyActor == Right.BodyActor && Left.HubOccupantId == Right.HubOccupantId;
	}

	bool DoesHubRouteConnectEndpoints(
		const FSRSpaceLogisticsHubRoute& Route,
		const FSRSpaceLogisticsHubEndpoint& SourceHub,
		const FSRSpaceLogisticsHubEndpoint& DestinationHub)
	{
		return (AreHubEndpointKeysEqual(Route.SourceHub, SourceHub) && AreHubEndpointKeysEqual(Route.DestinationHub, DestinationHub))
			|| (AreHubEndpointKeysEqual(Route.SourceHub, DestinationHub) && AreHubEndpointKeysEqual(Route.DestinationHub, SourceHub));
	}

	FString BuildCelestialBodyDisplayName(const AActor* BodyActor)
	{
		if (!IsValid(BodyActor))
		{
			return TEXT("Unknown Body");
		}

		const FText BodyName = USRCelestialBodyRuntimeLibrary::GetCelestialVariableName(BodyActor);
		return BodyName.IsEmpty()
			? GetNameSafe(BodyActor)
			: BodyName.ToString();
	}

	AActor* ResolvePrimaryStarOrbitBody(const AActor* BodyActor, const AActor* PrimaryStarActor)
	{
		if (!IsValid(BodyActor) || !IsValid(PrimaryStarActor) || BodyActor == PrimaryStarActor)
		{
			return nullptr;
		}

		AActor* CurrentBody = const_cast<AActor*>(BodyActor);
		for (int32 ParentDepth = 0; ParentDepth < 16 && IsValid(CurrentBody); ++ParentDepth)
		{
			AActor* ParentBody = nullptr;
			if (!USRCelestialBodyRuntimeLibrary::GetCelestialParentBody(CurrentBody, ParentBody) || !IsValid(ParentBody))
			{
				return nullptr;
			}

			if (ParentBody == PrimaryStarActor)
			{
				return CurrentBody;
			}

			CurrentBody = ParentBody;
		}

		return nullptr;
	}

	float ResolveStarOrbitSortValue(const AActor* BodyActor, const AActor* PrimaryStarActor)
	{
		float OrbitRadius = 0.0f;
		if (USRCelestialBodyRuntimeLibrary::GetCelestialOrbitRadius(BodyActor, OrbitRadius) && OrbitRadius > KINDA_SMALL_NUMBER)
		{
			return OrbitRadius;
		}

		return IsValid(BodyActor) && IsValid(PrimaryStarActor)
			? FVector::DistSquared(BodyActor->GetActorLocation(), PrimaryStarActor->GetActorLocation())
			: BIG_NUMBER;
	}

	int32 ResolveStarOrbitOrdinal(const AActor* BodyActor, UWorld* World, bool& bOutIsSatellite)
	{
		bOutIsSatellite = false;
		if (!IsValid(BodyActor) || !IsValid(World) || USRCelestialBodyRuntimeLibrary::IsCelestialStarActor(BodyActor))
		{
			return INDEX_NONE;
		}

		const USRCelestialBodyRegistrySubsystem* CelestialRegistry = World->GetSubsystem<USRCelestialBodyRegistrySubsystem>();
		if (!IsValid(CelestialRegistry))
		{
			return INDEX_NONE;
		}

		AActor* PrimaryStarActor = CelestialRegistry->GetPrimaryStarActor();
		if (!IsValid(PrimaryStarActor))
		{
			return INDEX_NONE;
		}

		AActor* TargetStarOrbitBody = ResolvePrimaryStarOrbitBody(BodyActor, PrimaryStarActor);
		if (!IsValid(TargetStarOrbitBody))
		{
			return INDEX_NONE;
		}
		bOutIsSatellite = TargetStarOrbitBody != BodyActor;

		TArray<AActor*> CelestialBodies;
		CelestialRegistry->GetCelestialBodies(CelestialBodies);

		TArray<AActor*> StarOrbitBodies;
		TSet<AActor*> UniqueStarOrbitBodies;
		StarOrbitBodies.Reserve(CelestialBodies.Num());
		UniqueStarOrbitBodies.Reserve(CelestialBodies.Num());
		for (AActor* CelestialBody : CelestialBodies)
		{
			if (!IsValid(CelestialBody) || CelestialBody == PrimaryStarActor)
			{
				continue;
			}

			AActor* StarOrbitBody = ResolvePrimaryStarOrbitBody(CelestialBody, PrimaryStarActor);
			if (IsValid(StarOrbitBody))
			{
				bool bAlreadyAdded = false;
				UniqueStarOrbitBodies.Add(StarOrbitBody, &bAlreadyAdded);
				if (!bAlreadyAdded)
				{
					StarOrbitBodies.Add(StarOrbitBody);
				}
			}
		}

		StarOrbitBodies.Sort(
			[PrimaryStarActor](const AActor& LeftBody, const AActor& RightBody)
			{
				const float LeftSortValue = ResolveStarOrbitSortValue(&LeftBody, PrimaryStarActor);
				const float RightSortValue = ResolveStarOrbitSortValue(&RightBody, PrimaryStarActor);
				if (!FMath::IsNearlyEqual(LeftSortValue, RightSortValue))
				{
					return LeftSortValue < RightSortValue;
				}

				return LeftBody.GetFName().LexicalLess(RightBody.GetFName());
			});

		for (int32 BodyIndex = 0; BodyIndex < StarOrbitBodies.Num(); ++BodyIndex)
		{
			if (StarOrbitBodies[BodyIndex] == TargetStarOrbitBody)
			{
				return BodyIndex + 1;
			}
		}

		return INDEX_NONE;
	}

	FString BuildHubEndpointUILabel(const FSRSpaceLogisticsHubEndpoint& HubEndpoint)
	{
		const FString HubName = HubEndpoint.DisplayName.IsEmpty()
			? HubEndpoint.HubOccupantId.ToString()
			: HubEndpoint.DisplayName.ToString();
		const FString BodyName = BuildCelestialBodyDisplayName(HubEndpoint.BodyActor.Get());
		return FString::Printf(
			TEXT("%s / %s"),
			*BodyName,
			*HubName);
	}

	FString BuildCompactResourceIdLabel(FName ResourceId)
	{
		if (ResourceId.IsNone())
		{
			return TEXT("Any");
		}

		FString Label = ResourceId.ToString();
		if (Label.Len() > 10)
		{
			Label = Label.Left(9) + TEXT(".");
		}
		return Label;
	}

	ASRStar* ResolvePrimaryStarForHubUI(UWorld* World)
	{
		USRCelestialBodyRegistrySubsystem* CelestialRegistry = IsValid(World)
			? World->GetSubsystem<USRCelestialBodyRegistrySubsystem>()
			: nullptr;
		if (!IsValid(CelestialRegistry))
		{
			return nullptr;
		}

		AActor* PrimaryStarActor = CelestialRegistry->GetPrimaryStarActor();
		if (!IsValid(PrimaryStarActor))
		{
			CelestialRegistry->RefreshCelestialBodies();
			PrimaryStarActor = CelestialRegistry->GetPrimaryStarActor();
		}

		return Cast<ASRStar>(PrimaryStarActor);
	}

	bool CanUseAsStarFuelMissileCargo(const FSRResourceInstance& ResourceInstance)
	{
		return !ResourceInstance.ResourceId.IsNone()
			&& ResourceInstance.StackCount > 0
			&& ResourceInstance.EnergyValue > UE_DOUBLE_SMALL_NUMBER;
	}

	int32 CountAvailableStarFuelMissileCargoStacks(const FSRFacilityInstance& FacilityInstance, const ASRStar* TargetStar)
	{
		if (!IsValid(TargetStar))
		{
			return 0;
		}

		int32 MissileCargoStackCount = 0;
		for (const FSRFacilityPortInventory& InputPortInventory : FacilityInstance.InputPortInventories)
		{
			for (const FSRResourceInstance& ResourceInstance : InputPortInventory.Inventory)
			{
				if (CanUseAsStarFuelMissileCargo(ResourceInstance))
				{
					MissileCargoStackCount += FMath::Max(0, ResourceInstance.StackCount);
				}
			}
		}
		return MissileCargoStackCount;
	}

	int32 CountActiveStarFuelMissilesForHub(
		const TArray<FSRSpaceLogisticsStarFuelMissile>& StarFuelMissiles,
		const FSRSpaceLogisticsHubEndpoint& SourceHub)
	{
		int32 ActiveMissileCount = 0;
		for (const FSRSpaceLogisticsStarFuelMissile& Missile : StarFuelMissiles)
		{
			if (Missile.bEnabled && AreHubEndpointKeysEqual(Missile.SourceHub, SourceHub))
			{
				++ActiveMissileCount;
			}
		}
		return ActiveMissileCount;
	}

	FString BuildResourceDisplayName(const FSRResourceInstance& ResourceInstance)
	{
		return FSRResourceGlyphPresentationBuilder::Build(ResourceInstance)
			.DisplayName.ToString();
	}

	FString BuildResourceDataAssetDisplayName(const USRResourceDataAsset* ResourceDataAsset)
	{
		if (!IsValid(ResourceDataAsset))
		{
			return TEXT("UnknownResource");
		}

		if (!ResourceDataAsset->DisplayName.IsEmpty())
		{
			return ResourceDataAsset->DisplayName.ToString();
		}

		return ResourceDataAsset->ResourceId.IsNone()
			? GetNameSafe(ResourceDataAsset)
			: ResourceDataAsset->ResourceId.ToString();
	}

	FString BuildResourceSummary(const FSRResourceInstance& ResourceInstance)
	{
		return FSRResourceGlyphPresentationBuilder::Build(ResourceInstance)
			.ToolTipText.ToString();
	}

	FString BuildResourceSlotText(
		const TCHAR* Label,
		int32 SlotIndex,
		FName PortId,
		const FSRResourceInstance* ResourceInstance,
		const TCHAR* EmptyText)
	{
		const FString PortLabel = PortId.IsNone()
			? FString::Printf(TEXT("%s %d"), Label, SlotIndex + 1)
			: PortId.ToString();
		const FString ResourceText = ResourceInstance
			? FSRResourceGlyphPresentationBuilder::Build(*ResourceInstance).StableSignature
			: FString(EmptyText);
		return FString::Printf(TEXT("%s %d\n%s\n%s"), Label, SlotIndex + 1, *PortLabel, *ResourceText);
	}

	FString BuildInventoryCardPortLabel(const FSRFacilityPortInventory& PortInventory, int32 SlotIndex, const TCHAR* FallbackLabel)
	{
		return PortInventory.PortId.IsNone()
			? FString::Printf(TEXT("%s_%02d"), FallbackLabel, SlotIndex + 1)
			: PortInventory.PortId.ToString();
	}

	FString BuildInventorySlotSignature(const FSRFacilityPortInventory& PortInventory)
	{
		FString Signature = FString::Printf(
			TEXT("%s:%d:%d"),
			*PortInventory.PortId.ToString(),
			PortInventory.Capacity,
			StarRovers::FacilityResources::GetInventorySlotStackCount(PortInventory));
		for (const FSRResourceInstance& ResourceInstance : PortInventory.Inventory)
		{
			Signature += FString::Printf(
				TEXT("|%s:%.3f:%d:%d:%d:%d:V2/%d/%d/%.3f/%d/%d/%X/%s/%d/%d/H/%s/%d/%d/%s/%s/%d"),
				*ResourceInstance.ResourceId.ToString(),
				ResourceInstance.EnergyValue,
				ResourceInstance.RemainingProcessLimit,
				ResourceInstance.ProcessCount,
				ResourceInstance.EnergyChangeCount,
				ResourceInstance.StackCount,
				ResourceInstance.ResourceSchemaVersion,
				static_cast<int32>(ResourceInstance.Family),
				ResourceInstance.CurrentEnergy,
				static_cast<int32>(ResourceInstance.Spectrum),
				ResourceInstance.Grade,
				ResourceInstance.ActiveFamilyStateFlags,
				*ResourceInstance.ProcessTagSlot.TagId.ToString(),
				static_cast<int32>(ResourceInstance.ProcessTagSlot.Lifecycle),
				ResourceInstance.ProcessTagSlot.RemainingTriggers,
				*ResourceInstance.ProcessingMemory.LastProcessArchetype.ToString(),
				static_cast<int32>(ResourceInstance.ProcessingMemory.LastTemperature),
				static_cast<int32>(ResourceInstance.ProcessingMemory.LastFamilyAction),
				*ResourceInstance.LogisticsMetadata.LastProcessedBodyId.ToString(),
				*ResourceInstance.LogisticsMetadata.LastTransitDestinationBodyId.ToString(),
				ResourceInstance.LogisticsMetadata.TransitCount);
			for (const FSRResourceTagStack& TagStack : ResourceInstance.Tags)
			{
				Signature += FString::Printf(
					TEXT(":T%d/%d/%d"),
					static_cast<int32>(TagStack.Tag),
					TagStack.StackCount,
					TagStack.RemainingCycles);
			}
		}
		return Signature;
	}

	FString BuildInventoryPanelSignature(const TCHAR* Label, const TArray<FSRFacilityPortInventory>& PortInventories, const FString& EmptyText)
	{
		FString Signature = FString::Printf(TEXT("%s:%d:%s"), Label, PortInventories.Num(), *EmptyText);
		for (const FSRFacilityPortInventory& PortInventory : PortInventories)
		{
			Signature += TEXT("|");
			Signature += BuildInventorySlotSignature(PortInventory);
		}
		return Signature;
	}

	void BuildNextInputPreviewResources(const FSRFacilityInstance& FacilityInstance, TArray<FSRResourceInstance>& OutPreviewInputs)
	{
		OutPreviewInputs.Reset();
		if (!FacilityInstance.ProcessingInventory.IsEmpty())
		{
			for (const FSRResourceInstance& ResourceInstance : FacilityInstance.ProcessingInventory)
			{
				if (ResourceInstance.ResourceId.IsNone() || ResourceInstance.StackCount <= 0)
				{
					continue;
				}

				FSRResourceInstance PreviewResource = ResourceInstance;
				PreviewResource.StackCount = 1;
				OutPreviewInputs.Add(PreviewResource);
			}
			return;
		}

		const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
		const int32 RequiredInputCount = IsValid(FacilityDataAsset)
			&& FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Process
			? FMath::Min(1, FacilityInstance.InputPortInventories.Num())
			: FacilityInstance.InputPortInventories.Num();
		for (int32 InputIndex = 0; InputIndex < RequiredInputCount; ++InputIndex)
		{
			const FSRFacilityPortInventory& PortInventory = FacilityInstance.InputPortInventories[InputIndex];
			FSRResourceInstance PreviewResource = StarRovers::FacilityResources::PeekSingleResourceFromInventorySlot(PortInventory);
			if (!PreviewResource.ResourceId.IsNone() && PreviewResource.StackCount > 0)
			{
				OutPreviewInputs.Add(PreviewResource);
			}
		}
	}

	struct FFacilityInspectorPreviewSnapshot
	{
		TArray<FSRResourceInstance> Inputs;
		TArray<FSRResourceInstance> Outputs;
		TArray<FString> EnergyFormulaTexts;
		bool bPreviewResolved = false;
		bool bOutputBlocked = false;
	};

	FFacilityInspectorPreviewSnapshot BuildFacilityInspectorPreviewSnapshot(
		USRFacilityNetworkComponent* FacilityNetwork,
		const FSRFacilityInstance& FacilityInstance,
		float ProgressRatio)
	{
		FFacilityInspectorPreviewSnapshot Snapshot;
		const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
		const bool bIsMiningFacility = IsValid(FacilityDataAsset)
			&& FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Mine;
		if (bIsMiningFacility)
		{
			FSRResourceDepositInstance MiningTarget;
			if (IsValid(FacilityNetwork)
				&& FacilityNetwork->GetFacilityMiningTarget(FacilityInstance.OccupantId, MiningTarget)
				&& IsValid(MiningTarget.ResourceDataAsset.Get()))
			{
				Snapshot.Inputs.Add(MiningTarget.ResourceDataAsset->BuildDefaultInstance());
			}
		}
		else
		{
			BuildNextInputPreviewResources(FacilityInstance, Snapshot.Inputs);
		}

		if (!IsValid(FacilityNetwork))
		{
			return Snapshot;
		}

		FSRResourceInstance PrimaryOutput;
		TArray<FSRResourceInstance> AdditionalOutputs;
		int32 PrimaryOutputCount = 0;
		Snapshot.bPreviewResolved = FacilityNetwork->GetFacilityOutputPreview(
			FacilityInstance.OccupantId,
			PrimaryOutput,
			AdditionalOutputs,
			PrimaryOutputCount,
			Snapshot.EnergyFormulaTexts);
		if (!Snapshot.bPreviewResolved)
		{
			return Snapshot;
		}

		for (int32 OutputIndex = 0; OutputIndex < FMath::Max(0, PrimaryOutputCount); ++OutputIndex)
		{
			Snapshot.Outputs.Add(PrimaryOutput);
		}
		Snapshot.Outputs.Append(AdditionalOutputs);

		if (!FacilityInstance.bProcessing
			|| ProgressRatio < 0.999f
			|| Snapshot.Outputs.IsEmpty())
		{
			return Snapshot;
		}

		TArray<FSRFacilityPortInventory> SimulatedOutputPorts = FacilityInstance.OutputPortInventories;
		for (const FSRResourceInstance& Output : Snapshot.Outputs)
		{
			const int32 RequiredStackCount = StarRovers::FacilityResources::GetResourceStackCount(Output);
			if (RequiredStackCount <= 0
				|| StarRovers::FacilityResources::TryAddResourceToInventorySlots(SimulatedOutputPorts, Output)
					!= RequiredStackCount)
			{
				Snapshot.bOutputBlocked = true;
				break;
			}
		}
		return Snapshot;
	}

	FString BuildConciseProcessRuleSummary(const FSRFacilityInstance& FacilityInstance)
	{
		const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
		if (!IsValid(FacilityDataAsset))
		{
			return TEXT("PROCESS RULE  |  Facility definition unavailable");
		}
		if (FSRStellarFuelFabricator::ShouldRouteThroughResourceV2(FacilityDataAsset))
		{
			return TEXT("5-CARD SYNTHESIS  |  Hand + Seals + Imprints\nFinal multiplication is applied here only");
		}
		if (FSROperationalEconomyProcessor::ShouldRouteThroughResourceV2(FacilityDataAsset))
		{
			return FacilityDataAsset->ResourceV2Synthesis.SynthesisRole
				== ESRFacilitySynthesisRoleV2::IndustrialSupplyFabricator
				? TEXT("SUPPLY SYNTHESIS  |  Common Ore + Biomass Feedstock\nProduces 2 Industrial Supply")
				: TEXT("SERVICE CORE  |  Industrial Supply -> Capacity\nConsumes supply; no resource output");
		}
		if (FSRFacilityResourceV2Processor::ShouldRouteStandardProcessThroughResourceV2(FacilityDataAsset))
		{
			const FSRFacilityProcessDefinitionV2& Definition = FacilityDataAsset->ResourceV2Process;
			const FString FamilyLabel = Definition.AcceptedFamily == ESRResourceFamily::None
				? TEXT("Any Family")
				: GetResourceV2FamilyLabel(Definition.AcceptedFamily);
			switch (Definition.ProcessRole)
			{
			case ESRFacilityProcessRoleV2::ApplyProcessTag:
				return FString::Printf(
					TEXT("TAG IMPRINT  |  %s\nRecipe: %s  |  Energy unchanged"),
					*FamilyLabel,
					*GetResourceV2RecipeDisplayName(
						FSRFacilityResourceV2Processor::ResolveProcessTagRecipeId(FacilityInstance)));
			case ESRFacilityProcessRoleV2::ApplyFuelImprint:
				return FString::Printf(
					TEXT("FUEL IMPRINT  |  %s\nRecipe: %s  |  Energy unchanged"),
					*FamilyLabel,
					*GetResourceV2RecipeDisplayName(
						FSRFacilityResourceV2Processor::ResolveFuelImprintRecipeId(FacilityInstance)));
			case ESRFacilityProcessRoleV2::ClearProcessTag:
				return FString::Printf(
					TEXT("TAG SCRUB  |  %s\nClears Process Tag  |  Energy unchanged"),
					*FamilyLabel);
			case ESRFacilityProcessRoleV2::FamilyProcess:
			default:
				return FString::Printf(
					TEXT("%s  |  %s / %s\nEnergy %+.1f additive  |  Cell temperature applies"),
					*Definition.ProcessArchetype.ToString(),
					*FamilyLabel,
					*StaticEnum<ESRResourceFamilyAction>()->GetDisplayNameTextByValue(
						static_cast<int64>(Definition.FamilyAction)).ToString(),
					Definition.FacilityEnergyDelta);
			}
		}

		const UEnum* OperationEnum = StaticEnum<ESRFacilityOperationKind>();
		const FString OperationLabel = OperationEnum
			? OperationEnum->GetDisplayNameTextByValue(static_cast<int64>(FacilityDataAsset->OperationKind)).ToString()
			: TEXT("Process");
		return FString::Printf(
			TEXT("%s  |  %d authored effect%s"),
			*OperationLabel,
			FacilityDataAsset->Effects.Num(),
			FacilityDataAsset->Effects.Num() == 1 ? TEXT("") : TEXT("s"));
	}

	FString BuildInspectorStateTransitionSummary(
		const TArray<FSRResourceInstance>& Inputs,
		const TArray<FSRResourceInstance>& Outputs,
		bool bUsesFinalFuelFormula)
	{
		if (bUsesFinalFuelFormula && !Inputs.IsEmpty() && !Outputs.IsEmpty())
		{
			return TEXT("STATE / TAG  |  Five Cards consumed; Seals and Imprints score at fabrication");
		}
		if (Inputs.IsEmpty() || Outputs.IsEmpty())
		{
			return TEXT("STATE / TAG  |  Awaiting valid input");
		}

		const FSRResourceInstance& Input = Inputs[0];
		const FSRResourceInstance& Output = Outputs[0];
		TArray<FString> Changes;
		const UEnum* StateEnum = StaticEnum<ESRResourceFamilyState>();
		for (int32 StateIndex = 0; StateIndex <= static_cast<int32>(ESRResourceFamilyState::Collapsed); ++StateIndex)
		{
			const bool bWasActive = (Input.ActiveFamilyStateFlags & (1 << StateIndex)) != 0;
			const bool bIsActive = (Output.ActiveFamilyStateFlags & (1 << StateIndex)) != 0;
			if (bWasActive == bIsActive)
			{
				continue;
			}
			const FString StateLabel = StateEnum
				? StateEnum->GetDisplayNameTextByValue(StateIndex).ToString()
				: FString::FromInt(StateIndex);
			Changes.Add(FString::Printf(TEXT("%s %s"), *StateLabel, bIsActive ? TEXT("ON") : TEXT("OFF")));
		}

		if (Input.ProcessTagSlot.TagId != Output.ProcessTagSlot.TagId
			|| Input.ProcessTagSlot.Lifecycle != Output.ProcessTagSlot.Lifecycle)
		{
			const FString OutputTag = Output.ProcessTagSlot.TagId.IsNone()
				? TEXT("Cleared")
				: Output.ProcessTagSlot.TagId.ToString();
			Changes.Add(FString::Printf(TEXT("Tag %s"), *OutputTag));
		}
		if (Input.FuelImprintSlot.ImprintId != Output.FuelImprintSlot.ImprintId)
		{
			const FString OutputImprint = Output.FuelImprintSlot.ImprintId.IsNone()
				? TEXT("Cleared")
				: Output.FuelImprintSlot.ImprintId.ToString();
			Changes.Add(FString::Printf(TEXT("Imprint %s"), *OutputImprint));
		}

		return Changes.IsEmpty()
			? TEXT("STATE / TAG  |  No change")
			: FString::Printf(TEXT("STATE / TAG  |  %s"), *FString::Join(Changes, TEXT("  /  ")));
	}

	FString BuildInlineResourceSummary(const FSRResourceInstance& ResourceInstance)
	{
		FString Summary = BuildResourceDisplayName(ResourceInstance);
		const FString EnergyText = FormatFacilityEnergyDisplayValue(ResolveDisplayedEnergy(ResourceInstance));
		Summary += UsesResourceV2Display()
			? FString::Printf(TEXT("  Energy: %s"), *EnergyText)
			: FString::Printf(TEXT("  Energy Total: %s"), *EnergyText);
		return Summary;
	}

	FString BuildInventorySummary(const TCHAR* Label, const TArray<FSRResourceInstance>& Inventory)
	{
		FString Summary = FString::Printf(TEXT("%s (%d)"), Label, Inventory.Num());
		for (int32 ResourceIndex = 0; ResourceIndex < Inventory.Num(); ++ResourceIndex)
		{
			Summary += FString::Printf(
				TEXT("\n%d. %s"),
				ResourceIndex + 1,
				*BuildInlineResourceSummary(Inventory[ResourceIndex]));
		}
		return Summary;
	}

	FString BuildPortInventorySummary(const TCHAR* Label, const TArray<FSRFacilityPortInventory>& PortInventories)
	{
		FString Summary = FString::Printf(TEXT("%s (%d ports)"), Label, PortInventories.Num());
		if (PortInventories.IsEmpty())
		{
			Summary += TEXT("\nNone");
			return Summary;
		}

		for (int32 PortIndex = 0; PortIndex < PortInventories.Num(); ++PortIndex)
		{
			const FSRFacilityPortInventory& PortInventory = PortInventories[PortIndex];
			Summary += FString::Printf(
				TEXT("\n%s (%d/%d)"),
				*PortInventory.PortId.ToString(),
				StarRovers::FacilityResources::GetInventorySlotStackCount(PortInventory),
				FMath::Max(1, PortInventory.Capacity));

			if (PortInventory.Inventory.IsEmpty())
			{
				Summary += TEXT("\n  Empty");
				continue;
			}

			for (int32 ResourceIndex = 0; ResourceIndex < PortInventory.Inventory.Num(); ++ResourceIndex)
			{
				Summary += FString::Printf(
					TEXT("\n  %d. %s"),
					ResourceIndex + 1,
					*BuildInlineResourceSummary(PortInventory.Inventory[ResourceIndex]));
			}
		}
		return Summary;
	}

	FString BuildMiningTargetSummary(USRFacilityNetworkComponent* FacilityNetwork, FName OccupantId)
	{
		if (!IsValid(FacilityNetwork) || OccupantId.IsNone())
		{
			return TEXT("Mining Target\nNo adjacent deposit");
		}

		FSRResourceDepositInstance ResourceDeposit;
		if (!FacilityNetwork->GetFacilityMiningTarget(OccupantId, ResourceDeposit))
		{
			FSRFacilityInstance FacilityInstance;
			USRStructureInstanceManagerComponent* StructureManager =
				IsValid(FacilityNetwork->GetOwner())
					? FacilityNetwork->GetOwner()->FindComponentByClass<USRStructureInstanceManagerComponent>()
					: nullptr;
			if (!FacilityNetwork->GetFacilityInstance(OccupantId, FacilityInstance)
				|| FacilityInstance.MiningTargetDepositOccupantId.IsNone()
				|| !IsValid(StructureManager)
				|| !StructureManager->GetResourceDepositInstance(
					FacilityInstance.MiningTargetDepositOccupantId,
					ResourceDeposit))
			{
				return TEXT("Mining Target\nNo adjacent deposit");
			}
		}

		const FString RemainingText = FSRResourceDepositAmountModel::IsInfinite(
			ResourceDeposit.TotalAmount)
			? TEXT("Infinite")
			: ResourceDeposit.RemainingAmount > 0
				? FString::Printf(
					TEXT("%d / %d"),
					ResourceDeposit.RemainingAmount,
					ResourceDeposit.TotalAmount)
				: FString::Printf(TEXT("DEPLETED / %d"), ResourceDeposit.TotalAmount);

		return FString::Printf(
			TEXT("Mining Target\nDeposit: %s\nResource: %s\nRemaining: %s"),
			ResourceDeposit.StructureId.IsNone() ? *ResourceDeposit.OccupantId.ToString() : *ResourceDeposit.StructureId.ToString(),
			*BuildResourceDataAssetDisplayName(ResourceDeposit.ResourceDataAsset.Get()),
			*RemainingText);
	}

	bool HasAvailableInputPortCapacity(const FSRFacilityInstance& FacilityInstance)
	{
		for (const FSRFacilityPortInventory& InputPortInventory : FacilityInstance.InputPortInventories)
		{
			if (StarRovers::FacilityResources::GetInventorySlotStackCount(InputPortInventory) < FMath::Max(1, InputPortInventory.Capacity))
			{
				return true;
			}
		}
		return false;
	}

	FString BuildResourceListSummary(const TCHAR* Label, const TArray<FSRResourceInstance>& ResourceInstances)
	{
		FString Summary = Label;
		if (ResourceInstances.IsEmpty())
		{
			Summary += TEXT("\nNone");
			return Summary;
		}

		const int32 VisibleCount = FMath::Min(ResourceInstances.Num(), 3);
		for (int32 ResourceIndex = 0; ResourceIndex < VisibleCount; ++ResourceIndex)
		{
			Summary += FString::Printf(
				TEXT("\n%d. %s"),
				ResourceIndex + 1,
				*BuildResourceSummary(ResourceInstances[ResourceIndex]));
		}
		if (ResourceInstances.Num() > VisibleCount)
		{
			Summary += FString::Printf(TEXT("\n... +%d"), ResourceInstances.Num() - VisibleCount);
		}
		return Summary;
	}

	FString BuildFacilityEffectSummary(const FSRFacilityEffectSpec& EffectSpec)
	{
		auto BuildConditionSummary = [](const FSRFacilityEffectConditionSpec& ConditionSpec)
		{
			switch (ConditionSpec.ConditionKind)
			{
			case ESRFacilityEffectConditionKind::EnergyAtLeast:
			case ESRFacilityEffectConditionKind::EnergyAtMost:
			case ESRFacilityEffectConditionKind::EnergyGreaterThan:
			case ESRFacilityEffectConditionKind::EnergyLessThan:
				return FString::Printf(
					TEXT("%s %s"),
					GetEffectConditionKindLabel(ConditionSpec.ConditionKind),
					*FormatFacilityEnergyDisplayValue(ConditionSpec.EnergyValue));
			case ESRFacilityEffectConditionKind::EnergyIncreased:
			case ESRFacilityEffectConditionKind::EnergyDecreased:
			case ESRFacilityEffectConditionKind::PrimeEnergy:
				return FString(GetEffectConditionKindLabel(ConditionSpec.ConditionKind));
			case ESRFacilityEffectConditionKind::Tag:
				if (ConditionSpec.TagTarget == ESRFacilityTagConditionTarget::AllTags)
				{
					if (ConditionSpec.TagMode == ESRFacilityTagConditionMode::StackCountAtLeast)
					{
						return FString::Printf(
							TEXT("%s %s %d"),
							GetTagConditionTargetLabel(ConditionSpec.TagTarget),
							GetTagConditionModeLabel(ConditionSpec.TagMode),
							FMath::Max(1, ConditionSpec.TagStackCount));
					}
					return FString::Printf(
						TEXT("%s %s"),
						GetTagConditionModeLabel(ConditionSpec.TagMode),
						GetTagConditionTargetLabel(ConditionSpec.TagTarget));
				}
				if (ConditionSpec.TagMode == ESRFacilityTagConditionMode::StackCountAtLeast)
				{
					return FString::Printf(
						TEXT("%s %s %d"),
						GetResourceProcessTagLabel(ConditionSpec.ResourceTag),
						GetTagConditionModeLabel(ConditionSpec.TagMode),
						FMath::Max(1, ConditionSpec.TagStackCount));
				}
				return FString::Printf(
					TEXT("%s %s"),
					GetTagConditionModeLabel(ConditionSpec.TagMode),
					GetResourceProcessTagLabel(ConditionSpec.ResourceTag));
			case ESRFacilityEffectConditionKind::TemperatureState:
				return FString::Printf(
					TEXT("%s %s"),
					GetEffectConditionKindLabel(ConditionSpec.ConditionKind),
					GetFacilityTemperatureLabel(ConditionSpec.TemperatureState));
			case ESRFacilityEffectConditionKind::ProcessCountEquals:
				return FString::Printf(
					TEXT("%s %d"),
					GetEffectConditionKindLabel(ConditionSpec.ConditionKind),
					FMath::Max(0, ConditionSpec.ProcessCount));
			default:
				return FString(GetEffectConditionKindLabel(ConditionSpec.ConditionKind));
			}
		};

		auto BuildConditionListSummary = [&BuildConditionSummary](
			const TArray<FSRFacilityEffectConditionSpec>& Conditions,
			ESRFacilityConditionLogic ConditionLogic)
		{
			FString Summary;
			const int32 VisibleConditionCount = FMath::Min(Conditions.Num(), 2);
			for (int32 ConditionIndex = 0; ConditionIndex < VisibleConditionCount; ++ConditionIndex)
			{
				if (ConditionIndex > 0)
				{
					Summary += FString::Printf(TEXT(" %s "), GetConditionLogicLabel(ConditionLogic));
				}
				Summary += BuildConditionSummary(Conditions[ConditionIndex]);
			}

			if (Conditions.Num() > VisibleConditionCount)
			{
				Summary += FString::Printf(TEXT(" +%d"), Conditions.Num() - VisibleConditionCount);
			}
			return Summary;
		};

		auto AppendConditions = [&EffectSpec, &BuildConditionListSummary](FString BaseSummary)
		{
			TArray<FString> RequiredConditionParts;
			if (!EffectSpec.Conditions.IsEmpty())
			{
				RequiredConditionParts.Add(BuildConditionListSummary(
					EffectSpec.Conditions,
					ESRFacilityConditionLogic::And));
			}

			TArray<FString> GroupParts;
			int32 ActiveGroupCount = 0;
			for (const FSRFacilityEffectConditionGroupSpec& ConditionGroup : EffectSpec.ConditionGroups)
			{
				if (ConditionGroup.Conditions.IsEmpty())
				{
					continue;
				}

				++ActiveGroupCount;
				if (GroupParts.Num() >= 2)
				{
					continue;
				}

				FString GroupSummary = BuildConditionListSummary(
					ConditionGroup.Conditions,
					ConditionGroup.ConditionLogic);
				if (ConditionGroup.Conditions.Num() > 1)
				{
					GroupSummary = FString::Printf(TEXT("(%s)"), *GroupSummary);
				}
				GroupParts.Add(GroupSummary);
			}

			if (!GroupParts.IsEmpty())
			{
				FString GroupSummary = FString::Join(GroupParts, *FString::Printf(
					TEXT(" %s "),
					GetConditionLogicLabel(EffectSpec.ConditionGroupLogic)));
				if (ActiveGroupCount > GroupParts.Num())
				{
					GroupSummary += FString::Printf(TEXT(" +%d"), ActiveGroupCount - GroupParts.Num());
				}
				RequiredConditionParts.Add(GroupSummary);
			}

			if (RequiredConditionParts.IsEmpty())
			{
				return BaseSummary;
			}

			BaseSummary += TEXT(" if ");
			BaseSummary += FString::Join(RequiredConditionParts, TEXT(" && "));
			return BaseSummary;
		};

		FString EffectSummary;
		switch (EffectSpec.EffectKind)
		{
		case ESRFacilityEffectKind::AdjustEnergy:
			if (EffectSpec.EnergyValueSource == ESRFacilityEnergyAdjustmentValueSource::FixedValue)
			{
				if (EffectSpec.EnergyAdjustmentMode == ESRFacilityEnergyAdjustmentMode::Multiply)
				{
					EffectSummary = FString::Printf(
						TEXT("%s * %s"),
						GetEffectKindLabel(EffectSpec.EffectKind),
						*FormatFacilityEnergyDisplayValue(EffectSpec.Value));
				}
				else if (EffectSpec.EnergyAdjustmentMode == ESRFacilityEnergyAdjustmentMode::Subtract)
				{
					EffectSummary = FString::Printf(
						TEXT("%s - %s"),
						GetEffectKindLabel(EffectSpec.EffectKind),
						*FormatFacilityEnergyDisplayValue(FMath::Abs(EffectSpec.Value)));
				}
				else
				{
					EffectSummary = FString::Printf(TEXT("%s %+.1f"), GetEffectKindLabel(EffectSpec.EffectKind), EffectSpec.Value);
				}
			}
			else if (EffectSpec.EnergyValueSource == ESRFacilityEnergyAdjustmentValueSource::TagStackCount)
			{
				const TCHAR* TagStackTargetLabel = EffectSpec.TagStackCountTarget == ESRFacilityTagStackCountTarget::All
					? GetTagStackCountTargetLabel(EffectSpec.TagStackCountTarget)
					: GetResourceProcessTagLabel(EffectSpec.ResourceTag);
				const FString MultiplierSummary = BuildEnergyValueMultiplierSummary(EffectSpec);
				EffectSummary = FString::Printf(
					TEXT("%s %s %s %s%s"),
					GetEffectKindLabel(EffectSpec.EffectKind),
					GetEnergyAdjustmentModeLabel(EffectSpec.EnergyAdjustmentMode),
					GetEnergyValueSourceLabel(EffectSpec.EnergyValueSource),
					TagStackTargetLabel,
					*MultiplierSummary);
			}
			else
			{
				const FString MultiplierSummary = BuildEnergyValueMultiplierSummary(EffectSpec);
				EffectSummary = FString::Printf(
					TEXT("%s %s %s%s"),
					GetEffectKindLabel(EffectSpec.EffectKind),
					GetEnergyAdjustmentModeLabel(EffectSpec.EnergyAdjustmentMode),
					GetEnergyValueSourceLabel(EffectSpec.EnergyValueSource),
					*MultiplierSummary);
			}
			break;
		case ESRFacilityEffectKind::AdjustProcessLimit:
			EffectSummary = FString::Printf(
				TEXT("%s %s"),
				GetEffectKindLabel(EffectSpec.EffectKind),
				*BuildProcessLimitAdjustmentSummary(EffectSpec));
			break;
		case ESRFacilityEffectKind::RemoveResource:
			EffectSummary = FString(GetEffectKindLabel(EffectSpec.EffectKind));
			break;
		case ESRFacilityEffectKind::AttachTag:
			if (EffectSpec.AttachTagSource == ESRFacilityAttachTagSource::MissingTags
				|| EffectSpec.AttachTagSource == ESRFacilityAttachTagSource::AttachedTags)
			{
				EffectSummary = FString::Printf(
					TEXT("%s %s"),
					GetEffectKindLabel(EffectSpec.EffectKind),
					GetAttachTagSourceLabel(EffectSpec.AttachTagSource));
			}
			else if (EffectSpec.AttachTagSource == ESRFacilityAttachTagSource::LastAttachedTag)
			{
				EffectSummary = FString::Printf(
					TEXT("%s %s x%d"),
					GetEffectKindLabel(EffectSpec.EffectKind),
					GetAttachTagSourceLabel(EffectSpec.AttachTagSource),
					FMath::Max(1, EffectSpec.Count));
			}
			else
			{
				EffectSummary = FString::Printf(
					TEXT("%s %s x%d"),
					GetEffectKindLabel(EffectSpec.EffectKind),
					GetResourceProcessTagLabel(EffectSpec.ResourceTag),
					FMath::Max(1, EffectSpec.Count));
			}
			break;
		case ESRFacilityEffectKind::ProduceWaste:
			EffectSummary = FString::Printf(
				TEXT("%s %s x%d"),
				GetEffectKindLabel(EffectSpec.EffectKind),
				IsValid(EffectSpec.ProducedResource.Get())
					? *EffectSpec.ProducedResource->ResourceId.ToString()
					: TEXT("None"),
				FMath::Max(1, EffectSpec.Count));
			break;
		case ESRFacilityEffectKind::TransferTagsToWaste:
			EffectSummary = FString::Printf(
				TEXT("%s %s -> %s"),
				GetEffectKindLabel(EffectSpec.EffectKind),
				EffectSpec.TagTarget == ESRFacilityEffectTagTarget::SpecificTag
					? GetResourceProcessTagLabel(EffectSpec.ResourceTag)
					: GetEffectTagTargetLabel(EffectSpec.TagTarget),
				IsValid(EffectSpec.ProducedResource.Get())
					? *EffectSpec.ProducedResource->ResourceId.ToString()
					: TEXT("None"));
			break;
		case ESRFacilityEffectKind::AdjustCellTemperature:
			EffectSummary = FString::Printf(
				TEXT("%s %+d R%d"),
				GetEffectKindLabel(EffectSpec.EffectKind),
				EffectSpec.TemperatureStepDelta,
				FMath::Max(1, EffectSpec.TileRange));
			break;
		case ESRFacilityEffectKind::InvertHeat:
		case ESRFacilityEffectKind::InvertTagEffects:
		case ESRFacilityEffectKind::DoubleTagEffects:
			EffectSummary = FString(GetEffectKindLabel(EffectSpec.EffectKind));
			break;
		case ESRFacilityEffectKind::DuplicateInputResource:
			EffectSummary = FString::Printf(
				TEXT("%s x%d"),
				GetEffectKindLabel(EffectSpec.EffectKind),
				FMath::Max(1, EffectSpec.Count));
			break;
		case ESRFacilityEffectKind::OverrideProcessTemperature:
			EffectSummary = FString::Printf(
				TEXT("%s %s"),
				GetEffectKindLabel(EffectSpec.EffectKind),
				GetFacilityTemperatureLabel(EffectSpec.ProcessTemperatureState));
			break;
		case ESRFacilityEffectKind::TriggerTagEffect:
			EffectSummary = EffectSpec.TagTarget == ESRFacilityEffectTagTarget::SpecificTag
				? FString::Printf(
					TEXT("%s %s"),
					GetEffectKindLabel(EffectSpec.EffectKind),
					GetResourceProcessTagLabel(EffectSpec.ResourceTag))
				: FString::Printf(
					TEXT("%s %s"),
					GetEffectKindLabel(EffectSpec.EffectKind),
					GetEffectTagTargetLabel(EffectSpec.TagTarget));
			break;
		case ESRFacilityEffectKind::AdjustProcessTime:
			if (EffectSpec.ProcessTimeValueSource == ESRFacilityProcessTimeAdjustmentValueSource::TagStackCount)
			{
				const TCHAR* TagStackTargetLabel = EffectSpec.TagStackCountTarget == ESRFacilityTagStackCountTarget::All
					? GetTagStackCountTargetLabel(EffectSpec.TagStackCountTarget)
					: GetResourceProcessTagLabel(EffectSpec.ResourceTag);
				EffectSummary = FString::Printf(
					TEXT("%s %s %s %s"),
					GetEffectKindLabel(EffectSpec.EffectKind),
					GetProcessTimeModeLabel(EffectSpec.ProcessTimeMode),
					GetProcessTimeValueSourceLabel(EffectSpec.ProcessTimeValueSource),
					TagStackTargetLabel);
			}
			else
			{
				EffectSummary = FString::Printf(
					TEXT("%s %s %.2f"),
					GetEffectKindLabel(EffectSpec.EffectKind),
					GetProcessTimeModeLabel(EffectSpec.ProcessTimeMode),
					EffectSpec.Value);
			}
			break;
		case ESRFacilityEffectKind::RemoveTag:
		{
			const FString AmountSummary = BuildRemoveTagAmountSummary(EffectSpec);
			EffectSummary = EffectSpec.TagTarget == ESRFacilityEffectTagTarget::SpecificTag
				? FString::Printf(
					TEXT("%s %s%s"),
					GetEffectKindLabel(EffectSpec.EffectKind),
					GetResourceProcessTagLabel(EffectSpec.ResourceTag),
					*AmountSummary)
				: FString::Printf(
					TEXT("%s %s%s"),
					GetEffectKindLabel(EffectSpec.EffectKind),
					GetEffectTagTargetLabel(EffectSpec.TagTarget),
					*AmountSummary);
			break;
		}
		case ESRFacilityEffectKind::ChangeResourceType:
			EffectSummary = FString::Printf(
				TEXT("%s %s"),
				GetEffectKindLabel(EffectSpec.EffectKind),
				IsValid(EffectSpec.TargetResource.Get())
					? *EffectSpec.TargetResource->ResourceId.ToString()
					: TEXT("None"));
			break;
		default:
			EffectSummary = FString(GetEffectKindLabel(EffectSpec.EffectKind));
			break;
		}
		return AppendConditions(EffectSummary);
	}

	FString BuildEffectsSummary(const FSRFacilityInstance& FacilityInstance, UWorld* World)
	{
		const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
		if (!IsValid(FacilityDataAsset))
		{
			return TEXT("Effects\nNone");
		}
		if (FSRStellarFuelFabricator::ShouldRouteThroughResourceV2(FacilityDataAsset))
		{
			FString FailureReason;
			if (!FSRStellarFuelFabricator::ValidateFacilityDefinition(FacilityDataAsset, FailureReason))
			{
				return FString::Printf(TEXT("Stellar Fuel Fabricator V2\nBlocked: %s"), *FailureReason);
			}

			const FSRStellarFuelFabricationRulesV2& Rules =
				FacilityDataAsset->ResourceV2Synthesis.StellarFuelRules;
			return FString::Printf(
				TEXT("Stellar Fuel Fabricator V2\n")
				TEXT("- Input: exactly five Resource V2 Cards\n")
				TEXT("- Final formula: A + B * C (multiplication occurs here only)\n")
				TEXT("- Full House: B %+.1f, C %+.1f\n")
				TEXT("- Twin Seal: B %+.1f each valid Card Key\n")
				TEXT("- Topology Seal: B %+.1f, one per batch\n")
				TEXT("- Prismatic Catalyst: C %+.1f, one per batch\n")
				TEXT("Legacy Effects inactive"),
				Rules.FullHouseBonus.B,
				Rules.FullHouseBonus.C,
				Rules.TwinSealEnergyB,
				Rules.TopologySealEnergyB,
				Rules.PrismaticCatalystC);
		}
		if (FSROperationalEconomyProcessor::ShouldRouteThroughResourceV2(FacilityDataAsset))
		{
			FString FailureReason;
			if (!FSROperationalEconomyProcessor::ValidateFacilityDefinition(FacilityDataAsset, FailureReason))
			{
				return FString::Printf(TEXT("Operational Economy V2\nBlocked: %s"), *FailureReason);
			}
			return FacilityDataAsset->ResourceV2Synthesis.SynthesisRole
				== ESRFacilitySynthesisRoleV2::IndustrialSupplyFabricator
				? TEXT("Supply Fabricator V2\n- 1 Common Ore + 1 Biomass Feedstock\n- Output: 2 Industrial Supply\n- Cycle: 30s | Operational Load: 4\nLegacy Effects inactive")
				: TEXT("Service Core V2\n- Consumes 1 Industrial Supply / 30s\n- Supplied: +18 Operational Capacity\n- Buffer: 4 units / 120s\nLegacy Effects inactive");
		}
		if (FSRFacilityResourceV2Processor::ShouldRouteStandardProcessThroughResourceV2(FacilityDataAsset))
		{
			FString FailureReason;
			if (!FSRFacilityResourceV2Processor::ValidateProcessDefinition(FacilityDataAsset, FailureReason))
			{
				return FString::Printf(TEXT("Resource V2 Process\nBlocked: %s"), *FailureReason);
			}
			if (const USRAugmentSubsystem* AugmentSubsystem =
				World ? World->GetSubsystem<USRAugmentSubsystem>() : nullptr)
			{
				if (!AugmentSubsystem->IsFacilityRecipeUnlockedV2(FacilityInstance, FailureReason))
				{
					return FString::Printf(TEXT("Resource V2 Recipe\nLocked: %s"), *FailureReason);
				}
			}

			const FSRFacilityProcessDefinitionV2& Definition = FacilityDataAsset->ResourceV2Process;
			const FString FamilyLabel = Definition.AcceptedFamily == ESRResourceFamily::None
				? TEXT("Any Card")
				: GetResourceV2FamilyLabel(Definition.AcceptedFamily);
			switch (Definition.ProcessRole)
			{
			case ESRFacilityProcessRoleV2::ApplyProcessTag:
			{
				const FName RecipeId =
					FSRFacilityResourceV2Processor::ResolveProcessTagRecipeId(FacilityInstance);
				return FString::Printf(
					TEXT("Resource V2 Tag Imprinter\n- Tag: %s\n- Family: %s\n- Energy: unchanged\n- Family/process/location history: unchanged\nLegacy Effects inactive"),
					*GetResourceV2RecipeDisplayName(RecipeId),
					*FamilyLabel);
			}
			case ESRFacilityProcessRoleV2::ApplyFuelImprint:
			{
				const FName RecipeId =
					FSRFacilityResourceV2Processor::ResolveFuelImprintRecipeId(FacilityInstance);
				return FString::Printf(
					TEXT("Resource V2 Fuel Imprinter\n- Imprint: %s\n- Family: %s\n- Energy: unchanged\n- Family/process/location history: unchanged\nLegacy Effects inactive"),
					*GetResourceV2RecipeDisplayName(RecipeId),
					*FamilyLabel);
			}
			case ESRFacilityProcessRoleV2::ClearProcessTag:
				return FString::Printf(
					TEXT("Resource V2 Tag Scrubber\n- Family: %s\n- Process Tag Slot: clear\n- Energy and Family State: unchanged\n- Process/location history: unchanged\nLegacy Effects inactive"),
					*FamilyLabel);
			case ESRFacilityProcessRoleV2::FamilyProcess:
			default:
			{
				const USRSimulationSettings* Settings = GetDefault<USRSimulationSettings>();
				const float ResistanceScale = IsValid(Settings)
					? Settings->RefinementResistanceEnergyScaleV2
					: 40.0f;
				const FString CycleRule = Definition.FamilyAction == ESRResourceFamilyAction::Anneal
					? TEXT("Recovery cycle; clears Metal Work Strain, Tempered, and Fatigued; Refinement Resistance ignored")
					: FMath::IsNearlyZero(Definition.FacilityEnergyDelta)
						? TEXT("Zero-Energy Family action; Refinement Resistance ignored")
						: FString::Printf(
						TEXT("Energy-changing cycle uses x(1 + max(0, Current - Seed) / %.1f)"),
						ResistanceScale);
				return FString::Printf(
					TEXT("Resource V2 Additive Process\n- Archetype: %s\n- Family: %s\n- Action: %s\n- Energy: %+.1f additive\n- Base Cycle: %.2fs\n- Timing: %s\n- Temperature: current Cell\nLegacy Effects inactive"),
					*Definition.ProcessArchetype.ToString(),
					*FamilyLabel,
					*StaticEnum<ESRResourceFamilyAction>()->GetDisplayNameTextByValue(
						static_cast<int64>(Definition.FamilyAction)).ToString(),
					Definition.FacilityEnergyDelta,
					FacilityDataAsset->BaseProcessSeconds,
					*CycleRule);
			}
			}
		}

		FString Summary = TEXT("Effects / Tags");
		if (FacilityDataAsset->Effects.IsEmpty())
		{
			Summary += TEXT("\nNone");
			return Summary;
		}

		const int32 VisibleCount = FMath::Min(FacilityDataAsset->Effects.Num(), 5);
		for (int32 EffectIndex = 0; EffectIndex < VisibleCount; ++EffectIndex)
		{
			const FSRFacilityEffectSpec& EffectSpec = FacilityDataAsset->Effects[EffectIndex];
			Summary += FString::Printf(
				TEXT("\n- %s"),
				*BuildFacilityEffectSummary(EffectSpec));
		}
		if (FacilityDataAsset->Effects.Num() > VisibleCount)
		{
			Summary += FString::Printf(TEXT("\n... +%d"), FacilityDataAsset->Effects.Num() - VisibleCount);
		}
		return Summary;
	}

	float ResolveProcessSeconds(const FSRFacilityInstance& FacilityInstance)
	{
		return FSRFacilityProcessingRuleEvaluator::ResolveProcessSeconds(FacilityInstance);
	}

	bool CanToggleProcess(const FSRFacilityInstance& FacilityInstance, UWorld* World, FString& OutReason)
	{
		const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
		if (!IsValid(FacilityDataAsset))
		{
			OutReason = TEXT("Invalid facility");
			return false;
		}
		const bool bUsesResourceV2Process =
			FSRFacilityResourceV2Processor::ShouldRouteStandardProcessThroughResourceV2(FacilityDataAsset);
		const bool bUsesStellarFuelFabricatorV2 =
			FSRStellarFuelFabricator::ShouldRouteThroughResourceV2(FacilityDataAsset);
		const bool bUsesOperationalEconomyV2 =
			FSROperationalEconomyProcessor::ShouldRouteThroughResourceV2(FacilityDataAsset);
		const bool bUsesResourceV2 = bUsesResourceV2Process
			|| bUsesStellarFuelFabricatorV2
			|| bUsesOperationalEconomyV2;
		if (bUsesResourceV2)
		{
			FString DefinitionFailure;
			const bool bDefinitionValid = bUsesStellarFuelFabricatorV2
				? FSRStellarFuelFabricator::ValidateFacilityDefinition(FacilityDataAsset, DefinitionFailure)
				: bUsesOperationalEconomyV2
					? FSROperationalEconomyProcessor::ValidateFacilityDefinition(FacilityDataAsset, DefinitionFailure)
					: FSRFacilityResourceV2Processor::ValidateProcessDefinition(FacilityDataAsset, DefinitionFailure);
			if (!bDefinitionValid)
			{
				OutReason = FString::Printf(TEXT("Blocked: %s"), *DefinitionFailure);
				return false;
			}
			if (bUsesResourceV2Process)
			{
				if (const USRAugmentSubsystem* AugmentSubsystem =
					World ? World->GetSubsystem<USRAugmentSubsystem>() : nullptr)
				{
					if (!AugmentSubsystem->IsFacilityRecipeUnlockedV2(FacilityInstance, DefinitionFailure))
					{
						OutReason = FString::Printf(TEXT("Locked: %s"), *DefinitionFailure);
						return false;
					}
				}
			}
		}

		const StarRovers::FacilityProcessing::FSRFacilityProcessContext ProcessContext =
			StarRovers::FacilityProcessing::ResolveProcessContext(FacilityInstance, FacilityInstance.ProcessingInventory);
		const ESRFacilityTemperatureState EffectiveTemperatureState = bUsesResourceV2
			? FacilityInstance.TemperatureState
			: ProcessContext.EffectiveTemperatureState;
		if (EffectiveTemperatureState == ESRFacilityTemperatureState::Frozen
			|| EffectiveTemperatureState == ESRFacilityTemperatureState::Overheated)
		{
			OutReason = FString::Printf(TEXT("Blocked by %s"), GetFacilityTemperatureLabel(EffectiveTemperatureState));
			return false;
		}

		if (EffectiveTemperatureState != FacilityInstance.TemperatureState)
		{
			OutReason = FString::Printf(TEXT("Ready as %s"), GetFacilityTemperatureLabel(EffectiveTemperatureState));
			return true;
		}
		if (bUsesResourceV2)
		{
			if (bUsesStellarFuelFabricatorV2)
			{
				OutReason = TEXT("Ready: five-Card Stellar Fuel fabrication");
				return true;
			}
			if (bUsesOperationalEconomyV2)
			{
				OutReason = FacilityDataAsset->ResourceV2Synthesis.SynthesisRole
					== ESRFacilitySynthesisRoleV2::ServiceCore
					? TEXT("Ready: supplied Service Core")
					: TEXT("Ready: Industrial Supply fabrication");
				return true;
			}
			OutReason = FString::Printf(
				TEXT("Ready: %s (%s)"),
				*FacilityDataAsset->ResourceV2Process.ProcessArchetype.ToString(),
				GetFacilityTemperatureLabel(EffectiveTemperatureState));
			return true;
		}

		OutReason = TEXT("Ready");
		return true;
	}

	UTextBlock* ConstructTextBlock(UWidgetTree* WidgetTree, const FName& Name, int32 FontSize, const FLinearColor& Color)
	{
		UTextBlock* TextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		FSlateFontInfo FontInfo = TextBlock->GetFont();
		FontInfo.Size = FontSize;
		TextBlock->SetFont(FontInfo);
		TextBlock->SetColorAndOpacity(FSlateColor(Color));
		TextBlock->SetAutoWrapText(true);
		return TextBlock;
	}

	void AddWidgetToCanvas(UCanvasPanel* CanvasPanel, UWidget* Widget, const FVector2D& Position, const FVector2D& Size)
	{
		if (!CanvasPanel || !Widget)
		{
			return;
		}

		if (UCanvasPanelSlot* CanvasSlot = CanvasPanel->AddChildToCanvas(Widget))
		{
			CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
			CanvasSlot->SetAlignment(FVector2D::ZeroVector);
			CanvasSlot->SetPosition(Position);
			CanvasSlot->SetSize(Size);
		}
	}

	UBorder* ConstructSectionBorder(
		UWidgetTree* WidgetTree,
		const FName& Name,
		UWidget* Content,
		const FLinearColor& Color = FLinearColor(0.075f, 0.095f, 0.115f, 0.96f),
		const FMargin& Padding = FMargin(10.0f))
	{
		UBorder* Border = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
		Border->SetBrushColor(Color);
		Border->SetPadding(Padding);
		if (Content)
		{
			Border->SetContent(Content);
		}
		return Border;
	}

	void AddResourceSlotCard(
		UWidgetTree* WidgetTree,
		UVerticalBox* SlotBox,
		const FString& Text,
		const FLinearColor& TextColor,
		const FLinearColor& CardColor)
	{
		if (!WidgetTree || !SlotBox)
		{
			return;
		}

		UTextBlock* SlotTextBlock = ConstructTextBlock(WidgetTree, NAME_None, 11, TextColor);
		SlotTextBlock->SetText(FText::FromString(Text));
		SlotTextBlock->SetJustification(ETextJustify::Left);

		UBorder* SlotBorder = ConstructSectionBorder(WidgetTree, NAME_None, SlotTextBlock, CardColor, FMargin(7.0f, 5.0f));
		if (UVerticalBoxSlot* Slot = SlotBox->AddChildToVerticalBox(SlotBorder))
		{
			Slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 5.0f));
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
	}

	void AddInventoryCardText(
		UWidgetTree* WidgetTree,
		UCanvasPanel* CardCanvas,
		const FString& Text,
		const FVector2D& Position,
		const FVector2D& Size,
		int32 FontSize,
		const FLinearColor& Color,
		ETextJustify::Type Justification = ETextJustify::Left)
	{
		if (!WidgetTree || !CardCanvas)
		{
			return;
		}

		UTextBlock* TextBlock = ConstructTextBlock(WidgetTree, NAME_None, FontSize, Color);
		TextBlock->SetText(FText::FromString(Text));
		TextBlock->SetJustification(Justification);
		TextBlock->SetAutoWrapText(false);
		AddWidgetToCanvas(CardCanvas, TextBlock, Position, Size);
	}

	UHorizontalBox* FindOrAddInventoryGridRow(
		UWidgetTree* WidgetTree,
		UVerticalBox* GridBox,
		int32 SlotIndex)
	{
		if (!WidgetTree || !GridBox)
		{
			return nullptr;
		}

		const int32 SafeSlotIndex = FMath::Max(0, SlotIndex);
		const int32 RowIndex = SafeSlotIndex / FacilityInventoryGridColumnCount;
		while (GridBox->GetChildrenCount() <= RowIndex)
		{
			UHorizontalBox* NewRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), NAME_None);
			if (UVerticalBoxSlot* RowSlot = GridBox->AddChildToVerticalBox(NewRow))
			{
				RowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
				RowSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			}
		}

		return Cast<UHorizontalBox>(GridBox->GetChildAt(RowIndex));
	}

	void AddWidgetToInventoryGrid(
		UWidgetTree* WidgetTree,
		UVerticalBox* GridBox,
		UWidget* Widget,
		int32 SlotIndex)
	{
		UHorizontalBox* Row = FindOrAddInventoryGridRow(WidgetTree, GridBox, SlotIndex);
		if (!Row || !Widget)
		{
			return;
		}

		const int32 ColumnIndex = FMath::Max(0, SlotIndex) % FacilityInventoryGridColumnCount;
		if (UHorizontalBoxSlot* Slot = Row->AddChildToHorizontalBox(Widget))
		{
			Slot->SetPadding(FMargin(0.0f, 0.0f, ColumnIndex < FacilityInventoryGridColumnCount - 1 ? 6.0f : 0.0f, 0.0f));
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}
	}

	void AddInventorySlotCard(
		UWidgetTree* WidgetTree,
		UVerticalBox* SlotBox,
		const FSRFacilityPortInventory& PortInventory,
		int32 SlotIndex,
		const TCHAR* FallbackLabel,
		const FLinearColor& AccentColor)
	{
		if (!WidgetTree || !SlotBox)
		{
			return;
		}

		const int32 Capacity = FMath::Max(1, PortInventory.Capacity);
		const int32 SlotStackCount = StarRovers::FacilityResources::GetInventorySlotStackCount(PortInventory);
		const FSRResourceInstance* ResourceInstance = PortInventory.Inventory.IsEmpty()
			? nullptr
			: &PortInventory.Inventory[0];
		const FSRResourceGlyphPresentation GlyphPresentation = ResourceInstance
			? FSRResourceGlyphPresentationBuilder::Build(*ResourceInstance)
			: FSRResourceGlyphPresentation();
		const bool bHasResource = GlyphPresentation.bHasResource && SlotStackCount > 0;

		UVerticalBox* CardBox = WidgetTree->ConstructWidget<UVerticalBox>(
			UVerticalBox::StaticClass(),
			NAME_None);
		UTextBlock* HeaderTextBlock = ConstructTextBlock(
			WidgetTree,
			NAME_None,
			8,
			AccentColor);
		HeaderTextBlock->SetText(FText::FromString(FString::Printf(
			TEXT("%s  %d/%d"),
			*BuildInventoryCardPortLabel(PortInventory, SlotIndex, FallbackLabel),
			SlotStackCount,
			Capacity)));
		HeaderTextBlock->SetAutoWrapText(false);
		if (UVerticalBoxSlot* HeaderSlot = CardBox->AddChildToVerticalBox(HeaderTextBlock))
		{
			HeaderSlot->SetPadding(FMargin(2.0f, 0.0f, 2.0f, 4.0f));
			HeaderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}

		USRResourceGlyphWidget* ResourceGlyph =
			WidgetTree->ConstructWidget<USRResourceGlyphWidget>(
				USRResourceGlyphWidget::StaticClass(),
				NAME_None);
		ResourceGlyph->SetGlyphMode(ESRResourceGlyphMode::Detailed);
		ResourceGlyph->SetPresentation(bHasResource
			? GlyphPresentation
			: FSRResourceGlyphPresentation());
		ResourceGlyph->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UVerticalBoxSlot* GlyphSlot = CardBox->AddChildToVerticalBox(ResourceGlyph))
		{
			GlyphSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			GlyphSlot->SetVerticalAlignment(VAlign_Center);
		}

		USizeBox* CardSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		CardSizeBox->SetWidthOverride(176.0f);
		CardSizeBox->SetHeightOverride(94.0f);
		CardSizeBox->AddChild(CardBox);

		const ESRUIVisualState CardState = bHasResource
			? ESRUIVisualState::Info
			: ESRUIVisualState::Disabled;
		UBorder* InnerBorder = ConstructSectionBorder(
			WidgetTree,
			NAME_None,
			CardSizeBox,
			USRUIThemeLibrary::ResolveStatePalette(CardState).SurfaceColor,
			FMargin(5.0f, 4.0f));
		UBorder* OuterBorder = ConstructSectionBorder(
			WidgetTree,
			NAME_None,
			InnerBorder,
			bHasResource ? GlyphPresentation.FamilyAccentColor : AccentColor,
			FMargin(2.0f));
		if (bHasResource)
		{
			OuterBorder->SetToolTipText(GlyphPresentation.ToolTipText);
		}
		AddWidgetToInventoryGrid(WidgetTree, SlotBox, OuterBorder, SlotIndex);
	}

	void AddPreviewResourceCard(
		UWidgetTree* WidgetTree,
		UHorizontalBox* SlotBox,
		const FSRResourceInstance& ResourceInstance,
		const FLinearColor& AccentColor)
	{
		if (!WidgetTree || !SlotBox)
		{
			return;
		}

		const FSRResourceGlyphPresentation GlyphPresentation =
			FSRResourceGlyphPresentationBuilder::Build(ResourceInstance);
		const bool bHasResource = GlyphPresentation.bHasResource;
		USRResourceGlyphWidget* ResourceGlyph =
			WidgetTree->ConstructWidget<USRResourceGlyphWidget>(
				USRResourceGlyphWidget::StaticClass(),
				NAME_None);
		ResourceGlyph->SetGlyphMode(ESRResourceGlyphMode::Detailed);
		ResourceGlyph->SetPresentation(GlyphPresentation);
		ResourceGlyph->SetVisibility(ESlateVisibility::HitTestInvisible);

		USizeBox* CardSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		CardSizeBox->SetWidthOverride(206.0f);
		CardSizeBox->SetHeightOverride(76.0f);
		CardSizeBox->AddChild(ResourceGlyph);

		UBorder* OuterBorder = ConstructSectionBorder(
			WidgetTree,
			NAME_None,
			CardSizeBox,
			bHasResource ? GlyphPresentation.FamilyAccentColor : AccentColor,
			FMargin(2.0f));
		if (bHasResource)
		{
			OuterBorder->SetToolTipText(GlyphPresentation.ToolTipText);
		}
		if (UHorizontalBoxSlot* Slot = SlotBox->AddChildToHorizontalBox(OuterBorder))
		{
			Slot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}
	}

	void AddEnergyFormulaCard(
		UWidgetTree* WidgetTree,
		UHorizontalBox* SlotBox,
		const FString& FormulaText,
		const FLinearColor& AccentColor)
	{
		if (!WidgetTree || !SlotBox || FormulaText.IsEmpty())
		{
			return;
		}

		UCanvasPanel* CardCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		AddInventoryCardText(WidgetTree, CardCanvas, TEXT("Energy Formula"), FVector2D(7.0f, 5.0f), FVector2D(266.0f, 18.0f), 9, AccentColor);

		UTextBlock* FormulaTextBlock = ConstructTextBlock(WidgetTree, NAME_None, 9, FLinearColor(0.90f, 0.94f, 0.96f, 1.0f));
		FormulaTextBlock->SetText(FText::FromString(FormulaText));
		FormulaTextBlock->SetAutoWrapText(true);
		FormulaTextBlock->SetJustification(ETextJustify::Left);

		UScrollBox* FormulaScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
		FormulaScrollBox->SetOrientation(Orient_Vertical);
		FormulaScrollBox->AddChild(FormulaTextBlock);
		AddWidgetToCanvas(CardCanvas, FormulaScrollBox, FVector2D(8.0f, 28.0f), FVector2D(264.0f, 112.0f));

		USizeBox* CardSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		CardSizeBox->SetWidthOverride(280.0f);
		CardSizeBox->SetHeightOverride(150.0f);
		CardSizeBox->AddChild(CardCanvas);

		UBorder* InnerBorder = ConstructSectionBorder(WidgetTree, NAME_None, CardSizeBox, FLinearColor(0.125f, 0.160f, 0.180f, 0.98f), FMargin(0.0f));
		UBorder* OuterBorder = ConstructSectionBorder(WidgetTree, NAME_None, InnerBorder, FLinearColor(0.005f, 0.006f, 0.007f, 1.0f), FMargin(3.0f));
		if (UHorizontalBoxSlot* Slot = SlotBox->AddChildToHorizontalBox(OuterBorder))
		{
			Slot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}
	}

	void AddInventoryInfoCard(
		UWidgetTree* WidgetTree,
		UHorizontalBox* SlotBox,
		const FString& Text,
		const FLinearColor& AccentColor)
	{
		if (!WidgetTree || !SlotBox)
		{
			return;
		}

		UCanvasPanel* CardCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		AddInventoryCardText(WidgetTree, CardCanvas, TEXT("Info"), FVector2D(7.0f, 5.0f), FVector2D(98.0f, 18.0f), 9, AccentColor);

		UTextBlock* CenterTextBlock = ConstructTextBlock(WidgetTree, NAME_None, 11, FLinearColor(0.90f, 0.94f, 0.96f, 1.0f));
		CenterTextBlock->SetText(FText::FromString(Text));
		CenterTextBlock->SetJustification(ETextJustify::Center);
		AddWidgetToCanvas(CardCanvas, CenterTextBlock, FVector2D(8.0f, 30.0f), FVector2D(96.0f, 46.0f));

		USizeBox* CardSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		CardSizeBox->SetWidthOverride(112.0f);
		CardSizeBox->SetHeightOverride(90.0f);
		CardSizeBox->AddChild(CardCanvas);

		UBorder* InnerBorder = ConstructSectionBorder(WidgetTree, NAME_None, CardSizeBox, FLinearColor(0.145f, 0.170f, 0.190f, 0.98f), FMargin(0.0f));
		UBorder* OuterBorder = ConstructSectionBorder(WidgetTree, NAME_None, InnerBorder, FLinearColor(0.005f, 0.006f, 0.007f, 1.0f), FMargin(3.0f));
		if (UHorizontalBoxSlot* Slot = SlotBox->AddChildToHorizontalBox(OuterBorder))
		{
			Slot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}
	}

	void AddInventoryInfoCard(
		UWidgetTree* WidgetTree,
		UVerticalBox* SlotBox,
		const FString& Text,
		const FLinearColor& AccentColor)
	{
		if (!WidgetTree || !SlotBox)
		{
			return;
		}

		UCanvasPanel* CardCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		AddInventoryCardText(WidgetTree, CardCanvas, TEXT("Info"), FVector2D(5.0f, 4.0f), FVector2D(74.0f, 16.0f), 8, AccentColor);

		UTextBlock* CenterTextBlock = ConstructTextBlock(WidgetTree, NAME_None, 11, FLinearColor(0.90f, 0.94f, 0.96f, 1.0f));
		CenterTextBlock->SetText(FText::FromString(Text));
		CenterTextBlock->SetJustification(ETextJustify::Center);
		AddWidgetToCanvas(CardCanvas, CenterTextBlock, FVector2D(5.0f, 27.0f), FVector2D(74.0f, 40.0f));

		USizeBox* CardSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		CardSizeBox->SetWidthOverride(84.0f);
		CardSizeBox->SetHeightOverride(78.0f);
		CardSizeBox->AddChild(CardCanvas);

		UBorder* InnerBorder = ConstructSectionBorder(WidgetTree, NAME_None, CardSizeBox, FLinearColor(0.145f, 0.170f, 0.190f, 0.98f), FMargin(0.0f));
		UBorder* OuterBorder = ConstructSectionBorder(WidgetTree, NAME_None, InnerBorder, FLinearColor(0.005f, 0.006f, 0.007f, 1.0f), FMargin(3.0f));
		AddWidgetToInventoryGrid(WidgetTree, SlotBox, OuterBorder, 0);
	}

	UButton* ConstructDebugInputButton(UWidgetTree* WidgetTree, const FName& ButtonName, const FText& Label)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), ButtonName);
		Button->SetBackgroundColor(FLinearColor(0.16f, 0.22f, 0.28f, 0.95f));

		UTextBlock* LabelTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		LabelTextBlock->SetText(Label);
		LabelTextBlock->SetJustification(ETextJustify::Center);
		LabelTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		FSlateFontInfo FontInfo = LabelTextBlock->GetFont();
		FontInfo.Size = 12;
		LabelTextBlock->SetFont(FontInfo);
		Button->AddChild(LabelTextBlock);
		return Button;
	}

	bool IsWidgetUnderScreenPosition(const UWidget* Widget, const FVector2D& ScreenPosition)
	{
		return IsValid(Widget)
			&& Widget->IsVisible()
			&& Widget->GetCachedGeometry().IsUnderLocation(ScreenPosition);
	}

	void ClearHubRouteButtonsAndActions(
		UHorizontalBox* ButtonBox,
		TArray<TObjectPtr<USRHubRouteDestinationAction>>& DestinationActions,
		TArray<TObjectPtr<USRHubRouteLaunchAction>>& LaunchActions,
		TArray<TObjectPtr<USRHubRouteRemovalAction>>& RemovalActions,
		TArray<TObjectPtr<USRHubRouteDebugOrbitAction>>& DebugOrbitActions,
		TArray<TObjectPtr<USRHubStarFuelMissileLaunchAction>>& MissileLaunchActions,
		TArray<TObjectPtr<USRHubRouteSettingAction>>& SettingActions)
	{
		if (ButtonBox)
		{
			ButtonBox->ClearChildren();
		}

		DestinationActions.Reset();
		LaunchActions.Reset();
		RemovalActions.Reset();
		DebugOrbitActions.Reset();
		MissileLaunchActions.Reset();
		SettingActions.Reset();
	}

	void BindInputSlotDebugButton(
		UButton* Button,
		USRFacilityControlWidget* OwnerWidget,
		TArray<TObjectPtr<USRFacilityInputSlotDebugAction>>& OutActions,
		int32 InputPortIndex,
		FName ResourceId)
	{
		if (!Button || !OwnerWidget || InputPortIndex == INDEX_NONE || ResourceId.IsNone())
		{
			return;
		}

		USRFacilityInputSlotDebugAction* Action = NewObject<USRFacilityInputSlotDebugAction>(OwnerWidget);
		Action->Initialize(OwnerWidget, InputPortIndex, ResourceId, Button);
		OutActions.Add(Action);
		Button->OnClicked.AddDynamic(Action, &USRFacilityInputSlotDebugAction::HandleClicked);
	}

	void AddHubRouteLaunchButton(
		UWidgetTree* WidgetTree,
		UHorizontalBox* ButtonBox,
		USRFacilityControlWidget* OwnerWidget,
		TArray<TObjectPtr<USRHubRouteLaunchAction>>& OutActions,
		const FSRSpaceLogisticsHubEndpoint& DestinationHub,
		bool bEnabled)
	{
		if (!WidgetTree || !ButtonBox || !OwnerWidget)
		{
			return;
		}

		UButton* Button = ConstructDebugInputButton(
			WidgetTree,
			NAME_None,
			NSLOCTEXT("StarRoversFacilityControl", "HubRouteLaunchButton", "Create\nRoute"));
		Button->SetBackgroundColor(bEnabled
			? FLinearColor(0.070f, 0.180f, 0.115f, 0.95f)
			: FLinearColor(0.060f, 0.066f, 0.072f, 0.95f));
		Button->SetIsEnabled(bEnabled);

		USRHubRouteLaunchAction* Action = NewObject<USRHubRouteLaunchAction>(OwnerWidget);
		Action->Initialize(OwnerWidget, DestinationHub, Button);
		OutActions.Add(Action);
		Button->OnClicked.AddDynamic(Action, &USRHubRouteLaunchAction::HandleClicked);

		USizeBox* ButtonSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), NAME_None);
		ButtonSizeBox->SetWidthOverride(104.0f);
		ButtonSizeBox->SetHeightOverride(64.0f);
		ButtonSizeBox->AddChild(Button);

		if (UHorizontalBoxSlot* ButtonSlot = ButtonBox->AddChildToHorizontalBox(ButtonSizeBox))
		{
			ButtonSlot->SetPadding(FMargin(0.0f, 0.0f, 12.0f, 0.0f));
			ButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}
	}

	void AddHubRouteRemoveButton(
		UWidgetTree* WidgetTree,
		UHorizontalBox* ButtonBox,
		USRFacilityControlWidget* OwnerWidget,
		TArray<TObjectPtr<USRHubRouteRemovalAction>>& OutActions,
		FName RouteId)
	{
		if (!WidgetTree || !ButtonBox || !OwnerWidget || RouteId.IsNone())
		{
			return;
		}

		UButton* Button = ConstructDebugInputButton(
			WidgetTree,
			NAME_None,
			NSLOCTEXT("StarRoversFacilityControl", "HubRouteRemoveButton", "Remove\nRoute"));
		Button->SetBackgroundColor(FLinearColor(0.240f, 0.075f, 0.070f, 0.95f));

		USRHubRouteRemovalAction* Action = NewObject<USRHubRouteRemovalAction>(OwnerWidget);
		Action->Initialize(OwnerWidget, RouteId, Button);
		OutActions.Add(Action);
		Button->OnClicked.AddDynamic(Action, &USRHubRouteRemovalAction::HandleClicked);

		USizeBox* ButtonSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), NAME_None);
		ButtonSizeBox->SetWidthOverride(88.0f);
		ButtonSizeBox->SetHeightOverride(54.0f);
		ButtonSizeBox->AddChild(Button);

		if (UHorizontalBoxSlot* ButtonSlot = ButtonBox->AddChildToHorizontalBox(ButtonSizeBox))
		{
			ButtonSlot->SetPadding(FMargin(0.0f, 0.0f, 12.0f, 0.0f));
			ButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}
	}

	void AddHubRouteDebugOrbitButton(
		UWidgetTree* WidgetTree,
		UHorizontalBox* ButtonBox,
		USRFacilityControlWidget* OwnerWidget,
		TArray<TObjectPtr<USRHubRouteDebugOrbitAction>>& OutActions,
		bool bEnabled)
	{
		if (!WidgetTree || !ButtonBox || !OwnerWidget)
		{
			return;
		}

		UButton* Button = ConstructDebugInputButton(
			WidgetTree,
			NAME_None,
			NSLOCTEXT("StarRoversFacilityControl", "HubRouteDebugOrbitButton", "Debug\nOrbit"));
		Button->SetBackgroundColor(bEnabled
			? FLinearColor(0.120f, 0.090f, 0.190f, 0.95f)
			: FLinearColor(0.060f, 0.066f, 0.072f, 0.95f));
		Button->SetIsEnabled(bEnabled);

		USRHubRouteDebugOrbitAction* Action = NewObject<USRHubRouteDebugOrbitAction>(OwnerWidget);
		Action->Initialize(OwnerWidget, Button);
		OutActions.Add(Action);
		Button->OnClicked.AddDynamic(Action, &USRHubRouteDebugOrbitAction::HandleClicked);

		USizeBox* ButtonSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), NAME_None);
		ButtonSizeBox->SetWidthOverride(104.0f);
		ButtonSizeBox->SetHeightOverride(54.0f);
		ButtonSizeBox->AddChild(Button);

		if (UHorizontalBoxSlot* ButtonSlot = ButtonBox->AddChildToHorizontalBox(ButtonSizeBox))
		{
			ButtonSlot->SetPadding(FMargin(0.0f, 0.0f, 10.0f, 0.0f));
			ButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}
	}

	void AddHubStarFuelMissileLaunchButton(
		UWidgetTree* WidgetTree,
		UHorizontalBox* ButtonBox,
		USRFacilityControlWidget* OwnerWidget,
		TArray<TObjectPtr<USRHubStarFuelMissileLaunchAction>>& OutActions,
		bool bEnabled)
	{
		if (!WidgetTree || !ButtonBox || !OwnerWidget)
		{
			return;
		}

		UButton* Button = ConstructDebugInputButton(
			WidgetTree,
			NAME_None,
			NSLOCTEXT("StarRoversFacilityControl", "HubStarFuelMissileLaunchButton", "Missile\nAuto Slots"));
		Button->SetBackgroundColor(bEnabled
			? FLinearColor(0.185f, 0.095f, 0.070f, 0.95f)
			: FLinearColor(0.060f, 0.066f, 0.072f, 0.95f));
		Button->SetIsEnabled(bEnabled);
		Button->SetToolTipText(NSLOCTEXT(
			"StarRoversFacilityControl",
			"HubStarFuelMissileAutoLaunchTooltip",
			"Select an input slot whose positive-Energy Stellar Fuel should launch automatically."));

		USRHubStarFuelMissileLaunchAction* Action = NewObject<USRHubStarFuelMissileLaunchAction>(OwnerWidget);
		Action->Initialize(OwnerWidget, Button);
		OutActions.Add(Action);
		Button->OnClicked.AddDynamic(Action, &USRHubStarFuelMissileLaunchAction::HandleClicked);

		USizeBox* ButtonSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), NAME_None);
		ButtonSizeBox->SetWidthOverride(112.0f);
		ButtonSizeBox->SetHeightOverride(54.0f);
		ButtonSizeBox->AddChild(Button);

		if (UHorizontalBoxSlot* ButtonSlot = ButtonBox->AddChildToHorizontalBox(ButtonSizeBox))
		{
			ButtonSlot->SetPadding(FMargin(0.0f, 0.0f, 12.0f, 0.0f));
			ButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}
	}

	void AddHubRouteMaxCargoStackCountButton(
		UWidgetTree* WidgetTree,
		UHorizontalBox* ButtonBox,
		USRFacilityControlWidget* OwnerWidget,
		TArray<TObjectPtr<USRHubRouteSettingAction>>& OutActions,
		FName RouteId,
		int32 NewMaxCargoStackCount)
	{
		if (!WidgetTree || !ButtonBox || !OwnerWidget || RouteId.IsNone())
		{
			return;
		}

		UButton* Button = ConstructDebugInputButton(
			WidgetTree,
			NAME_None,
			FText::FromString(FString::Printf(TEXT("Stack Limit\nx%d"), FMath::Max(1, NewMaxCargoStackCount))));
		Button->SetBackgroundColor(FLinearColor(0.080f, 0.130f, 0.165f, 0.95f));

		USRHubRouteSettingAction* Action = NewObject<USRHubRouteSettingAction>(OwnerWidget);
		Action->InitializeMaxCargoStackCount(OwnerWidget, RouteId, NewMaxCargoStackCount, Button);
		OutActions.Add(Action);
		Button->OnClicked.AddDynamic(Action, &USRHubRouteSettingAction::HandleClicked);

		USizeBox* ButtonSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), NAME_None);
		ButtonSizeBox->SetWidthOverride(74.0f);
		ButtonSizeBox->SetHeightOverride(54.0f);
		ButtonSizeBox->AddChild(Button);

		if (UHorizontalBoxSlot* ButtonSlot = ButtonBox->AddChildToHorizontalBox(ButtonSizeBox))
		{
			ButtonSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
			ButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}
	}

	void AddHubRouteProfileButton(
		UWidgetTree* WidgetTree,
		UHorizontalBox* ButtonBox,
		USRFacilityControlWidget* OwnerWidget,
		TArray<TObjectPtr<USRHubRouteSettingAction>>& OutActions,
		FName RouteId,
		ESRSpaceLogisticsRouteProfileV2 NewRouteProfile,
		bool bEnabled)
	{
		if (!WidgetTree || !ButtonBox || !OwnerWidget || RouteId.IsNone())
		{
			return;
		}

		const FSRSpaceLogisticsRouteProfileRulesV2 Rules =
			FSRFleetCapacityV2::GetRouteProfileRules(NewRouteProfile);
		UButton* Button = ConstructDebugInputButton(
			WidgetTree,
			NAME_None,
			FText::FromString(FString::Printf(
				TEXT("Profile\n%s"),
				*GetHubRouteProfileShortLabel(Rules.Profile))));
		Button->SetBackgroundColor(bEnabled
			? FLinearColor(0.075f, 0.145f, 0.175f, 0.95f)
			: FLinearColor(0.060f, 0.066f, 0.072f, 0.95f));
		Button->SetIsEnabled(bEnabled);
		Button->SetToolTipText(FText::FromString(FString::Printf(
			TEXT("Set %s: %s. Cargo %d, Fleet Load %d (%.1f cargo/load). Profiles can only change while docked with no onboard cargo."),
			*Rules.DisplayName.ToString(),
			*Rules.CargoContractText.ToString(),
			Rules.CargoCapacity,
			Rules.FleetLoad,
			FSRFleetCapacityV2::ResolveMaximumCargoPerFleetLoad(Rules.Profile))));

		USRHubRouteSettingAction* Action = NewObject<USRHubRouteSettingAction>(OwnerWidget);
		Action->InitializeRouteProfile(OwnerWidget, RouteId, Rules.Profile, Button);
		OutActions.Add(Action);
		Button->OnClicked.AddDynamic(Action, &USRHubRouteSettingAction::HandleClicked);

		USizeBox* ButtonSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), NAME_None);
		ButtonSizeBox->SetWidthOverride(96.0f);
		ButtonSizeBox->SetHeightOverride(54.0f);
		ButtonSizeBox->AddChild(Button);
		if (UHorizontalBoxSlot* ButtonSlot = ButtonBox->AddChildToHorizontalBox(ButtonSizeBox))
		{
			ButtonSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
			ButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}
	}

	void AddHubRouteConditionedModuleButton(
		UWidgetTree* WidgetTree,
		UHorizontalBox* ButtonBox,
		USRFacilityControlWidget* OwnerWidget,
		TArray<TObjectPtr<USRHubRouteSettingAction>>& OutActions,
		FName RouteId,
		ESRConditionedTransitModuleV2 NewModule,
		bool bEnabled)
	{
		if (!WidgetTree || !ButtonBox || !OwnerWidget || RouteId.IsNone())
		{
			return;
		}

		const FSRConditionedTransitModuleRulesV2 Rules = FSRConditionedTransitV2::GetModuleRules(NewModule);
		UButton* Button = ConstructDebugInputButton(
			WidgetTree,
			NAME_None,
			FText::FromString(FString::Printf(
				TEXT("Hold Module\n%s"),
				*GetConditionedTransitModuleShortLabel(Rules.Module))));
		Button->SetBackgroundColor(bEnabled
			? FLinearColor(0.105f, 0.125f, 0.205f, 0.95f)
			: FLinearColor(0.060f, 0.066f, 0.072f, 0.95f));
		Button->SetIsEnabled(bEnabled);
		Button->SetToolTipText(Rules.PreviewText);

		USRHubRouteSettingAction* Action = NewObject<USRHubRouteSettingAction>(OwnerWidget);
		Action->InitializeConditionedTransitModule(OwnerWidget, RouteId, Rules.Module, Button);
		OutActions.Add(Action);
		Button->OnClicked.AddDynamic(Action, &USRHubRouteSettingAction::HandleClicked);

		USizeBox* ButtonSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), NAME_None);
		ButtonSizeBox->SetWidthOverride(112.0f);
		ButtonSizeBox->SetHeightOverride(54.0f);
		ButtonSizeBox->AddChild(Button);
		if (UHorizontalBoxSlot* ButtonSlot = ButtonBox->AddChildToHorizontalBox(ButtonSizeBox))
		{
			ButtonSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
			ButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}
	}

	void AddHubRouteReturnEmptyButton(
		UWidgetTree* WidgetTree,
		UHorizontalBox* ButtonBox,
		USRFacilityControlWidget* OwnerWidget,
		TArray<TObjectPtr<USRHubRouteSettingAction>>& OutActions,
		FName RouteId,
		bool bNewReturnEmptyWhenNoCargo)
	{
		if (!WidgetTree || !ButtonBox || !OwnerWidget || RouteId.IsNone())
		{
			return;
		}

		UButton* Button = ConstructDebugInputButton(
			WidgetTree,
			NAME_None,
			bNewReturnEmptyWhenNoCargo
				? NSLOCTEXT("StarRoversFacilityControl", "HubRouteReturnEmptyOnButton", "Empty Return\nON")
				: NSLOCTEXT("StarRoversFacilityControl", "HubRouteReturnEmptyOffButton", "Empty Return\nOFF"));
		Button->SetBackgroundColor(bNewReturnEmptyWhenNoCargo
			? FLinearColor(0.070f, 0.145f, 0.105f, 0.95f)
			: FLinearColor(0.145f, 0.105f, 0.070f, 0.95f));

		USRHubRouteSettingAction* Action = NewObject<USRHubRouteSettingAction>(OwnerWidget);
		Action->InitializeReturnEmptyWhenNoCargo(OwnerWidget, RouteId, bNewReturnEmptyWhenNoCargo, Button);
		OutActions.Add(Action);
		Button->OnClicked.AddDynamic(Action, &USRHubRouteSettingAction::HandleClicked);

		USizeBox* ButtonSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), NAME_None);
		ButtonSizeBox->SetWidthOverride(78.0f);
		ButtonSizeBox->SetHeightOverride(54.0f);
		ButtonSizeBox->AddChild(Button);

		if (UHorizontalBoxSlot* ButtonSlot = ButtonBox->AddChildToHorizontalBox(ButtonSizeBox))
		{
			ButtonSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
			ButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}
	}

	void AddHubRouteCargoResourceButton(
		UWidgetTree* WidgetTree,
		UHorizontalBox* ButtonBox,
		USRFacilityControlWidget* OwnerWidget,
		TArray<TObjectPtr<USRHubRouteSettingAction>>& OutActions,
		FName RouteId,
		FName CargoResourceId)
	{
		if (!WidgetTree || !ButtonBox || !OwnerWidget || RouteId.IsNone())
		{
			return;
		}

		const FString Label = CargoResourceId.IsNone()
			? FString(TEXT("Any\nCargo"))
			: FString::Printf(TEXT("Only\n%s"), *BuildCompactResourceIdLabel(CargoResourceId));
		UButton* Button = ConstructDebugInputButton(WidgetTree, NAME_None, FText::FromString(Label));
		Button->SetBackgroundColor(CargoResourceId.IsNone()
			? FLinearColor(0.095f, 0.095f, 0.135f, 0.95f)
			: FLinearColor(0.120f, 0.105f, 0.165f, 0.95f));

		USRHubRouteSettingAction* Action = NewObject<USRHubRouteSettingAction>(OwnerWidget);
		Action->InitializeCargoResourceId(OwnerWidget, RouteId, CargoResourceId, Button);
		OutActions.Add(Action);
		Button->OnClicked.AddDynamic(Action, &USRHubRouteSettingAction::HandleClicked);

		USizeBox* ButtonSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), NAME_None);
		ButtonSizeBox->SetWidthOverride(86.0f);
		ButtonSizeBox->SetHeightOverride(54.0f);
		ButtonSizeBox->AddChild(Button);

		if (UHorizontalBoxSlot* ButtonSlot = ButtonBox->AddChildToHorizontalBox(ButtonSizeBox))
		{
			ButtonSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
			ButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}
	}

	void AddInputResourceSlotCard(
		UWidgetTree* WidgetTree,
		UVerticalBox* SlotBox,
		USRFacilityControlWidget* OwnerWidget,
		TArray<TObjectPtr<USRFacilityInputSlotDebugAction>>& OutActions,
		TArray<TObjectPtr<USRHubAutoMissileInventorySlotAction>>& OutAutoMissileActions,
		const FSRFacilityPortInventory& PortInventory,
		int32 InputPortIndex,
		const TCHAR* FallbackLabel,
		const FLinearColor& AccentColor,
		bool bAutoMissileLaunchEnabled,
		bool bAutoMissileLaunchSelectionActive)
	{
		if (!WidgetTree || !SlotBox)
		{
			return;
		}

		const int32 Capacity = FMath::Max(1, PortInventory.Capacity);
		const int32 SlotStackCount = StarRovers::FacilityResources::GetInventorySlotStackCount(PortInventory);
		const FSRResourceInstance* ResourceInstance = PortInventory.Inventory.IsEmpty()
			? nullptr
			: &PortInventory.Inventory[0];
		const FSRResourceGlyphPresentation GlyphPresentation = ResourceInstance
			? FSRResourceGlyphPresentationBuilder::Build(*ResourceInstance)
			: FSRResourceGlyphPresentation();
		const bool bHasResource = GlyphPresentation.bHasResource && SlotStackCount > 0;
		const bool bHasCapacity = SlotStackCount < Capacity;
		FString PortLabel = BuildInventoryCardPortLabel(PortInventory, InputPortIndex, FallbackLabel);
		if (bAutoMissileLaunchEnabled)
		{
			PortLabel += TEXT(" [M]");
		}
		else if (bAutoMissileLaunchSelectionActive)
		{
			PortLabel += TEXT(" [?]");
		}
		const FString Text = FString::Printf(
			TEXT("%s  %d/%d"),
			*PortLabel,
			SlotStackCount,
			Capacity);
		const FLinearColor CardColor = bAutoMissileLaunchEnabled
			? FLinearColor(0.190f, 0.115f, 0.065f, 0.98f)
			: (bHasResource
				? FLinearColor(0.125f, 0.175f, 0.160f, 0.98f)
				: FLinearColor(0.145f, 0.170f, 0.190f, 0.98f));
		const FLinearColor OuterBorderColor = bAutoMissileLaunchEnabled
			? FLinearColor(0.900f, 0.360f, 0.115f, 1.0f)
			: (bAutoMissileLaunchSelectionActive
				? FLinearColor(0.780f, 0.620f, 0.180f, 1.0f)
				: FLinearColor(0.005f, 0.006f, 0.007f, 1.0f));

		UVerticalBox* CardBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UTextBlock* SlotTextBlock = ConstructTextBlock(WidgetTree, NAME_None, 8, FLinearColor(0.90f, 0.94f, 0.96f, 1.0f));
		SlotTextBlock->SetText(FText::FromString(Text));
		SlotTextBlock->SetAutoWrapText(true);
		if (UVerticalBoxSlot* TextSlot = CardBox->AddChildToVerticalBox(SlotTextBlock))
		{
			TextSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
			TextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}

		USRResourceGlyphWidget* ResourceGlyph =
			WidgetTree->ConstructWidget<USRResourceGlyphWidget>(
				USRResourceGlyphWidget::StaticClass(),
				NAME_None);
		ResourceGlyph->SetGlyphMode(ESRResourceGlyphMode::Detailed);
		ResourceGlyph->SetPresentation(bHasResource
			? GlyphPresentation
			: FSRResourceGlyphPresentation());
		ResourceGlyph->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UVerticalBoxSlot* GlyphSlot = CardBox->AddChildToVerticalBox(ResourceGlyph))
		{
			GlyphSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 5.0f));
			GlyphSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			GlyphSlot->SetVerticalAlignment(VAlign_Center);
		}

		UHorizontalBox* ButtonRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		struct FSRFacilityInputDebugButtonSpec
		{
			FName ResourceId;
			FText Label;
			double EnergyValue = 0.0;
			int32 RemainingProcessLimit = 0;
		};
		const FSRFacilityInputDebugButtonSpec ButtonSpecs[] =
		{
			{ TEXT("Territe"), NSLOCTEXT("StarRoversFacilityControl", "SlotAddTerrite", "+T"), 1.0, 3 },
			{ TEXT("Aquid"), NSLOCTEXT("StarRoversFacilityControl", "SlotAddAquid", "+A"), 0.0, 5 },
			{ TEXT("Nitain"), NSLOCTEXT("StarRoversFacilityControl", "SlotAddNitain", "+N"), 3.0, 2 },
			{ TEXT("Waste"), NSLOCTEXT("StarRoversFacilityControl", "SlotAddWaste", "+W"), 0.1, 3 },
		};

		for (int32 ButtonIndex = 0; ButtonIndex < UE_ARRAY_COUNT(ButtonSpecs); ++ButtonIndex)
		{
			const FSRFacilityInputDebugButtonSpec& ButtonSpec = ButtonSpecs[ButtonIndex];
			FSRResourceInstance CandidateResource;
			CandidateResource.ResourceInstanceId = FName(*FGuid::NewGuid().ToString(EGuidFormats::Digits));
			CandidateResource.ResourceId = ButtonSpec.ResourceId;
			CandidateResource.EnergyValue = ButtonSpec.EnergyValue;
			CandidateResource.RemainingProcessLimit = ButtonSpec.RemainingProcessLimit;
			CandidateResource.StackCount = 1;
			StarRovers::Resources::SynchronizeLegacyRuntimeStateToResourceV2(CandidateResource);

			UButton* Button = ConstructDebugInputButton(WidgetTree, NAME_None, ButtonSpec.Label);
			Button->SetIsEnabled(StarRovers::FacilityResources::CanInventorySlotAcceptResource(PortInventory, CandidateResource));
			BindInputSlotDebugButton(Button, OwnerWidget, OutActions, InputPortIndex, ButtonSpec.ResourceId);
			if (UHorizontalBoxSlot* ButtonSlot = ButtonRow->AddChildToHorizontalBox(Button))
			{
				ButtonSlot->SetPadding(FMargin(0.0f, 0.0f, ButtonIndex < UE_ARRAY_COUNT(ButtonSpecs) - 1 ? 4.0f : 0.0f, 0.0f));
				ButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			}
		}
		if (UVerticalBoxSlot* ButtonRowSlot = CardBox->AddChildToVerticalBox(ButtonRow))
		{
			ButtonRowSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}

		USizeBox* CardSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		CardSizeBox->SetWidthOverride(176.0f);
		CardSizeBox->SetHeightOverride(132.0f);
		CardSizeBox->AddChild(CardBox);

		UBorder* SlotBorder = ConstructSectionBorder(
			WidgetTree,
			NAME_None,
			CardSizeBox,
			bHasCapacity ? CardColor : FLinearColor(0.060f, 0.070f, 0.082f, 0.98f),
			FMargin(5.0f, 3.0f));
		UBorder* OuterBorder = ConstructSectionBorder(WidgetTree, NAME_None, SlotBorder, OuterBorderColor, FMargin(2.0f));
		if (bHasResource)
		{
			OuterBorder->SetToolTipText(GlyphPresentation.ToolTipText);
		}
		if (OwnerWidget && InputPortIndex != INDEX_NONE)
		{
			USRHubAutoMissileInventorySlotAction* AutoMissileAction = NewObject<USRHubAutoMissileInventorySlotAction>(OwnerWidget);
			AutoMissileAction->Initialize(OwnerWidget, InputPortIndex, OuterBorder);
			OutAutoMissileActions.Add(AutoMissileAction);
		}
		AddWidgetToInventoryGrid(WidgetTree, SlotBox, OuterBorder, InputPortIndex);
	}

	UHorizontalBox* AddHubRouteCard(
		UWidgetTree* WidgetTree,
		UHorizontalBox* CardBox,
		USRFacilityControlWidget* OwnerWidget,
		TArray<TObjectPtr<USRHubRouteDestinationAction>>& OutDestinationActions,
		const FSRSpaceLogisticsHubEndpoint& DestinationHub,
		const FSRHubRouteCardPresentation& Presentation,
		bool bShowActions)
	{
		if (!WidgetTree || !CardBox || !OwnerWidget || !DestinationHub.IsValid())
		{
			return nullptr;
		}

		USRThemedCardWidget* Card = WidgetTree->ConstructWidget<USRThemedCardWidget>(
			USRThemedCardWidget::StaticClass(),
			NAME_None);
		Card->SetVisualState(Presentation.VisualState);
		Card->SetCardPadding(FMargin(8.0f, 7.0f));

		UVerticalBox* ContentBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), NAME_None);
		Card->SetContent(ContentBox);

		UButton* LaneButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), NAME_None);
		const FSRUIStatePalette CardPalette = USRUIThemeLibrary::ResolveStatePalette(Presentation.VisualState);
		LaneButton->SetBackgroundColor(CardPalette.SurfaceColor);
		LaneButton->SetToolTipText(FText::Format(
			NSLOCTEXT("StarRoversFacilityControl", "HubRouteSelectTooltip", "Select lane: {0}"),
			Presentation.LaneTitle));
		UTextBlock* LaneTextBlock = ConstructTextBlock(
			WidgetTree,
			NAME_None,
			10,
			CardPalette.PrimaryTextColor);
		LaneTextBlock->SetText(FText::Format(
			NSLOCTEXT("StarRoversFacilityControl", "HubRouteLaneLabel", "{0}  |  {1}"),
			Presentation.DirectionLabel,
			Presentation.LaneTitle));
		LaneTextBlock->SetAutoWrapText(false);
		LaneButton->AddChild(LaneTextBlock);
		if (UVerticalBoxSlot* HeaderSlot = ContentBox->AddChildToVerticalBox(LaneButton))
		{
			HeaderSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
			HeaderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}

		USRHubRouteDestinationAction* DestinationAction = NewObject<USRHubRouteDestinationAction>(OwnerWidget);
		DestinationAction->Initialize(OwnerWidget, DestinationHub, LaneButton);
		OutDestinationActions.Add(DestinationAction);
		LaneButton->OnClicked.AddDynamic(DestinationAction, &USRHubRouteDestinationAction::HandleClicked);

		USRStatusBadgeWidget* StatusBadge = WidgetTree->ConstructWidget<USRStatusBadgeWidget>(
			USRStatusBadgeWidget::StaticClass(),
			NAME_None);
		StatusBadge->SetBadge(Presentation.StatusLabel, Presentation.VisualState);
		if (UVerticalBoxSlot* StatusSlot = ContentBox->AddChildToVerticalBox(StatusBadge))
		{
			StatusSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 3.0f));
			StatusSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}

		auto AddDetailLine = [WidgetTree, ContentBox](const FText& Text, const FLinearColor& Color)
		{
			UTextBlock* DetailTextBlock = ConstructTextBlock(WidgetTree, NAME_None, 8, Color);
			DetailTextBlock->SetText(Text);
			DetailTextBlock->SetAutoWrapText(false);
			DetailTextBlock->SetToolTipText(Text);
			if (UVerticalBoxSlot* DetailSlot = ContentBox->AddChildToVerticalBox(DetailTextBlock))
			{
				DetailSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 2.0f));
				DetailSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			}
		};
		AddDetailLine(Presentation.PhaseDetail, CardPalette.PrimaryTextColor);

		if (Presentation.bShowProgress)
		{
			UProgressBar* ProgressBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), NAME_None);
			ProgressBar->SetPercent(Presentation.ProgressRatio);
			ProgressBar->SetFillColorAndOpacity(CardPalette.AccentColor);
			if (UVerticalBoxSlot* ProgressSlot = ContentBox->AddChildToVerticalBox(ProgressBar))
			{
				ProgressSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 3.0f));
				ProgressSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			}
		}

		if (Presentation.bShowCargoGlyph)
		{
			USRResourceGlyphWidget* CargoGlyph =
				WidgetTree->ConstructWidget<USRResourceGlyphWidget>(
					USRResourceGlyphWidget::StaticClass(),
					NAME_None);
			CargoGlyph->SetGlyphMode(ESRResourceGlyphMode::Detailed);
			CargoGlyph->SetPresentation(Presentation.CargoGlyph);
			CargoGlyph->SetVisibility(ESlateVisibility::HitTestInvisible);
			if (UVerticalBoxSlot* GlyphSlot = ContentBox->AddChildToVerticalBox(CargoGlyph))
			{
				GlyphSlot->SetPadding(FMargin(0.0f, 1.0f, 0.0f, 3.0f));
				GlyphSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			}
		}

		AddDetailLine(Presentation.CargoDetail, CardPalette.SecondaryTextColor);
		AddDetailLine(Presentation.ProfileDetail, CardPalette.SecondaryTextColor);
		AddDetailLine(Presentation.ModuleDetail, CardPalette.SecondaryTextColor);

		UHorizontalBox* ActionRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), NAME_None);
		if (bShowActions)
		{
			UScrollBox* ActionScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), NAME_None);
			ActionScrollBox->SetOrientation(Orient_Horizontal);
			ActionScrollBox->AddChild(ActionRow);
			if (UVerticalBoxSlot* ActionSlot = ContentBox->AddChildToVerticalBox(ActionScrollBox))
			{
				ActionSlot->SetPadding(FMargin(0.0f, 3.0f, 0.0f, 0.0f));
				ActionSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			}
		}

		USizeBox* CardSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), NAME_None);
		CardSizeBox->SetWidthOverride(382.0f);
		CardSizeBox->SetHeightOverride(
			(bShowActions ? 190.0f : 148.0f)
			+ (Presentation.bShowCargoGlyph ? 42.0f : 0.0f));
		CardSizeBox->AddChild(Card);
		if (UHorizontalBoxSlot* CardSlot = CardBox->AddChildToHorizontalBox(CardSizeBox))
		{
			CardSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
			CardSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}
		return ActionRow;
	}

	FSRResourceInstance MakeDebugEnergyResource(FName ResourceId, double EnergyValue, int32 RemainingProcessLimit)
	{
		FSRResourceInstance ResourceInstance;
		ResourceInstance.ResourceInstanceId = FName(*FGuid::NewGuid().ToString(EGuidFormats::Digits));
		ResourceInstance.ResourceId = ResourceId;
		ResourceInstance.EnergyValue = EnergyValue;
		ResourceInstance.RemainingProcessLimit = FMath::Max(0, RemainingProcessLimit);
		ResourceInstance.ProcessCount = 0;
		ResourceInstance.EnergyChangeCount = 0;
		ResourceInstance.StackCount = 1;
		StarRovers::Resources::SynchronizeLegacyRuntimeStateToResourceV2(ResourceInstance);
		return ResourceInstance;
	}
}

void USRFacilityInputSlotDebugAction::Initialize(USRFacilityControlWidget* InOwnerWidget, int32 InInputPortIndex, FName InResourceId, UButton* InButton)
{
	OwnerWidget = InOwnerWidget;
	InputPortIndex = InInputPortIndex;
	ResourceId = InResourceId;
	Button = InButton;
}

void USRFacilityInputSlotDebugAction::HandleClicked()
{
	SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl InputSlotDebug OnClicked InputPortIndex=%d ResourceId=%s"),
		InputPortIndex,
		*ResourceId.ToString());

	if (IsValid(OwnerWidget))
	{
		OwnerWidget->AddDebugInputResourceToPort(InputPortIndex, ResourceId);
	}
}

bool USRFacilityInputSlotDebugAction::TryHandleManualClick(const FVector2D& ScreenPosition)
{
	if (!IsValid(Button.Get())
		|| !Button->GetIsEnabled()
		|| !IsWidgetUnderScreenPosition(Button.Get(), ScreenPosition))
	{
		return false;
	}

	HandleClicked();
	return true;
}

void USRHubRouteDestinationAction::Initialize(
	USRFacilityControlWidget* InOwnerWidget,
	const FSRSpaceLogisticsHubEndpoint& InDestinationHub,
	UButton* InButton)
{
	OwnerWidget = InOwnerWidget;
	DestinationHub = InDestinationHub;
	Button = InButton;
}

void USRHubRouteDestinationAction::HandleClicked()
{
	SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl HubRouteDestination OnClicked Destination=%s/%s"),
		*GetNameSafe(DestinationHub.BodyActor.Get()),
		*DestinationHub.HubOccupantId.ToString());

	if (IsValid(OwnerWidget))
	{
		OwnerWidget->SelectRouteDestinationHubEndpoint(DestinationHub);
	}
}

bool USRHubRouteDestinationAction::TryHandleManualClick(const FVector2D& ScreenPosition)
{
	if (!IsValid(Button.Get())
		|| !Button->GetIsEnabled()
		|| !IsWidgetUnderScreenPosition(Button.Get(), ScreenPosition))
	{
		return false;
	}

	HandleClicked();
	return true;
}

void USRHubRouteLaunchAction::Initialize(
	USRFacilityControlWidget* InOwnerWidget,
	const FSRSpaceLogisticsHubEndpoint& InDestinationHub,
	UButton* InButton)
{
	OwnerWidget = InOwnerWidget;
	DestinationHub = InDestinationHub;
	Button = InButton;
}

void USRHubRouteLaunchAction::HandleClicked()
{
	SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl HubRouteLaunch OnClicked Destination=%s/%s"),
		*GetNameSafe(DestinationHub.BodyActor.Get()),
		*DestinationHub.HubOccupantId.ToString());

	if (IsValid(OwnerWidget))
	{
		OwnerWidget->CreateRouteToHubEndpoint(DestinationHub);
	}
}

bool USRHubRouteLaunchAction::TryHandleManualClick(const FVector2D& ScreenPosition)
{
	if (!IsValid(Button.Get())
		|| !Button->GetIsEnabled()
		|| !IsWidgetUnderScreenPosition(Button.Get(), ScreenPosition))
	{
		return false;
	}

	HandleClicked();
	return true;
}

void USRHubRouteRemovalAction::Initialize(USRFacilityControlWidget* InOwnerWidget, FName InRouteId, UButton* InButton)
{
	OwnerWidget = InOwnerWidget;
	RouteId = InRouteId;
	Button = InButton;
}

void USRHubRouteRemovalAction::HandleClicked()
{
	SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl HubRouteRemoval OnClicked RouteId=%s"),
		*RouteId.ToString());

	if (IsValid(OwnerWidget))
	{
		OwnerWidget->RemoveHubRoute(RouteId);
	}
}

bool USRHubRouteRemovalAction::TryHandleManualClick(const FVector2D& ScreenPosition)
{
	if (!IsValid(Button.Get())
		|| !Button->GetIsEnabled()
		|| !IsWidgetUnderScreenPosition(Button.Get(), ScreenPosition))
	{
		return false;
	}

	HandleClicked();
	return true;
}

void USRHubRouteDebugOrbitAction::Initialize(USRFacilityControlWidget* InOwnerWidget, UButton* InButton)
{
	OwnerWidget = InOwnerWidget;
	Button = InButton;
}

void USRHubRouteDebugOrbitAction::HandleClicked()
{
	SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl HubRouteDebugOrbit OnClicked"));

	if (IsValid(OwnerWidget))
	{
		OwnerWidget->LaunchDebugLocalOrbitRoute();
	}
}

bool USRHubRouteDebugOrbitAction::TryHandleManualClick(const FVector2D& ScreenPosition)
{
	if (!IsValid(Button.Get())
		|| !Button->GetIsEnabled()
		|| !IsWidgetUnderScreenPosition(Button.Get(), ScreenPosition))
	{
		return false;
	}

	HandleClicked();
	return true;
}

void USRHubStarFuelMissileLaunchAction::Initialize(USRFacilityControlWidget* InOwnerWidget, UButton* InButton)
{
	OwnerWidget = InOwnerWidget;
	Button = InButton;
}

void USRHubStarFuelMissileLaunchAction::HandleClicked()
{
	SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl HubStarFuelMissileLaunch OnClicked"));

	if (IsValid(OwnerWidget))
	{
		OwnerWidget->BeginSelectStarFuelMissileAutoLaunchSlot();
	}
}

bool USRHubStarFuelMissileLaunchAction::TryHandleManualClick(const FVector2D& ScreenPosition)
{
	if (!IsValid(Button.Get())
		|| !Button->GetIsEnabled()
		|| !IsWidgetUnderScreenPosition(Button.Get(), ScreenPosition))
	{
		return false;
	}

	HandleClicked();
	return true;
}

void USRHubAutoMissileInventorySlotAction::Initialize(
	USRFacilityControlWidget* InOwnerWidget,
	int32 InInputPortIndex,
	UWidget* InSlotWidget)
{
	OwnerWidget = InOwnerWidget;
	InputPortIndex = InInputPortIndex;
	SlotWidget = InSlotWidget;
}

bool USRHubAutoMissileInventorySlotAction::TryHandleManualClick(const FVector2D& ScreenPosition)
{
	if (!IsValid(SlotWidget.Get()) || !IsWidgetUnderScreenPosition(SlotWidget.Get(), ScreenPosition))
	{
		return false;
	}

	return IsValid(OwnerWidget) && OwnerWidget->SelectStarFuelMissileAutoLaunchInputPort(InputPortIndex);
}

void USRHubRouteSettingAction::InitializeMaxCargoStackCount(
	USRFacilityControlWidget* InOwnerWidget,
	FName InRouteId,
	int32 InMaxCargoStackCount,
	UButton* InButton)
{
	OwnerWidget = InOwnerWidget;
	RouteId = InRouteId;
	MaxCargoStackCount = FMath::Max(1, InMaxCargoStackCount);
	bSetMaxCargoStackCount = true;
	bSetReturnEmptyWhenNoCargo = false;
	bSetCargoResourceId = false;
	bSetRouteProfile = false;
	bSetConditionedTransitModule = false;
	Button = InButton;
}

void USRHubRouteSettingAction::InitializeReturnEmptyWhenNoCargo(
	USRFacilityControlWidget* InOwnerWidget,
	FName InRouteId,
	bool bInReturnEmptyWhenNoCargo,
	UButton* InButton)
{
	OwnerWidget = InOwnerWidget;
	RouteId = InRouteId;
	bReturnEmptyWhenNoCargo = bInReturnEmptyWhenNoCargo;
	bSetMaxCargoStackCount = false;
	bSetReturnEmptyWhenNoCargo = true;
	bSetCargoResourceId = false;
	bSetRouteProfile = false;
	bSetConditionedTransitModule = false;
	Button = InButton;
}

void USRHubRouteSettingAction::InitializeCargoResourceId(
	USRFacilityControlWidget* InOwnerWidget,
	FName InRouteId,
	FName InCargoResourceId,
	UButton* InButton)
{
	OwnerWidget = InOwnerWidget;
	RouteId = InRouteId;
	CargoResourceId = InCargoResourceId;
	bSetMaxCargoStackCount = false;
	bSetReturnEmptyWhenNoCargo = false;
	bSetCargoResourceId = true;
	bSetRouteProfile = false;
	bSetConditionedTransitModule = false;
	Button = InButton;
}

void USRHubRouteSettingAction::InitializeRouteProfile(
	USRFacilityControlWidget* InOwnerWidget,
	FName InRouteId,
	ESRSpaceLogisticsRouteProfileV2 InRouteProfile,
	UButton* InButton)
{
	OwnerWidget = InOwnerWidget;
	RouteId = InRouteId;
	RouteProfile = InRouteProfile;
	bSetMaxCargoStackCount = false;
	bSetReturnEmptyWhenNoCargo = false;
	bSetCargoResourceId = false;
	bSetRouteProfile = true;
	bSetConditionedTransitModule = false;
	Button = InButton;
}

void USRHubRouteSettingAction::InitializeConditionedTransitModule(
	USRFacilityControlWidget* InOwnerWidget,
	FName InRouteId,
	ESRConditionedTransitModuleV2 InModule,
	UButton* InButton)
{
	OwnerWidget = InOwnerWidget;
	RouteId = InRouteId;
	ConditionedTransitModule = InModule;
	bSetMaxCargoStackCount = false;
	bSetReturnEmptyWhenNoCargo = false;
	bSetCargoResourceId = false;
	bSetRouteProfile = false;
	bSetConditionedTransitModule = true;
	Button = InButton;
}

void USRHubRouteSettingAction::HandleClicked()
{
	SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl HubRouteSetting OnClicked RouteId=%s SetStack=%s Stack=%d SetReturnEmpty=%s ReturnEmpty=%s SetCargoResource=%s CargoResourceId=%s SetProfile=%s Profile=%d SetModule=%s Module=%d"),
		*RouteId.ToString(),
		bSetMaxCargoStackCount ? TEXT("true") : TEXT("false"),
		MaxCargoStackCount,
		bSetReturnEmptyWhenNoCargo ? TEXT("true") : TEXT("false"),
		bReturnEmptyWhenNoCargo ? TEXT("true") : TEXT("false"),
		bSetCargoResourceId ? TEXT("true") : TEXT("false"),
		CargoResourceId.IsNone() ? TEXT("Any") : *CargoResourceId.ToString(),
		bSetRouteProfile ? TEXT("true") : TEXT("false"),
		static_cast<int32>(RouteProfile),
		bSetConditionedTransitModule ? TEXT("true") : TEXT("false"),
		static_cast<int32>(ConditionedTransitModule));

	if (!IsValid(OwnerWidget))
	{
		return;
	}

	if (bSetMaxCargoStackCount)
	{
		OwnerWidget->SetHubRouteMaxCargoStackCount(RouteId, MaxCargoStackCount);
	}
	else if (bSetReturnEmptyWhenNoCargo)
	{
		OwnerWidget->SetHubRouteReturnEmptyWhenNoCargo(RouteId, bReturnEmptyWhenNoCargo);
	}
	else if (bSetCargoResourceId)
	{
		OwnerWidget->SetHubRouteCargoResourceId(RouteId, CargoResourceId);
	}
	else if (bSetRouteProfile)
	{
		OwnerWidget->SetHubRouteProfile(RouteId, RouteProfile);
	}
	else if (bSetConditionedTransitModule)
	{
		OwnerWidget->SetHubRouteConditionedTransitModule(RouteId, ConditionedTransitModule);
	}
}

bool USRHubRouteSettingAction::TryHandleManualClick(const FVector2D& ScreenPosition)
{
	if (!IsValid(Button.Get())
		|| !Button->GetIsEnabled()
		|| !IsWidgetUnderScreenPosition(Button.Get(), ScreenPosition))
	{
		return false;
	}

	HandleClicked();
	return true;
}

TSharedRef<SWidget> USRFacilityControlWidget::RebuildWidget()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
	}

	BuildFacilityControlWidgetTree();
	return Super::RebuildWidget();
}

void USRFacilityControlWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BuildFacilityControlWidgetTree();
	BindControlHandlers();
	RefreshControlText();
}

void USRFacilityControlWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	BuildFacilityControlWidgetTree();
	RefreshControlText();
}

void USRFacilityControlWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!IsVisible() || !bHasFocusedFacility)
	{
		return;
	}

	RefreshControlText();
}

FReply USRFacilityControlWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FVector2D ScreenPosition = InMouseEvent.GetScreenSpacePosition();
	if (IsScreenPositionOverControlPanel(ScreenPosition))
	{
		SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl NativeOnMouseButtonDown handled Mouse=(%.1f, %.1f)"),
			ScreenPosition.X,
			ScreenPosition.Y);
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply USRFacilityControlWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FVector2D ScreenPosition = InMouseEvent.GetScreenSpacePosition();
	if (IsScreenPositionOverControlPanel(ScreenPosition))
	{
		SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl NativeOnMouseButtonUp handled Mouse=(%.1f, %.1f)"),
			ScreenPosition.X,
			ScreenPosition.Y);
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply USRFacilityControlWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (IsScreenPositionOverControlPanel(InMouseEvent.GetScreenSpacePosition()))
	{
		return FReply::Handled();
	}

	return Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
}

void USRFacilityControlWidget::SetFocusedFacility(AActor* NewFocusedActor, FName NewOccupantId)
{
	const bool bFocusChanged = FocusedActor.Get() != NewFocusedActor || FocusedOccupantId != NewOccupantId;
	FocusedActor = NewFocusedActor;
	FocusedOccupantId = NewOccupantId;
	bHasFocusedFacility = IsValid(NewFocusedActor) && !NewOccupantId.IsNone();
	if (bFocusChanged)
	{
		LastHubRouteStatus.Reset();
		HubRoutePanelSignature.Reset();
		InputInventoryPanelSignature.Reset();
		SelectedHubRouteDestination = FSRSpaceLogisticsHubEndpoint();
		bHasSelectedHubRouteDestination = false;
		bSelectingHubStarFuelMissileAutoLaunchSlot = false;
	}
	RefreshControlText();
}

void USRFacilityControlWidget::ClearFocusedFacility()
{
	FocusedActor.Reset();
	FocusedOccupantId = NAME_None;
	bHasFocusedFacility = false;
	LastHubRouteStatus.Reset();
	HubRoutePanelSignature.Reset();
	InputInventoryPanelSignature.Reset();
	SelectedHubRouteDestination = FSRSpaceLogisticsHubEndpoint();
	bHasSelectedHubRouteDestination = false;
	bSelectingHubStarFuelMissileAutoLaunchSlot = false;
	RefreshControlText();
}

bool USRFacilityControlWidget::HasFocusedFacility() const
{
	return bHasFocusedFacility;
}

bool USRFacilityControlWidget::IsPointerOverControlPanel() const
{
	if (!FSlateApplication::IsInitialized())
	{
		return false;
	}

	return IsScreenPositionOverControlPanel(FSlateApplication::Get().GetCursorPos());
}

bool USRFacilityControlWidget::TryHandleFacilityControlPointerClick()
{
	if (!IsVisible() || !FSlateApplication::IsInitialized())
	{
		return false;
	}

	const FVector2D ScreenPosition = FSlateApplication::Get().GetCursorPos();
	if (!IsScreenPositionOverControlPanel(ScreenPosition))
	{
		return false;
	}

	SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl TryHandleFacilityControlPointerClick Mouse=(%.1f, %.1f)"),
		ScreenPosition.X,
		ScreenPosition.Y);

	if (IsWidgetUnderScreenPosition(CloseButton, ScreenPosition) && CloseButton->GetIsEnabled())
	{
		SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl manual click resolved CloseButton"));
		HandleCloseClicked();
		return true;
	}

	if (IsWidgetUnderScreenPosition(ProcessCheckBox, ScreenPosition) && ProcessCheckBox->GetIsEnabled())
	{
		const bool bNewChecked = !ProcessCheckBox->IsChecked();
		SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl manual click resolved ProcessCheckBox bNewChecked=%s"),
			bNewChecked ? TEXT("true") : TEXT("false"));
		HandleProcessCheckStateChanged(bNewChecked);
		return true;
	}

	if (IsWidgetUnderScreenPosition(OperationalPriorityButton, ScreenPosition)
		&& OperationalPriorityButton->GetIsEnabled())
	{
		SR_LOG(UIClickTrace, LogTemp, Log,
			TEXT("SR UI Click Trace: FacilityControl manual click resolved OperationalPriorityButton"));
		HandleCycleOperationalPriorityClicked();
		return true;
	}

	if (IsWidgetUnderScreenPosition(ResourceV2RecipeButton, ScreenPosition)
		&& ResourceV2RecipeButton->GetIsEnabled())
	{
		SR_LOG(UIClickTrace, LogTemp, Log,
			TEXT("SR UI Click Trace: FacilityControl manual click resolved ResourceV2RecipeButton"));
		HandleCycleResourceV2RecipeClicked();
		return true;
	}

	if (IsWidgetUnderScreenPosition(DeliverCheckBox, ScreenPosition) && DeliverCheckBox->GetIsEnabled())
	{
		const bool bNewChecked = !DeliverCheckBox->IsChecked();
		SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl manual click resolved DeliverCheckBox bNewChecked=%s"),
			bNewChecked ? TEXT("true") : TEXT("false"));
		HandleDeliverCheckStateChanged(bNewChecked);
		return true;
	}

	if (IsWidgetUnderScreenPosition(DebugAddTerriteButton, ScreenPosition) && DebugAddTerriteButton->GetIsEnabled())
	{
		SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl manual click resolved DebugAddTerriteButton"));
		HandleDebugAddTerriteClicked();
		return true;
	}

	if (IsWidgetUnderScreenPosition(DebugAddAquidButton, ScreenPosition) && DebugAddAquidButton->GetIsEnabled())
	{
		SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl manual click resolved DebugAddAquidButton"));
		HandleDebugAddAquidClicked();
		return true;
	}

	if (IsWidgetUnderScreenPosition(DebugAddNitainButton, ScreenPosition) && DebugAddNitainButton->GetIsEnabled())
	{
		SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl manual click resolved DebugAddNitainButton"));
		HandleDebugAddNitainClicked();
		return true;
	}

	if (IsWidgetUnderScreenPosition(DebugAddWasteButton, ScreenPosition) && DebugAddWasteButton->GetIsEnabled())
	{
		SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl manual click resolved DebugAddWasteButton"));
		HandleDebugAddWasteClicked();
		return true;
	}

	for (USRFacilityInputSlotDebugAction* InputSlotDebugAction : InputSlotDebugActions)
	{
		if (IsValid(InputSlotDebugAction) && InputSlotDebugAction->TryHandleManualClick(ScreenPosition))
		{
			SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl manual click resolved InputSlotDebugButton"));
			return true;
		}
	}

	for (USRHubAutoMissileInventorySlotAction* HubAutoMissileInventorySlotAction : HubAutoMissileInventorySlotActions)
	{
		if (IsValid(HubAutoMissileInventorySlotAction) && HubAutoMissileInventorySlotAction->TryHandleManualClick(ScreenPosition))
		{
			SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl manual click resolved HubAutoMissileInventorySlot"));
			return true;
		}
	}

	for (USRHubRouteDestinationAction* HubRouteDestinationAction : HubRouteDestinationActions)
	{
		if (IsValid(HubRouteDestinationAction) && HubRouteDestinationAction->TryHandleManualClick(ScreenPosition))
		{
			SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl manual click resolved HubRouteDestinationButton"));
			return true;
		}
	}

	for (USRHubRouteLaunchAction* HubRouteLaunchAction : HubRouteLaunchActions)
	{
		if (IsValid(HubRouteLaunchAction) && HubRouteLaunchAction->TryHandleManualClick(ScreenPosition))
		{
			SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl manual click resolved HubRouteLaunchButton"));
			return true;
		}
	}

	for (USRHubRouteRemovalAction* HubRouteRemovalAction : HubRouteRemovalActions)
	{
		if (IsValid(HubRouteRemovalAction) && HubRouteRemovalAction->TryHandleManualClick(ScreenPosition))
		{
			SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl manual click resolved HubRouteRemoveButton"));
			return true;
		}
	}

	for (USRHubRouteDebugOrbitAction* HubRouteDebugOrbitAction : HubRouteDebugOrbitActions)
	{
		if (IsValid(HubRouteDebugOrbitAction) && HubRouteDebugOrbitAction->TryHandleManualClick(ScreenPosition))
		{
			SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl manual click resolved HubRouteDebugOrbitButton"));
			return true;
		}
	}

	for (USRHubStarFuelMissileLaunchAction* HubStarFuelMissileLaunchAction : HubStarFuelMissileLaunchActions)
	{
		if (IsValid(HubStarFuelMissileLaunchAction) && HubStarFuelMissileLaunchAction->TryHandleManualClick(ScreenPosition))
		{
			SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl manual click resolved HubStarFuelMissileLaunchButton"));
			return true;
		}
	}

	for (USRHubRouteSettingAction* HubRouteSettingAction : HubRouteSettingActions)
	{
		if (IsValid(HubRouteSettingAction) && HubRouteSettingAction->TryHandleManualClick(ScreenPosition))
		{
			SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl manual click resolved HubRouteSettingButton"));
			return true;
		}
	}

	SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl manual click consumed panel background."));
	return true;
}

bool USRFacilityControlWidget::AddDebugInputResourceToPort(int32 InputPortIndex, FName ResourceId)
{
	USRFacilityNetworkComponent* FacilityNetwork = GetFocusedFacilityNetwork();
	if (!IsValid(FacilityNetwork) || FocusedOccupantId.IsNone() || InputPortIndex == INDEX_NONE)
	{
		return false;
	}

	FSRResourceInstance ResourceInstance;
	if (ResourceId == FName(TEXT("Territe")))
	{
		ResourceInstance = MakeDebugEnergyResource(TEXT("Territe"), 1.0, 3);
	}
	else if (ResourceId == FName(TEXT("Aquid")))
	{
		ResourceInstance = MakeDebugEnergyResource(TEXT("Aquid"), 0.0, 5);
	}
	else if (ResourceId == FName(TEXT("Nitain")))
	{
		ResourceInstance = MakeDebugEnergyResource(TEXT("Nitain"), 3.0, 2);
	}
	else if (ResourceId == FName(TEXT("Waste")))
	{
		ResourceInstance = MakeDebugEnergyResource(TEXT("Waste"), 0.1, 3);
	}
	else
	{
		return false;
	}

	const bool bAdded = FacilityNetwork->AddInputResourceToPort(FocusedOccupantId, InputPortIndex, ResourceInstance);
	RefreshControlText();
	return bAdded;
}

bool USRFacilityControlWidget::SelectRouteDestinationHubEndpoint(const FSRSpaceLogisticsHubEndpoint& DestinationHub)
{
	USRFacilityNetworkComponent* FacilityNetwork = GetFocusedFacilityNetwork();
	if (!IsValid(FacilityNetwork) || !FacilityNetwork->IsHubFacility(FocusedOccupantId))
	{
		LastHubRouteStatus = TEXT("Destination select failed: selected facility is not a Hub.");
		RefreshControlText();
		return false;
	}

	if (!DestinationHub.IsValid())
	{
		LastHubRouteStatus = TEXT("Destination select failed: invalid Hub endpoint.");
		RefreshControlText();
		return false;
	}

	USRSpaceLogisticsSubsystem* SpaceLogisticsSubsystem = GetSpaceLogisticsSubsystem();
	if (!IsValid(SpaceLogisticsSubsystem))
	{
		LastHubRouteStatus = TEXT("Destination select failed: logistics subsystem unavailable.");
		RefreshControlText();
		return false;
	}

	FSRSpaceLogisticsHubEndpoint SourceHub;
	if (!SpaceLogisticsSubsystem->GetHubEndpoint(FocusedActor.Get(), FocusedOccupantId, SourceHub))
	{
		LastHubRouteStatus = TEXT("Destination select failed: source Hub endpoint not found.");
		RefreshControlText();
		return false;
	}

	if (AreHubEndpointKeysEqual(SourceHub, DestinationHub))
	{
		LastHubRouteStatus = TEXT("Destination select failed: select a different Hub.");
		RefreshControlText();
		return false;
	}
	TArray<FSRSpaceLogisticsHubRoute> ExistingRoutes;
	SpaceLogisticsSubsystem->GetHubRoutes(ExistingRoutes);
	const bool bExistingLane = ExistingRoutes.ContainsByPredicate(
		[&SourceHub, &DestinationHub](const FSRSpaceLogisticsHubRoute& Route)
		{
			return !Route.bDebugLocalOrbit
				&& DoesHubRouteConnectEndpoints(Route, SourceHub, DestinationHub);
		});

	SelectedHubRouteDestination = DestinationHub;
	bHasSelectedHubRouteDestination = true;
	LastHubRouteStatus = bExistingLane
		? FString::Printf(
			TEXT("Lane selected: %s. Configure it on the expanded Route card."),
			*BuildCelestialBodyDisplayName(DestinationHub.BodyActor.Get()))
		: FString::Printf(
			TEXT("Destination selected: %s. Press Create Route."),
			*BuildCelestialBodyDisplayName(DestinationHub.BodyActor.Get()));
	HubRoutePanelSignature.Reset();
	RefreshControlText();
	return true;
}

bool USRFacilityControlWidget::CreateRouteToHubEndpoint(const FSRSpaceLogisticsHubEndpoint& DestinationHub)
{
	USRFacilityNetworkComponent* FacilityNetwork = GetFocusedFacilityNetwork();
	if (!IsValid(FacilityNetwork) || !FacilityNetwork->IsHubFacility(FocusedOccupantId))
	{
		LastHubRouteStatus = TEXT("Route failed: selected facility is not a Hub.");
		RefreshControlText();
		return false;
	}

	USRSpaceLogisticsSubsystem* SpaceLogisticsSubsystem = GetSpaceLogisticsSubsystem();
	if (!IsValid(SpaceLogisticsSubsystem))
	{
		LastHubRouteStatus = TEXT("Route failed: logistics subsystem unavailable.");
		RefreshControlText();
		return false;
	}

	FSRSpaceLogisticsHubEndpoint SourceHub;
	if (!SpaceLogisticsSubsystem->GetHubEndpoint(FocusedActor.Get(), FocusedOccupantId, SourceHub))
	{
		LastHubRouteStatus = TEXT("Route failed: source Hub endpoint not found.");
		RefreshControlText();
		return false;
	}

	FName RouteId = NAME_None;
	const bool bCreated = SpaceLogisticsSubsystem->CreateHubRoute(
		SourceHub,
		DestinationHub,
		RouteId,
		true,
		UsesResourceV2Display()
			? FSRFleetCapacityV2::GetRouteProfileRules(
				ESRSpaceLogisticsRouteProfileV2::NeutralShuttle).CargoCapacity
			: 1);
	LastHubRouteStatus = bCreated
		? FString::Printf(TEXT("Route created: %s"), *RouteId.ToString())
		: TEXT("Route failed: endpoint invalid or route already exists.");
	if (bCreated)
	{
		SelectedHubRouteDestination = FSRSpaceLogisticsHubEndpoint();
		bHasSelectedHubRouteDestination = false;
	}
	HubRoutePanelSignature.Reset();
	RefreshControlText();
	return bCreated;
}

bool USRFacilityControlWidget::LaunchDebugLocalOrbitRoute()
{
	USRFacilityNetworkComponent* FacilityNetwork = GetFocusedFacilityNetwork();
	if (!IsValid(FacilityNetwork) || !FacilityNetwork->IsHubFacility(FocusedOccupantId))
	{
		LastHubRouteStatus = TEXT("Debug orbit failed: selected facility is not a Hub.");
		RefreshControlText();
		return false;
	}

	USRSpaceLogisticsSubsystem* SpaceLogisticsSubsystem = GetSpaceLogisticsSubsystem();
	if (!IsValid(SpaceLogisticsSubsystem))
	{
		LastHubRouteStatus = TEXT("Debug orbit failed: logistics subsystem unavailable.");
		RefreshControlText();
		return false;
	}

	FSRSpaceLogisticsHubEndpoint SourceHub;
	if (!SpaceLogisticsSubsystem->GetHubEndpoint(FocusedActor.Get(), FocusedOccupantId, SourceHub))
	{
		LastHubRouteStatus = TEXT("Debug orbit failed: source Hub endpoint not found.");
		RefreshControlText();
		return false;
	}

	FName RouteId = NAME_None;
	const bool bCreated = SpaceLogisticsSubsystem->CreateDebugLocalOrbitRoute(
		SourceHub,
		RouteId);
	LastHubRouteStatus = bCreated
		? FString::Printf(TEXT("Debug orbit launched: %s"), *RouteId.ToString())
		: TEXT("Debug orbit failed: already active or endpoint invalid.");
	HubRoutePanelSignature.Reset();
	RefreshControlText();
	return bCreated;
}

bool USRFacilityControlWidget::LaunchStarFuelMissileFromFocusedHub()
{
	USRFacilityNetworkComponent* FacilityNetwork = GetFocusedFacilityNetwork();
	if (!IsValid(FacilityNetwork) || !FacilityNetwork->IsHubFacility(FocusedOccupantId))
	{
		LastHubRouteStatus = TEXT("Missile failed: selected facility is not a Hub.");
		RefreshControlText();
		return false;
	}

	USRSpaceLogisticsSubsystem* SpaceLogisticsSubsystem = GetSpaceLogisticsSubsystem();
	if (!IsValid(SpaceLogisticsSubsystem))
	{
		LastHubRouteStatus = TEXT("Missile failed: logistics subsystem unavailable.");
		RefreshControlText();
		return false;
	}

	FSRSpaceLogisticsHubEndpoint SourceHub;
	if (!SpaceLogisticsSubsystem->GetHubEndpoint(FocusedActor.Get(), FocusedOccupantId, SourceHub))
	{
		LastHubRouteStatus = TEXT("Missile failed: source Hub endpoint not found.");
		RefreshControlText();
		return false;
	}

	FName MissileId = NAME_None;
	const bool bLaunched = SpaceLogisticsSubsystem->LaunchStarFuelMissileFromHub(SourceHub, MissileId);
	LastHubRouteStatus = bLaunched
		? FString::Printf(TEXT("Missile launched: %s"), *MissileId.ToString())
		: TEXT("Missile failed: no positive-energy cargo or target star unavailable.");
	HubRoutePanelSignature.Reset();
	RefreshControlText();
	return bLaunched;
}

bool USRFacilityControlWidget::BeginSelectStarFuelMissileAutoLaunchSlot()
{
	USRFacilityNetworkComponent* FacilityNetwork = GetFocusedFacilityNetwork();
	if (!IsValid(FacilityNetwork) || !FacilityNetwork->IsHubFacility(FocusedOccupantId))
	{
		LastHubRouteStatus = TEXT("Missile auto-launch failed: selected facility is not a Hub.");
		RefreshControlText();
		return false;
	}

	FSRFacilityInstance FacilityInstance;
	if (!FacilityNetwork->GetFacilityInstance(FocusedOccupantId, FacilityInstance) || FacilityInstance.InputPortInventories.IsEmpty())
	{
		LastHubRouteStatus = TEXT("Missile auto-launch failed: no input inventory slots.");
		RefreshControlText();
		return false;
	}

	bSelectingHubStarFuelMissileAutoLaunchSlot = !bSelectingHubStarFuelMissileAutoLaunchSlot;
	LastHubRouteStatus = bSelectingHubStarFuelMissileAutoLaunchSlot
		? TEXT("Select an input inventory slot to toggle missile auto-launch.")
		: TEXT("Missile auto-launch slot selection canceled.");
	InputInventoryPanelSignature.Reset();
	HubRoutePanelSignature.Reset();
	RefreshControlText();
	return true;
}

bool USRFacilityControlWidget::SelectStarFuelMissileAutoLaunchInputPort(int32 InputPortIndex)
{
	if (!bSelectingHubStarFuelMissileAutoLaunchSlot)
	{
		return false;
	}

	USRFacilityNetworkComponent* FacilityNetwork = GetFocusedFacilityNetwork();
	FSRFacilityInstance FacilityInstance;
	if (!IsValid(FacilityNetwork)
		|| !FacilityNetwork->IsHubFacility(FocusedOccupantId)
		|| !FacilityNetwork->GetFacilityInstance(FocusedOccupantId, FacilityInstance)
		|| !FacilityInstance.InputPortInventories.IsValidIndex(InputPortIndex))
	{
		LastHubRouteStatus = TEXT("Missile auto-launch failed: invalid input inventory slot.");
		bSelectingHubStarFuelMissileAutoLaunchSlot = false;
		InputInventoryPanelSignature.Reset();
		HubRoutePanelSignature.Reset();
		RefreshControlText();
		return false;
	}

	const bool bCurrentlyEnabled = FacilityNetwork->IsHubStarFuelMissileAutoLaunchInputPort(FocusedOccupantId, InputPortIndex);
	const bool bSet = FacilityNetwork->SetHubStarFuelMissileAutoLaunchInputPort(FocusedOccupantId, InputPortIndex, !bCurrentlyEnabled);
	const FString SlotLabel = BuildInventoryCardPortLabel(FacilityInstance.InputPortInventories[InputPortIndex], InputPortIndex, TEXT("Input"));
	LastHubRouteStatus = bSet
		? FString::Printf(
			TEXT("Missile auto-launch %s: %s."),
			bCurrentlyEnabled ? TEXT("disabled") : TEXT("enabled"),
			*SlotLabel)
		: FString::Printf(TEXT("Missile auto-launch failed: %s."), *SlotLabel);
	bSelectingHubStarFuelMissileAutoLaunchSlot = false;
	InputInventoryPanelSignature.Reset();
	HubRoutePanelSignature.Reset();
	RefreshControlText();
	return bSet;
}

bool USRFacilityControlWidget::RemoveHubRoute(FName RouteId)
{
	if (RouteId.IsNone())
	{
		LastHubRouteStatus = TEXT("Remove failed: invalid route.");
		RefreshControlText();
		return false;
	}

	USRSpaceLogisticsSubsystem* SpaceLogisticsSubsystem = GetSpaceLogisticsSubsystem();
	if (!IsValid(SpaceLogisticsSubsystem))
	{
		LastHubRouteStatus = TEXT("Remove failed: logistics subsystem unavailable.");
		RefreshControlText();
		return false;
	}

	const bool bRemoved = SpaceLogisticsSubsystem->RemoveHubRoute(RouteId);
	LastHubRouteStatus = bRemoved
		? FString::Printf(TEXT("Route removed: %s"), *RouteId.ToString())
		: FString::Printf(TEXT("Remove failed: %s"), *RouteId.ToString());
	HubRoutePanelSignature.Reset();
	RefreshControlText();
	return bRemoved;
}

bool USRFacilityControlWidget::SetHubRouteMaxCargoStackCount(FName RouteId, int32 MaxCargoStackCount)
{
	if (RouteId.IsNone())
	{
		LastHubRouteStatus = TEXT("Stack update failed: invalid route.");
		RefreshControlText();
		return false;
	}

	USRSpaceLogisticsSubsystem* SpaceLogisticsSubsystem = GetSpaceLogisticsSubsystem();
	if (!IsValid(SpaceLogisticsSubsystem))
	{
		LastHubRouteStatus = TEXT("Stack update failed: logistics subsystem unavailable.");
		RefreshControlText();
		return false;
	}

	const int32 ClampedMaxCargoStackCount = FMath::Max(1, MaxCargoStackCount);
	const bool bUpdated = SpaceLogisticsSubsystem->SetHubRouteMaxCargoStackCount(RouteId, ClampedMaxCargoStackCount);
	LastHubRouteStatus = bUpdated
		? FString::Printf(TEXT("Route stack updated: x%d"), ClampedMaxCargoStackCount)
		: FString::Printf(TEXT("Stack update failed: %s"), *RouteId.ToString());
	HubRoutePanelSignature.Reset();
	RefreshControlText();
	return bUpdated;
}

bool USRFacilityControlWidget::SetHubRouteReturnEmptyWhenNoCargo(FName RouteId, bool bReturnEmptyWhenNoCargo)
{
	if (RouteId.IsNone())
	{
		LastHubRouteStatus = TEXT("Return setting failed: invalid route.");
		RefreshControlText();
		return false;
	}

	USRSpaceLogisticsSubsystem* SpaceLogisticsSubsystem = GetSpaceLogisticsSubsystem();
	if (!IsValid(SpaceLogisticsSubsystem))
	{
		LastHubRouteStatus = TEXT("Return setting failed: logistics subsystem unavailable.");
		RefreshControlText();
		return false;
	}

	const bool bUpdated = SpaceLogisticsSubsystem->SetHubRouteReturnEmptyWhenNoCargo(RouteId, bReturnEmptyWhenNoCargo);
	LastHubRouteStatus = bUpdated
		? FString::Printf(TEXT("Empty return: %s"), bReturnEmptyWhenNoCargo ? TEXT("ON") : TEXT("OFF"))
		: FString::Printf(TEXT("Return setting failed: %s"), *RouteId.ToString());
	HubRoutePanelSignature.Reset();
	RefreshControlText();
	return bUpdated;
}

bool USRFacilityControlWidget::SetHubRouteCargoResourceId(FName RouteId, FName CargoResourceId)
{
	if (RouteId.IsNone())
	{
		LastHubRouteStatus = TEXT("Cargo filter failed: invalid route.");
		RefreshControlText();
		return false;
	}

	USRSpaceLogisticsSubsystem* SpaceLogisticsSubsystem = GetSpaceLogisticsSubsystem();
	if (!IsValid(SpaceLogisticsSubsystem))
	{
		LastHubRouteStatus = TEXT("Cargo filter failed: logistics subsystem unavailable.");
		RefreshControlText();
		return false;
	}

	const bool bUpdated = SpaceLogisticsSubsystem->SetHubRouteCargoResourceId(RouteId, CargoResourceId);
	LastHubRouteStatus = bUpdated
		? FString::Printf(TEXT("Cargo filter: %s"), CargoResourceId.IsNone() ? TEXT("Any") : *CargoResourceId.ToString())
		: FString::Printf(TEXT("Cargo filter failed: %s"), *RouteId.ToString());
	HubRoutePanelSignature.Reset();
	RefreshControlText();
	return bUpdated;
}

bool USRFacilityControlWidget::SetHubRouteProfile(
	FName RouteId,
	ESRSpaceLogisticsRouteProfileV2 RouteProfile)
{
	if (RouteId.IsNone())
	{
		LastHubRouteStatus = TEXT("Profile update failed: invalid route.");
		RefreshControlText();
		return false;
	}

	USRSpaceLogisticsSubsystem* SpaceLogisticsSubsystem = GetSpaceLogisticsSubsystem();
	if (!IsValid(SpaceLogisticsSubsystem))
	{
		LastHubRouteStatus = TEXT("Profile update failed: logistics subsystem unavailable.");
		RefreshControlText();
		return false;
	}

	const FSRSpaceLogisticsRouteProfileRulesV2 Rules =
		FSRFleetCapacityV2::GetRouteProfileRules(RouteProfile);
	if (!SpaceLogisticsSubsystem->IsHubRouteProfileUnlocked(Rules.Profile))
	{
		LastHubRouteStatus = FString::Printf(
			TEXT("Profile locked: select an Augment Package that grants %s first."),
			*Rules.DisplayName.ToString());
		HubRoutePanelSignature.Reset();
		RefreshControlText();
		return false;
	}
	const bool bUpdated = SpaceLogisticsSubsystem->SetHubRouteProfile(RouteId, Rules.Profile);
	LastHubRouteStatus = bUpdated
		? FString::Printf(
			TEXT("Route profile: %s (Cargo %d / Fleet Load %d)"),
			*Rules.DisplayName.ToString(),
			Rules.CargoCapacity,
			Rules.FleetLoad)
		: FString::Printf(
			TEXT("Profile update failed: dock the empty route first (%s)."),
			*RouteId.ToString());
	HubRoutePanelSignature.Reset();
	RefreshControlText();
	return bUpdated;
}

bool USRFacilityControlWidget::SetHubRouteConditionedTransitModule(
	FName RouteId,
	ESRConditionedTransitModuleV2 ConditionedTransitModule)
{
	if (RouteId.IsNone())
	{
		LastHubRouteStatus = TEXT("Hold module update failed: invalid route.");
		RefreshControlText();
		return false;
	}

	USRSpaceLogisticsSubsystem* SpaceLogisticsSubsystem = GetSpaceLogisticsSubsystem();
	if (!IsValid(SpaceLogisticsSubsystem))
	{
		LastHubRouteStatus = TEXT("Hold module update failed: logistics subsystem unavailable.");
		RefreshControlText();
		return false;
	}

	const FSRConditionedTransitModuleRulesV2 Rules =
		FSRConditionedTransitV2::GetModuleRules(ConditionedTransitModule);
	const bool bUpdated = SpaceLogisticsSubsystem->SetHubRouteConditionedTransitModule(
		RouteId,
		Rules.Module);
	LastHubRouteStatus = bUpdated
		? FString::Printf(
			TEXT("Conditioned Hold module: %s | %s"),
			*Rules.DisplayName.ToString(),
			*Rules.PreviewText.ToString())
		: FString::Printf(
			TEXT("Hold module update failed: unlock its Augment and dock the empty Conditioned Hold first (%s)."),
			*RouteId.ToString());
	HubRoutePanelSignature.Reset();
	RefreshControlText();
	return bUpdated;
}

USRFacilityNetworkComponent* USRFacilityControlWidget::GetFocusedFacilityNetwork() const
{
	AActor* Actor = FocusedActor.Get();
	return IsValid(Actor) ? Actor->FindComponentByClass<USRFacilityNetworkComponent>() : nullptr;
}

USRSpaceLogisticsSubsystem* USRFacilityControlWidget::GetSpaceLogisticsSubsystem() const
{
	UWorld* World = GetWorld();
	return IsValid(World)
		? World->GetSubsystem<USRSpaceLogisticsSubsystem>()
		: nullptr;
}

bool USRFacilityControlWidget::IsScreenPositionOverControlPanel(const FVector2D& ScreenPosition) const
{
	return IsVisible()
		&& PanelBorder
		&& PanelBorder->GetCachedGeometry().IsUnderLocation(ScreenPosition);
}

void USRFacilityControlWidget::HandleCloseClicked()
{
	SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl CloseButton OnClicked"));

	if (ASRPlayerController* PlayerController = Cast<ASRPlayerController>(GetOwningPlayer()))
	{
		PlayerController->ClearFacilityFocus();
		return;
	}

	ClearFocusedFacility();
}

void USRFacilityControlWidget::HandleProcessCheckStateChanged(bool bIsChecked)
{
	SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl ProcessCheckBox changed bIsChecked=%s"),
		bIsChecked ? TEXT("true") : TEXT("false"));

	if (bUpdatingControls)
	{
		return;
	}

	if (USRFacilityNetworkComponent* FacilityNetwork = GetFocusedFacilityNetwork())
	{
		FacilityNetwork->SetFacilityProcessEnabled(FocusedOccupantId, bIsChecked);
	}
	RefreshControlText();
}

void USRFacilityControlWidget::HandleCycleResourceV2RecipeClicked()
{
	SR_LOG(UIClickTrace, LogTemp, Log,
		TEXT("SR UI Click Trace: FacilityControl ResourceV2RecipeButton OnClicked"));

	if (USRFacilityNetworkComponent* FacilityNetwork = GetFocusedFacilityNetwork())
	{
		FacilityNetwork->CycleFacilityResourceV2Recipe(FocusedOccupantId);
	}
	RefreshControlText();
}

void USRFacilityControlWidget::HandleCycleOperationalPriorityClicked()
{
	FSRFacilityInstance FacilityInstance;
	USRFacilityNetworkComponent* FacilityNetwork = GetFocusedFacilityNetwork();
	if (!IsValid(FacilityNetwork)
		|| !FacilityNetwork->GetFacilityInstance(FocusedOccupantId, FacilityInstance))
	{
		return;
	}

	ESROperationalPriorityV2 NextPriority = ESROperationalPriorityV2::Normal;
	switch (FacilityInstance.OperationalPriority)
	{
	case ESROperationalPriorityV2::Normal:
		NextPriority = ESROperationalPriorityV2::Background;
		break;
	case ESROperationalPriorityV2::Background:
		NextPriority = ESROperationalPriorityV2::Critical;
		break;
	case ESROperationalPriorityV2::Critical:
	default:
		NextPriority = ESROperationalPriorityV2::Normal;
		break;
	}
	FacilityNetwork->SetFacilityOperationalPriority(FocusedOccupantId, NextPriority);
	RefreshControlText();
}

void USRFacilityControlWidget::HandleDeliverCheckStateChanged(bool bIsChecked)
{
	SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl DeliverCheckBox changed bIsChecked=%s"),
		bIsChecked ? TEXT("true") : TEXT("false"));

	if (bUpdatingControls)
	{
		return;
	}

	if (USRFacilityNetworkComponent* FacilityNetwork = GetFocusedFacilityNetwork())
	{
		FacilityNetwork->SetFacilityDeliverEnabled(FocusedOccupantId, bIsChecked);
	}
	RefreshControlText();
}

void USRFacilityControlWidget::HandleDebugAddTerriteClicked()
{
	SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl DebugAddTerriteButton OnClicked"));

	USRFacilityNetworkComponent* FacilityNetwork = GetFocusedFacilityNetwork();
	if (!IsValid(FacilityNetwork) || FocusedOccupantId.IsNone())
	{
		return;
	}

	FacilityNetwork->AddInputResource(
		FocusedOccupantId,
		MakeDebugEnergyResource(TEXT("Territe"), 1.0, 3));
	RefreshControlText();
}

void USRFacilityControlWidget::HandleDebugAddAquidClicked()
{
	SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl DebugAddAquidButton OnClicked"));

	USRFacilityNetworkComponent* FacilityNetwork = GetFocusedFacilityNetwork();
	if (!IsValid(FacilityNetwork) || FocusedOccupantId.IsNone())
	{
		return;
	}

	FacilityNetwork->AddInputResource(
		FocusedOccupantId,
		MakeDebugEnergyResource(TEXT("Aquid"), 0.0, 5));
	RefreshControlText();
}

void USRFacilityControlWidget::HandleDebugAddNitainClicked()
{
	SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl DebugAddNitainButton OnClicked"));

	USRFacilityNetworkComponent* FacilityNetwork = GetFocusedFacilityNetwork();
	if (!IsValid(FacilityNetwork) || FocusedOccupantId.IsNone())
	{
		return;
	}

	FacilityNetwork->AddInputResource(
		FocusedOccupantId,
		MakeDebugEnergyResource(TEXT("Nitain"), 3.0, 2));
	RefreshControlText();
}

void USRFacilityControlWidget::HandleDebugAddWasteClicked()
{
	SR_LOG(UIClickTrace, LogTemp, Log, TEXT("SR UI Click Trace: FacilityControl DebugAddWasteButton OnClicked"));

	USRFacilityNetworkComponent* FacilityNetwork = GetFocusedFacilityNetwork();
	if (!IsValid(FacilityNetwork) || FocusedOccupantId.IsNone())
	{
		return;
	}

	FacilityNetwork->AddInputResource(
		FocusedOccupantId,
		MakeDebugEnergyResource(TEXT("Waste"), 0.1, 3));
	RefreshControlText();
}

void USRFacilityControlWidget::BuildFacilityControlWidgetTree()
{
	if (!WidgetTree)
	{
		return;
	}

	if (WidgetTree->RootWidget)
	{
		PanelScaleBox = Cast<UScaleBox>(WidgetTree->FindWidget(FName(TEXT("FacilityControlPanelScaleBox"))));
		PanelDesignSizeBox = Cast<USizeBox>(WidgetTree->FindWidget(FName(TEXT("FacilityControlPanelDesignSizeBox"))));
		PanelBorder = Cast<UBorder>(WidgetTree->FindWidget(FName(TEXT("FacilityControlPanelBorder"))));
		TitleTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("FacilityControlTitleTextBlock"))));
		FacilityStatusBadge = Cast<USRStatusBadgeWidget>(WidgetTree->FindWidget(FName(TEXT("FacilityControlStatusBadge"))));
		CloseButton = Cast<UButton>(WidgetTree->FindWidget(FName(TEXT("FacilityControlCloseButton"))));
		ProcessCheckBox = Cast<UCheckBox>(WidgetTree->FindWidget(FName(TEXT("FacilityControlProcessCheckBox"))));
		ProcessStatusTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("FacilityControlProcessStatusTextBlock"))));
		OperationalPriorityButton = Cast<UButton>(WidgetTree->FindWidget(FName(TEXT("FacilityControlOperationalPriorityButton"))));
		OperationalPriorityTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("FacilityControlOperationalPriorityTextBlock"))));
		ResourceV2RecipeButton = Cast<UButton>(WidgetTree->FindWidget(FName(TEXT("FacilityControlResourceV2RecipeButton"))));
		ResourceV2RecipeTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("FacilityControlResourceV2RecipeTextBlock"))));
		InputResourceTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("FacilityControlInputResourceTextBlock"))));
		InputStageBadge = Cast<USRStatusBadgeWidget>(WidgetTree->FindWidget(FName(TEXT("FacilityControlInputStageBadge"))));
		InputResourceSlotBox = Cast<UHorizontalBox>(WidgetTree->FindWidget(FName(TEXT("FacilityControlInputResourceSlotBox"))));
		EffectsTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("FacilityControlEffectsTextBlock"))));
		ProcessStageBadge = Cast<USRStatusBadgeWidget>(WidgetTree->FindWidget(FName(TEXT("FacilityControlProcessStageBadge"))));
		EnergyTransitionTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("FacilityControlEnergyTransitionTextBlock"))));
		StateTransitionTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("FacilityControlStateTransitionTextBlock"))));
		ProcessProgressBar = Cast<UProgressBar>(WidgetTree->FindWidget(FName(TEXT("FacilityControlProcessProgressBar"))));
		ProcessTimeTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("FacilityControlProcessTimeTextBlock"))));
		OutputPreviewTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("FacilityControlOutputPreviewTextBlock"))));
		OutputStageBadge = Cast<USRStatusBadgeWidget>(WidgetTree->FindWidget(FName(TEXT("FacilityControlOutputStageBadge"))));
		InputToProcessArrowTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("FacilityControlInputToProcessArrowTextBlock"))));
		ProcessToOutputArrowTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("FacilityControlProcessToOutputArrowTextBlock"))));
		OutputResourceSlotBox = Cast<UHorizontalBox>(WidgetTree->FindWidget(FName(TEXT("FacilityControlOutputResourceSlotBox"))));
		InputInventoryTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("FacilityControlInputInventoryTextBlock"))));
		InputInventorySlotBox = Cast<UVerticalBox>(WidgetTree->FindWidget(FName(TEXT("FacilityControlInputInventorySlotBox"))));
		OutputInventoryTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("FacilityControlOutputInventoryTextBlock"))));
		OutputInventorySlotBox = Cast<UVerticalBox>(WidgetTree->FindWidget(FName(TEXT("FacilityControlOutputInventorySlotBox"))));
		DebugAddTerriteButton = Cast<UButton>(WidgetTree->FindWidget(FName(TEXT("FacilityControlDebugAddTerriteButton"))));
		DebugAddAquidButton = Cast<UButton>(WidgetTree->FindWidget(FName(TEXT("FacilityControlDebugAddAquidButton"))));
		DebugAddNitainButton = Cast<UButton>(WidgetTree->FindWidget(FName(TEXT("FacilityControlDebugAddNitainButton"))));
		DebugAddWasteButton = Cast<UButton>(WidgetTree->FindWidget(FName(TEXT("FacilityControlDebugAddWasteButton"))));
		DeliverCheckBox = Cast<UCheckBox>(WidgetTree->FindWidget(FName(TEXT("FacilityControlDeliverCheckBox"))));
		DeliverStatusTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("FacilityControlDeliverStatusTextBlock"))));
		HubRouteTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("FacilityControlHubRouteTextBlock"))));
		HubRoutePanelBorder = Cast<UBorder>(WidgetTree->FindWidget(FName(TEXT("FacilityControlHubRouteBorder"))));
		HubNetworkStatusBadge = Cast<USRStatusBadgeWidget>(WidgetTree->FindWidget(FName(TEXT("FacilityControlHubNetworkStatusBadge"))));
		HubFleetInfoCard = Cast<USRInfoCardWidget>(WidgetTree->FindWidget(FName(TEXT("FacilityControlHubFleetInfoCard"))));
		HubQueueInfoCard = Cast<USRInfoCardWidget>(WidgetTree->FindWidget(FName(TEXT("FacilityControlHubQueueInfoCard"))));
		HubMissileInfoCard = Cast<USRInfoCardWidget>(WidgetTree->FindWidget(FName(TEXT("FacilityControlHubMissileInfoCard"))));
		HubUtilityButtonBox = Cast<UHorizontalBox>(WidgetTree->FindWidget(FName(TEXT("FacilityControlHubUtilityButtonBox"))));
		HubDestinationButtonBox = Cast<UHorizontalBox>(WidgetTree->FindWidget(FName(TEXT("FacilityControlHubDestinationButtonBox"))));
		HubRouteStatusTextBlock = Cast<UTextBlock>(WidgetTree->FindWidget(FName(TEXT("FacilityControlHubRouteStatusTextBlock"))));
		if (PanelScaleBox && PanelDesignSizeBox && PanelBorder)
		{
			return;
		}

		// Configured Blueprint widgets can retain an older fixed-size root.
		// Replace that tree with the complete responsive native inspector so
		// runtime behavior does not depend on manually resaving every asset.
		WidgetTree->RootWidget = nullptr;
	}

	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("FacilityControlCanvasPanel"));
	WidgetTree->RootWidget = RootCanvas;
	RootCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	PanelScaleBox = WidgetTree->ConstructWidget<UScaleBox>(
		UScaleBox::StaticClass(),
		TEXT("FacilityControlPanelScaleBox"));
	PanelScaleBox->SetStretch(EStretch::ScaleToFit);
	PanelScaleBox->SetStretchDirection(EStretchDirection::DownOnly);
	if (UCanvasPanelSlot* PanelScaleCanvasSlot = RootCanvas->AddChildToCanvas(PanelScaleBox))
	{
		PanelScaleCanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		PanelScaleCanvasSlot->SetOffsets(FMargin(FSRUILayoutPolicy::DefaultSafeMargin));
	}

	PanelDesignSizeBox = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(),
		TEXT("FacilityControlPanelDesignSizeBox"));
	PanelDesignSizeBox->SetWidthOverride(920.0f);
	PanelDesignSizeBox->SetHeightOverride(660.0f);
	if (UScaleBoxSlot* DesignSlot = Cast<UScaleBoxSlot>(PanelScaleBox->AddChild(PanelDesignSizeBox)))
	{
		DesignSlot->SetHorizontalAlignment(HAlign_Center);
		DesignSlot->SetVerticalAlignment(VAlign_Center);
	}

	PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("FacilityControlPanelBorder"));
	PanelBorder->SetPadding(FMargin(USRUIThemeLibrary::ResolveSpacing(4)));
	PanelBorder->SetBrushColor(USRUIThemeLibrary::GetThemeSettings()->PanelColor);
	PanelDesignSizeBox->AddChild(PanelBorder);

	UCanvasPanel* PanelCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("FacilityControlPanelCanvas"));
	PanelCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PanelBorder->SetContent(PanelCanvas);

	TitleTextBlock = ConstructTextBlock(WidgetTree, TEXT("FacilityControlTitleTextBlock"), 18, FLinearColor::White);
	TitleTextBlock->SetJustification(ETextJustify::Center);
	AddWidgetToCanvas(
		PanelCanvas,
		ConstructSectionBorder(
			WidgetTree,
			TEXT("FacilityControlTitleBorder"),
			TitleTextBlock,
			USRUIThemeLibrary::ResolveStatePalette(ESRUIVisualState::Neutral).SurfaceColor,
			FMargin(12.0f, 8.0f)),
		FVector2D(16.0f, 12.0f),
		FVector2D(630.0f, 56.0f));

	FacilityStatusBadge = WidgetTree->ConstructWidget<USRStatusBadgeWidget>(
		USRStatusBadgeWidget::StaticClass(),
		TEXT("FacilityControlStatusBadge"));
	FacilityStatusBadge->SetBadge(
		NSLOCTEXT("StarRoversFacilityControl", "DefaultFacilityStatus", "STANDBY"),
		ESRUIVisualState::Neutral);
	AddWidgetToCanvas(
		PanelCanvas,
		FacilityStatusBadge,
		FVector2D(662.0f, 21.0f),
		FVector2D(170.0f, 38.0f));

	CloseButton = ConstructDebugInputButton(
		WidgetTree,
		TEXT("FacilityControlCloseButton"),
		NSLOCTEXT("StarRoversFacilityControl", "CloseButton", "X"));
	CloseButton->SetBackgroundColor(FLinearColor(0.28f, 0.075f, 0.070f, 0.95f));
	AddWidgetToCanvas(
		PanelCanvas,
		CloseButton,
		FVector2D(842.0f, 12.0f),
		FVector2D(40.0f, 56.0f));

	UHorizontalBox* ProcessRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("FacilityControlProcessRow"));
	ProcessCheckBox = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), TEXT("FacilityControlProcessCheckBox"));
	if (UHorizontalBoxSlot* CheckSlot = ProcessRow->AddChildToHorizontalBox(ProcessCheckBox))
	{
		CheckSlot->SetPadding(FMargin(0.0f, 3.0f, 10.0f, 0.0f));
		CheckSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}
	ProcessStatusTextBlock = ConstructTextBlock(WidgetTree, TEXT("FacilityControlProcessStatusTextBlock"), 15, FLinearColor(0.86f, 0.92f, 0.96f, 1.0f));
	ProcessRow->AddChildToHorizontalBox(ProcessStatusTextBlock);
	AddWidgetToCanvas(
		PanelCanvas,
		ConstructSectionBorder(WidgetTree, TEXT("FacilityControlProcessBorder"), ProcessRow, FLinearColor(0.07f, 0.085f, 0.105f, 0.98f), FMargin(14.0f)),
		FVector2D(308.0f, 82.0f),
		FVector2D(264.0f, 62.0f));

	OperationalPriorityButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(),
		TEXT("FacilityControlOperationalPriorityButton"));
	OperationalPriorityButton->SetBackgroundColor(FLinearColor(0.17f, 0.14f, 0.26f, 0.96f));
	OperationalPriorityTextBlock = ConstructTextBlock(
		WidgetTree,
		TEXT("FacilityControlOperationalPriorityTextBlock"),
		11,
		FLinearColor(0.92f, 0.88f, 1.0f, 1.0f));
	OperationalPriorityTextBlock->SetJustification(ETextJustify::Center);
	OperationalPriorityButton->AddChild(OperationalPriorityTextBlock);
	AddWidgetToCanvas(
		PanelCanvas,
		OperationalPriorityButton,
		FVector2D(18.0f, 82.0f),
		FVector2D(266.0f, 62.0f));

	ResourceV2RecipeButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(),
		TEXT("FacilityControlResourceV2RecipeButton"));
	ResourceV2RecipeButton->SetBackgroundColor(FLinearColor(0.10f, 0.24f, 0.30f, 0.96f));
	ResourceV2RecipeTextBlock = ConstructTextBlock(
		WidgetTree,
		TEXT("FacilityControlResourceV2RecipeTextBlock"),
		12,
		FLinearColor(0.82f, 0.96f, 1.0f, 1.0f));
	ResourceV2RecipeTextBlock->SetJustification(ETextJustify::Center);
	ResourceV2RecipeButton->AddChild(ResourceV2RecipeTextBlock);
	AddWidgetToCanvas(
		PanelCanvas,
		ResourceV2RecipeButton,
		FVector2D(596.0f, 82.0f),
		FVector2D(266.0f, 62.0f));

	UVerticalBox* InputResourceSectionBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("FacilityControlInputResourceSectionBox"));
	InputStageBadge = WidgetTree->ConstructWidget<USRStatusBadgeWidget>(
		USRStatusBadgeWidget::StaticClass(),
		TEXT("FacilityControlInputStageBadge"));
	InputStageBadge->SetBadge(
		NSLOCTEXT("StarRoversFacilityControl", "InputStageBadge", "1  INPUT"),
		ESRUIVisualState::Info);
	if (UVerticalBoxSlot* InputStageBadgeSlot = InputResourceSectionBox->AddChildToVerticalBox(InputStageBadge))
	{
		InputStageBadgeSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 5.0f));
		InputStageBadgeSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}
	InputResourceTextBlock = ConstructTextBlock(WidgetTree, TEXT("FacilityControlInputResourceTextBlock"), 13, FLinearColor(0.80f, 0.88f, 1.0f, 1.0f));
	if (UVerticalBoxSlot* InputResourceTitleSlot = InputResourceSectionBox->AddChildToVerticalBox(InputResourceTextBlock))
	{
		InputResourceTitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 7.0f));
		InputResourceTitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}
	UScrollBox* InputResourceScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("FacilityControlInputResourceScrollBox"));
	InputResourceScrollBox->SetOrientation(Orient_Horizontal);
	InputResourceSlotBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("FacilityControlInputResourceSlotBox"));
	InputResourceScrollBox->AddChild(InputResourceSlotBox);
	if (UVerticalBoxSlot* InputResourceSlotBoxSlot = InputResourceSectionBox->AddChildToVerticalBox(InputResourceScrollBox))
	{
		InputResourceSlotBoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	UVerticalBox* ProcessResourceSectionBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("FacilityControlProcessResourceSectionBox"));
	ProcessStageBadge = WidgetTree->ConstructWidget<USRStatusBadgeWidget>(
		USRStatusBadgeWidget::StaticClass(),
		TEXT("FacilityControlProcessStageBadge"));
	ProcessStageBadge->SetBadge(
		NSLOCTEXT("StarRoversFacilityControl", "ProcessStageBadge", "2  PROCESS"),
		ESRUIVisualState::Neutral);
	if (UVerticalBoxSlot* ProcessStageBadgeSlot = ProcessResourceSectionBox->AddChildToVerticalBox(ProcessStageBadge))
	{
		ProcessStageBadgeSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 5.0f));
		ProcessStageBadgeSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}
	EffectsTextBlock = ConstructTextBlock(WidgetTree, TEXT("FacilityControlEffectsTextBlock"), 10, FLinearColor(0.96f, 0.90f, 0.72f, 1.0f));
	if (UVerticalBoxSlot* RuleSlot = ProcessResourceSectionBox->AddChildToVerticalBox(EffectsTextBlock))
	{
		RuleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
		RuleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}
	EnergyTransitionTextBlock = ConstructTextBlock(
		WidgetTree,
		TEXT("FacilityControlEnergyTransitionTextBlock"),
		10,
		USRUIThemeLibrary::ResolveStatePalette(ESRUIVisualState::Positive).PrimaryTextColor);
	if (UVerticalBoxSlot* EnergySlot = ProcessResourceSectionBox->AddChildToVerticalBox(EnergyTransitionTextBlock))
	{
		EnergySlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 3.0f));
		EnergySlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}
	StateTransitionTextBlock = ConstructTextBlock(
		WidgetTree,
		TEXT("FacilityControlStateTransitionTextBlock"),
		9,
		USRUIThemeLibrary::ResolveStatePalette(ESRUIVisualState::Info).SecondaryTextColor);
	if (UVerticalBoxSlot* StateSlot = ProcessResourceSectionBox->AddChildToVerticalBox(StateTransitionTextBlock))
	{
		StateSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
		StateSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}
	UVerticalBox* OutputResourceSectionBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("FacilityControlOutputResourceSectionBox"));
	OutputStageBadge = WidgetTree->ConstructWidget<USRStatusBadgeWidget>(
		USRStatusBadgeWidget::StaticClass(),
		TEXT("FacilityControlOutputStageBadge"));
	OutputStageBadge->SetBadge(
		NSLOCTEXT("StarRoversFacilityControl", "OutputStageBadge", "3  OUTPUT"),
		ESRUIVisualState::Neutral);
	if (UVerticalBoxSlot* OutputStageBadgeSlot = OutputResourceSectionBox->AddChildToVerticalBox(OutputStageBadge))
	{
		OutputStageBadgeSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 5.0f));
		OutputStageBadgeSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}
	OutputPreviewTextBlock = ConstructTextBlock(WidgetTree, TEXT("FacilityControlOutputPreviewTextBlock"), 13, FLinearColor(0.78f, 1.0f, 0.86f, 1.0f));
	if (UVerticalBoxSlot* OutputResourceTitleSlot = OutputResourceSectionBox->AddChildToVerticalBox(OutputPreviewTextBlock))
	{
		OutputResourceTitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 7.0f));
		OutputResourceTitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}
	UScrollBox* OutputResourceScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("FacilityControlOutputResourceScrollBox"));
	OutputResourceScrollBox->SetOrientation(Orient_Horizontal);
	OutputResourceSlotBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("FacilityControlOutputResourceSlotBox"));
	OutputResourceScrollBox->AddChild(OutputResourceSlotBox);
	if (UVerticalBoxSlot* OutputResourceSlotBoxSlot = OutputResourceSectionBox->AddChildToVerticalBox(OutputResourceScrollBox))
	{
		OutputResourceSlotBoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	AddWidgetToCanvas(
		PanelCanvas,
		ConstructSectionBorder(WidgetTree, TEXT("FacilityControlInputResourceBorder"), InputResourceSectionBox),
		FVector2D(18.0f, 166.0f),
		FVector2D(266.0f, 190.0f));
	AddWidgetToCanvas(
		PanelCanvas,
		ConstructSectionBorder(WidgetTree, TEXT("FacilityControlEffectsBorder"), ProcessResourceSectionBox, FLinearColor(0.085f, 0.080f, 0.060f, 0.96f)),
		FVector2D(308.0f, 166.0f),
		FVector2D(264.0f, 190.0f));
	AddWidgetToCanvas(
		PanelCanvas,
		ConstructSectionBorder(WidgetTree, TEXT("FacilityControlOutputPreviewBorder"), OutputResourceSectionBox),
		FVector2D(596.0f, 166.0f),
		FVector2D(266.0f, 190.0f));

	InputToProcessArrowTextBlock = ConstructTextBlock(
		WidgetTree,
		TEXT("FacilityControlInputToProcessArrowTextBlock"),
		20,
		USRUIThemeLibrary::ResolveStatePalette(ESRUIVisualState::Info).AccentColor);
	InputToProcessArrowTextBlock->SetText(FText::FromString(TEXT(">")));
	InputToProcessArrowTextBlock->SetJustification(ETextJustify::Center);
	AddWidgetToCanvas(PanelCanvas, InputToProcessArrowTextBlock, FVector2D(284.0f, 232.0f), FVector2D(24.0f, 32.0f));

	ProcessToOutputArrowTextBlock = ConstructTextBlock(
		WidgetTree,
		TEXT("FacilityControlProcessToOutputArrowTextBlock"),
		20,
		USRUIThemeLibrary::ResolveStatePalette(ESRUIVisualState::Info).AccentColor);
	ProcessToOutputArrowTextBlock->SetText(FText::FromString(TEXT(">")));
	ProcessToOutputArrowTextBlock->SetJustification(ETextJustify::Center);
	AddWidgetToCanvas(PanelCanvas, ProcessToOutputArrowTextBlock, FVector2D(572.0f, 232.0f), FVector2D(24.0f, 32.0f));

	ProcessProgressBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("FacilityControlProcessProgressBar"));
	ProcessProgressBar->SetFillColorAndOpacity(FLinearColor(0.40f, 0.72f, 1.0f, 1.0f));
	UVerticalBox* ProgressBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("FacilityControlProgressBox"));
	if (UVerticalBoxSlot* ProgressSlot = ProgressBox->AddChildToVerticalBox(ProcessProgressBar))
	{
		ProgressSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
	}
	ProcessTimeTextBlock = ConstructTextBlock(WidgetTree, TEXT("FacilityControlProcessTimeTextBlock"), 12, FLinearColor(0.82f, 0.84f, 0.86f, 1.0f));
	ProcessTimeTextBlock->SetJustification(ETextJustify::Center);
	ProgressBox->AddChildToVerticalBox(ProcessTimeTextBlock);
	if (UVerticalBoxSlot* ProcessProgressSlot = ProcessResourceSectionBox->AddChildToVerticalBox(ProgressBox))
	{
		ProcessProgressSlot->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 0.0f));
		ProcessProgressSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	UVerticalBox* InputInventorySectionBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("FacilityControlInputInventorySectionBox"));
	InputInventoryTextBlock = ConstructTextBlock(WidgetTree, TEXT("FacilityControlInputInventoryTextBlock"), 12, FLinearColor(0.82f, 0.88f, 1.0f, 1.0f));
	if (UVerticalBoxSlot* InputInventoryTitleSlot = InputInventorySectionBox->AddChildToVerticalBox(InputInventoryTextBlock))
	{
		InputInventoryTitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 7.0f));
		InputInventoryTitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}
	UScrollBox* InputInventoryScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("FacilityControlInputInventoryScrollBox"));
	InputInventoryScrollBox->SetOrientation(Orient_Vertical);
	InputInventorySlotBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("FacilityControlInputInventorySlotBox"));
	InputInventoryScrollBox->AddChild(InputInventorySlotBox);
	if (UVerticalBoxSlot* InputInventorySlotsSlot = InputInventorySectionBox->AddChildToVerticalBox(InputInventoryScrollBox))
	{
		InputInventorySlotsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	UVerticalBox* OutputInventorySectionBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("FacilityControlOutputInventorySectionBox"));
	OutputInventoryTextBlock = ConstructTextBlock(WidgetTree, TEXT("FacilityControlOutputInventoryTextBlock"), 12, FLinearColor(0.82f, 1.0f, 0.88f, 1.0f));
	if (UVerticalBoxSlot* OutputInventoryTitleSlot = OutputInventorySectionBox->AddChildToVerticalBox(OutputInventoryTextBlock))
	{
		OutputInventoryTitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 7.0f));
		OutputInventoryTitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}
	UScrollBox* OutputInventoryScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("FacilityControlOutputInventoryScrollBox"));
	OutputInventoryScrollBox->SetOrientation(Orient_Vertical);
	OutputInventorySlotBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("FacilityControlOutputInventorySlotBox"));
	OutputInventoryScrollBox->AddChild(OutputInventorySlotBox);
	if (UVerticalBoxSlot* OutputInventorySlotsSlot = OutputInventorySectionBox->AddChildToVerticalBox(OutputInventoryScrollBox))
	{
		OutputInventorySlotsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	AddWidgetToCanvas(
		PanelCanvas,
		ConstructSectionBorder(WidgetTree, TEXT("FacilityControlInputInventoryBorder"), InputInventorySectionBox),
		FVector2D(18.0f, 386.0f),
		FVector2D(390.0f, 154.0f));
	AddWidgetToCanvas(
		PanelCanvas,
		ConstructSectionBorder(WidgetTree, TEXT("FacilityControlOutputInventoryBorder"), OutputInventorySectionBox),
		FVector2D(472.0f, 386.0f),
		FVector2D(390.0f, 154.0f));

	UVerticalBox* DebugInputBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("FacilityControlDebugInputBox"));
	UTextBlock* DebugInputLabelTextBlock = ConstructTextBlock(
		WidgetTree,
		TEXT("FacilityControlDebugInputLabelTextBlock"),
		11,
		FLinearColor(0.92f, 0.82f, 0.64f, 1.0f));
	DebugInputLabelTextBlock->SetText(NSLOCTEXT("StarRoversFacilityControl", "DebugInputLabel", "Debug Input"));
	if (UVerticalBoxSlot* DebugInputLabelSlot = DebugInputBox->AddChildToVerticalBox(DebugInputLabelTextBlock))
	{
		DebugInputLabelSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 5.0f));
	}

	UHorizontalBox* DebugInputRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("FacilityControlDebugInputRow"));

	DebugAddTerriteButton = ConstructDebugInputButton(
		WidgetTree,
		TEXT("FacilityControlDebugAddTerriteButton"),
		NSLOCTEXT("StarRoversFacilityControl", "DebugAddTerrite", "+ Territe"));
	if (UHorizontalBoxSlot* TerriteButtonSlot = DebugInputRow->AddChildToHorizontalBox(DebugAddTerriteButton))
	{
		TerriteButtonSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
		TerriteButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	DebugAddAquidButton = ConstructDebugInputButton(
		WidgetTree,
		TEXT("FacilityControlDebugAddAquidButton"),
		NSLOCTEXT("StarRoversFacilityControl", "DebugAddAquid", "+ Aquid"));
	if (UHorizontalBoxSlot* AquidButtonSlot = DebugInputRow->AddChildToHorizontalBox(DebugAddAquidButton))
	{
		AquidButtonSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
		AquidButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	DebugAddNitainButton = ConstructDebugInputButton(
		WidgetTree,
		TEXT("FacilityControlDebugAddNitainButton"),
		NSLOCTEXT("StarRoversFacilityControl", "DebugAddNitain", "+ Nitain"));
	if (UHorizontalBoxSlot* NitainButtonSlot = DebugInputRow->AddChildToHorizontalBox(DebugAddNitainButton))
	{
		NitainButtonSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
		NitainButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	DebugAddWasteButton = ConstructDebugInputButton(
		WidgetTree,
		TEXT("FacilityControlDebugAddWasteButton"),
		NSLOCTEXT("StarRoversFacilityControl", "DebugAddWaste", "+ Waste"));
	if (UHorizontalBoxSlot* WasteButtonSlot = DebugInputRow->AddChildToHorizontalBox(DebugAddWasteButton))
	{
		WasteButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	DebugInputBox->AddChildToVerticalBox(DebugInputRow);
	AddWidgetToCanvas(
		PanelCanvas,
		ConstructSectionBorder(
			WidgetTree,
			TEXT("FacilityControlDebugInputBorder"),
			DebugInputBox,
			FLinearColor(0.070f, 0.065f, 0.050f, 0.96f),
			FMargin(10.0f, 8.0f)),
		FVector2D(18.0f, 556.0f),
		FVector2D(390.0f, 52.0f));

	UHorizontalBox* DeliverRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("FacilityControlDeliverRow"));
	DeliverCheckBox = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), TEXT("FacilityControlDeliverCheckBox"));
	if (UHorizontalBoxSlot* DeliverCheckSlot = DeliverRow->AddChildToHorizontalBox(DeliverCheckBox))
	{
		DeliverCheckSlot->SetPadding(FMargin(0.0f, 3.0f, 10.0f, 0.0f));
		DeliverCheckSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}
	DeliverStatusTextBlock = ConstructTextBlock(WidgetTree, TEXT("FacilityControlDeliverStatusTextBlock"), 14, FLinearColor(0.86f, 0.92f, 0.96f, 1.0f));
	DeliverRow->AddChildToHorizontalBox(DeliverStatusTextBlock);
	AddWidgetToCanvas(
		PanelCanvas,
		ConstructSectionBorder(
			WidgetTree,
			TEXT("FacilityControlDeliverBorder"),
			DeliverRow,
			FLinearColor(0.060f, 0.085f, 0.070f, 0.96f),
			FMargin(14.0f)),
		FVector2D(554.0f, 552.0f),
		FVector2D(254.0f, 58.0f));

	UVerticalBox* HubRouteSectionBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("FacilityControlHubRouteSectionBox"));
	UHorizontalBox* HubHeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("FacilityControlHubHeaderRow"));
	HubNetworkStatusBadge = WidgetTree->ConstructWidget<USRStatusBadgeWidget>(
		USRStatusBadgeWidget::StaticClass(),
		TEXT("FacilityControlHubNetworkStatusBadge"));
	HubNetworkStatusBadge->SetBadge(
		NSLOCTEXT("StarRoversFacilityControl", "HubNetworkDefaultStatus", "LOGISTICS"),
		ESRUIVisualState::Neutral);
	if (UHorizontalBoxSlot* NetworkStatusSlot = HubHeaderRow->AddChildToHorizontalBox(HubNetworkStatusBadge))
	{
		NetworkStatusSlot->SetPadding(FMargin(0.0f, 0.0f, 10.0f, 0.0f));
		NetworkStatusSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}
	HubRouteTextBlock = ConstructTextBlock(
		WidgetTree,
		TEXT("FacilityControlHubRouteTextBlock"),
		12,
		FLinearColor(0.78f, 0.92f, 1.0f, 1.0f));
	HubRouteTextBlock->SetAutoWrapText(false);
	if (UHorizontalBoxSlot* HubTitleSlot = HubHeaderRow->AddChildToHorizontalBox(HubRouteTextBlock))
	{
		HubTitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	if (UVerticalBoxSlot* HubRouteTitleSlot = HubRouteSectionBox->AddChildToVerticalBox(HubHeaderRow))
	{
		HubRouteTitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
		HubRouteTitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}

	UHorizontalBox* HubMetricRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("FacilityControlHubMetricRow"));
	auto AddHubMetricCard = [this, HubMetricRow](TObjectPtr<USRInfoCardWidget>& OutCard, const TCHAR* WidgetName, const FText& Title)
	{
		OutCard = WidgetTree->ConstructWidget<USRInfoCardWidget>(USRInfoCardWidget::StaticClass(), FName(WidgetName));
		OutCard->SetCardData(Title, FText::FromString(TEXT("-")), FText::GetEmpty(), ESRUIVisualState::Neutral);
		USizeBox* MetricSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), NAME_None);
		MetricSizeBox->SetWidthOverride(214.0f);
		MetricSizeBox->SetHeightOverride(80.0f);
		MetricSizeBox->AddChild(OutCard);
		if (UHorizontalBoxSlot* MetricSlot = HubMetricRow->AddChildToHorizontalBox(MetricSizeBox))
		{
			MetricSlot->SetPadding(FMargin(0.0f, 0.0f, 7.0f, 0.0f));
			MetricSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}
	};
	AddHubMetricCard(
		HubFleetInfoCard,
		TEXT("FacilityControlHubFleetInfoCard"),
		NSLOCTEXT("StarRoversFacilityControl", "HubFleetMetricTitle", "FLEET LOAD"));
	AddHubMetricCard(
		HubQueueInfoCard,
		TEXT("FacilityControlHubQueueInfoCard"),
		NSLOCTEXT("StarRoversFacilityControl", "HubQueueMetricTitle", "DEPARTURE QUEUE"));
	AddHubMetricCard(
		HubMissileInfoCard,
		TEXT("FacilityControlHubMissileInfoCard"),
		NSLOCTEXT("StarRoversFacilityControl", "HubMissileMetricTitle", "MISSILES IN FLIGHT"));

	HubUtilityButtonBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("FacilityControlHubUtilityButtonBox"));
	USizeBox* HubUtilitySizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), NAME_None);
	HubUtilitySizeBox->SetWidthOverride(130.0f);
	HubUtilitySizeBox->SetHeightOverride(80.0f);
	HubUtilitySizeBox->AddChild(HubUtilityButtonBox);
	if (UHorizontalBoxSlot* UtilitySlot = HubMetricRow->AddChildToHorizontalBox(HubUtilitySizeBox))
	{
		UtilitySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		UtilitySlot->SetVerticalAlignment(VAlign_Center);
	}
	if (UVerticalBoxSlot* MetricsSlot = HubRouteSectionBox->AddChildToVerticalBox(HubMetricRow))
	{
		MetricsSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 7.0f));
		MetricsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}

	UScrollBox* HubDestinationScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("FacilityControlHubDestinationScrollBox"));
	HubDestinationScrollBox->SetOrientation(Orient_Horizontal);
	HubDestinationButtonBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("FacilityControlHubDestinationButtonBox"));
	HubDestinationScrollBox->AddChild(HubDestinationButtonBox);
	if (UVerticalBoxSlot* HubRouteButtonSlot = HubRouteSectionBox->AddChildToVerticalBox(HubDestinationScrollBox))
	{
		HubRouteButtonSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
		HubRouteButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	HubRouteStatusTextBlock = ConstructTextBlock(
		WidgetTree,
		TEXT("FacilityControlHubRouteStatusTextBlock"),
		11,
		FLinearColor(0.82f, 0.86f, 0.90f, 1.0f));
	HubRouteStatusTextBlock->SetAutoWrapText(false);
	HubRouteSectionBox->AddChildToVerticalBox(HubRouteStatusTextBlock);
	HubRoutePanelBorder = ConstructSectionBorder(
		WidgetTree,
		TEXT("FacilityControlHubRouteBorder"),
		HubRouteSectionBox,
		FLinearColor(0.050f, 0.072f, 0.090f, 0.96f),
		FMargin(10.0f, 8.0f));
	HubRoutePanelBorder->SetVisibility(ESlateVisibility::Collapsed);
	AddWidgetToCanvas(
		PanelCanvas,
		HubRoutePanelBorder,
		FVector2D(18.0f, 622.0f),
		FVector2D(844.0f, 364.0f));
}

void USRFacilityControlWidget::BindControlHandlers()
{
	if (CloseButton)
	{
		CloseButton->OnClicked.RemoveDynamic(this, &USRFacilityControlWidget::HandleCloseClicked);
		CloseButton->OnClicked.AddDynamic(this, &USRFacilityControlWidget::HandleCloseClicked);
	}
	if (ProcessCheckBox)
	{
		ProcessCheckBox->OnCheckStateChanged.RemoveDynamic(this, &USRFacilityControlWidget::HandleProcessCheckStateChanged);
		ProcessCheckBox->OnCheckStateChanged.AddDynamic(this, &USRFacilityControlWidget::HandleProcessCheckStateChanged);
	}
	if (ResourceV2RecipeButton)
	{
		ResourceV2RecipeButton->OnClicked.RemoveDynamic(this, &USRFacilityControlWidget::HandleCycleResourceV2RecipeClicked);
		ResourceV2RecipeButton->OnClicked.AddDynamic(this, &USRFacilityControlWidget::HandleCycleResourceV2RecipeClicked);
	}
	if (OperationalPriorityButton)
	{
		OperationalPriorityButton->OnClicked.RemoveDynamic(this, &USRFacilityControlWidget::HandleCycleOperationalPriorityClicked);
		OperationalPriorityButton->OnClicked.AddDynamic(this, &USRFacilityControlWidget::HandleCycleOperationalPriorityClicked);
	}
	if (DeliverCheckBox)
	{
		DeliverCheckBox->OnCheckStateChanged.RemoveDynamic(this, &USRFacilityControlWidget::HandleDeliverCheckStateChanged);
		DeliverCheckBox->OnCheckStateChanged.AddDynamic(this, &USRFacilityControlWidget::HandleDeliverCheckStateChanged);
	}
	if (DebugAddTerriteButton)
	{
		DebugAddTerriteButton->OnClicked.RemoveDynamic(this, &USRFacilityControlWidget::HandleDebugAddTerriteClicked);
		DebugAddTerriteButton->OnClicked.AddDynamic(this, &USRFacilityControlWidget::HandleDebugAddTerriteClicked);
	}
	if (DebugAddAquidButton)
	{
		DebugAddAquidButton->OnClicked.RemoveDynamic(this, &USRFacilityControlWidget::HandleDebugAddAquidClicked);
		DebugAddAquidButton->OnClicked.AddDynamic(this, &USRFacilityControlWidget::HandleDebugAddAquidClicked);
	}
	if (DebugAddNitainButton)
	{
		DebugAddNitainButton->OnClicked.RemoveDynamic(this, &USRFacilityControlWidget::HandleDebugAddNitainClicked);
		DebugAddNitainButton->OnClicked.AddDynamic(this, &USRFacilityControlWidget::HandleDebugAddNitainClicked);
	}
	if (DebugAddWasteButton)
	{
		DebugAddWasteButton->OnClicked.RemoveDynamic(this, &USRFacilityControlWidget::HandleDebugAddWasteClicked);
		DebugAddWasteButton->OnClicked.AddDynamic(this, &USRFacilityControlWidget::HandleDebugAddWasteClicked);
	}
}

void USRFacilityControlWidget::RefreshInputResourceSlots(USRFacilityNetworkComponent* FacilityNetwork, const FSRFacilityInstance& FacilityInstance)
{
	if (!InputResourceSlotBox)
	{
		return;
	}

	TArray<FSRResourceInstance> PreviewResources;
	FString EmptyText;
	int32 PreviewResourceCount = 0;
	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	const bool bIsMiningFacility = IsValid(FacilityDataAsset) && FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Mine;
	if (bIsMiningFacility)
	{
		FSRResourceDepositInstance MiningTarget;
		if (IsValid(FacilityNetwork)
			&& FacilityNetwork->GetFacilityMiningTarget(FacilityInstance.OccupantId, MiningTarget)
			&& IsValid(MiningTarget.ResourceDataAsset.Get()))
		{
			PreviewResources.Add(MiningTarget.ResourceDataAsset->BuildDefaultInstance());
		}
		PreviewResourceCount = PreviewResources.Num();
		EmptyText = BuildMiningTargetSummary(FacilityNetwork, FacilityInstance.OccupantId);
	}
	else
	{
		BuildNextInputPreviewResources(FacilityInstance, PreviewResources);
		PreviewResourceCount = PreviewResources.Num();
		EmptyText = TEXT("No queued resource");
	}

	if (InputResourceTextBlock)
	{
		InputResourceTextBlock->SetText(FText::FromString(FString::Printf(
			TEXT("Input Resource Preview (%d)"),
			PreviewResourceCount)));
	}

	FString NewSignature = FString::Printf(TEXT("InputPreview:%d:%s"), PreviewResources.Num(), *EmptyText);
	for (int32 PreviewIndex = 0; PreviewIndex < PreviewResources.Num(); ++PreviewIndex)
	{
		NewSignature += TEXT("|");
		NewSignature += BuildResourceSlotText(TEXT("Input"), PreviewIndex, NAME_None, &PreviewResources[PreviewIndex], TEXT("No Input"));
	}
	if (InputResourcePanelSignature == NewSignature)
	{
		return;
	}

	InputResourcePanelSignature = NewSignature;
	InputResourceSlotBox->ClearChildren();
	if (PreviewResources.IsEmpty())
	{
		AddInventoryInfoCard(
			WidgetTree,
			InputResourceSlotBox,
			EmptyText,
			FLinearColor(0.84f, 0.91f, 1.0f, 1.0f));
		return;
	}

	for (const FSRResourceInstance& PreviewResource : PreviewResources)
	{
		AddPreviewResourceCard(
			WidgetTree,
			InputResourceSlotBox,
			PreviewResource,
			FLinearColor(0.84f, 0.91f, 1.0f, 1.0f));
	}
}

void USRFacilityControlWidget::RefreshOutputResourceSlots(USRFacilityNetworkComponent* FacilityNetwork, const FSRFacilityInstance&)
{
	if (!OutputResourceSlotBox)
	{
		return;
	}

	TArray<FSRResourceInstance> PreviewOutputs;
	TArray<FString> PreviewOutputEnergyFormulas;
	if (IsValid(FacilityNetwork))
	{
		FSRResourceInstance PrimaryOutput;
		TArray<FSRResourceInstance> AdditionalOutputs;
		int32 OutputCount = 0;
		TArray<FString> EnergyFormulaTexts;
		if (FacilityNetwork->GetFacilityOutputPreview(
			FocusedOccupantId,
			PrimaryOutput,
			AdditionalOutputs,
			OutputCount,
			EnergyFormulaTexts))
		{
			const int32 PrimaryOutputCount = FMath::Max(0, OutputCount);
			PreviewOutputs.Reserve(PrimaryOutputCount + AdditionalOutputs.Num());
			PreviewOutputEnergyFormulas.Reserve(PrimaryOutputCount + AdditionalOutputs.Num());
			for (int32 OutputIndex = 0; OutputIndex < PrimaryOutputCount; ++OutputIndex)
			{
				PreviewOutputs.Add(PrimaryOutput);
				PreviewOutputEnergyFormulas.Add(EnergyFormulaTexts.IsValidIndex(OutputIndex)
					? EnergyFormulaTexts[OutputIndex]
					: FString());
			}
			const int32 AdditionalFormulaOffset = PrimaryOutputCount;
			PreviewOutputs.Append(AdditionalOutputs);
			for (int32 AdditionalOutputIndex = 0; AdditionalOutputIndex < AdditionalOutputs.Num(); ++AdditionalOutputIndex)
			{
				const int32 FormulaIndex = AdditionalFormulaOffset + AdditionalOutputIndex;
				PreviewOutputEnergyFormulas.Add(EnergyFormulaTexts.IsValidIndex(FormulaIndex)
					? EnergyFormulaTexts[FormulaIndex]
					: FString());
			}
		}
	}

	const int32 PreviewOutputCount = PreviewOutputs.Num();
	const FString EmptyText = TEXT("Process result unavailable");

	if (OutputPreviewTextBlock)
	{
		OutputPreviewTextBlock->SetText(FText::FromString(FString::Printf(
			TEXT("Output Resource Preview (%d)"),
			PreviewOutputCount)));
	}

	FString NewSignature = FString::Printf(TEXT("OutputPreview:%d:%s"), PreviewOutputs.Num(), *EmptyText);
	for (int32 PreviewIndex = 0; PreviewIndex < PreviewOutputs.Num(); ++PreviewIndex)
	{
		NewSignature += TEXT("|");
		NewSignature += BuildResourceSlotText(TEXT("Output"), PreviewIndex, NAME_None, &PreviewOutputs[PreviewIndex], TEXT("No Preview"));
		NewSignature += TEXT("|F:");
		NewSignature += PreviewOutputEnergyFormulas.IsValidIndex(PreviewIndex)
			? PreviewOutputEnergyFormulas[PreviewIndex]
			: FString();
	}
	if (OutputResourcePanelSignature == NewSignature)
	{
		return;
	}

	OutputResourcePanelSignature = NewSignature;
	OutputResourceSlotBox->ClearChildren();
	if (PreviewOutputs.IsEmpty())
	{
		AddInventoryInfoCard(
			WidgetTree,
			OutputResourceSlotBox,
			EmptyText,
			FLinearColor(0.84f, 1.0f, 0.90f, 1.0f));
		return;
	}

	for (int32 PreviewIndex = 0; PreviewIndex < PreviewOutputs.Num(); ++PreviewIndex)
	{
		const FSRResourceInstance& PreviewOutput = PreviewOutputs[PreviewIndex];
		AddPreviewResourceCard(
			WidgetTree,
			OutputResourceSlotBox,
			PreviewOutput,
			FLinearColor(0.84f, 1.0f, 0.90f, 1.0f));
		if (PreviewOutputEnergyFormulas.IsValidIndex(PreviewIndex))
		{
			AddEnergyFormulaCard(
				WidgetTree,
				OutputResourceSlotBox,
				PreviewOutputEnergyFormulas[PreviewIndex],
				FLinearColor(0.78f, 1.0f, 0.86f, 1.0f));
		}
	}
}

void USRFacilityControlWidget::RefreshInputInventorySlots(
	USRFacilityNetworkComponent* FacilityNetwork,
	const FSRFacilityInstance& FacilityInstance,
	bool bIsMiningFacility)
{
	if (!InputInventorySlotBox)
	{
		return;
	}

	const FString EmptyText = bIsMiningFacility
		? BuildMiningTargetSummary(FacilityNetwork, FacilityInstance.OccupantId)
		: TEXT("No input slots");
	FString NewSignature = bIsMiningFacility
		? FString::Printf(TEXT("InputMining:%s"), *EmptyText)
		: BuildInventoryPanelSignature(TEXT("Input"), FacilityInstance.InputPortInventories, EmptyText);
	if (!bIsMiningFacility)
	{
		NewSignature += FString::Printf(TEXT("|MissileSelect:%d"), bSelectingHubStarFuelMissileAutoLaunchSlot ? 1 : 0);
		for (const int32 InputPortIndex : FacilityInstance.StarFuelMissileAutoLaunchInputPortIndices)
		{
			NewSignature += FString::Printf(TEXT("|MissileAuto:%d"), InputPortIndex);
		}
	}
	if (InputInventoryPanelSignature == NewSignature)
	{
		return;
	}

	InputInventoryPanelSignature = NewSignature;
	InputInventorySlotBox->ClearChildren();
	InputSlotDebugActions.Reset();
	HubAutoMissileInventorySlotActions.Reset();
	if (bIsMiningFacility)
	{
		AddInventoryInfoCard(WidgetTree, InputInventorySlotBox, EmptyText, FLinearColor(0.82f, 0.88f, 1.0f, 1.0f));
		return;
	}

	if (FacilityInstance.InputPortInventories.IsEmpty())
	{
		AddInventoryInfoCard(WidgetTree, InputInventorySlotBox, EmptyText, FLinearColor(0.82f, 0.88f, 1.0f, 1.0f));
		return;
	}

	for (int32 SlotIndex = 0; SlotIndex < FacilityInstance.InputPortInventories.Num(); ++SlotIndex)
	{
		AddInputResourceSlotCard(
			WidgetTree,
			InputInventorySlotBox,
			this,
			InputSlotDebugActions,
			HubAutoMissileInventorySlotActions,
			FacilityInstance.InputPortInventories[SlotIndex],
			SlotIndex,
			TEXT("Input"),
			FLinearColor(0.82f, 0.88f, 1.0f, 1.0f),
			FacilityInstance.StarFuelMissileAutoLaunchInputPortIndices.Contains(SlotIndex),
			bSelectingHubStarFuelMissileAutoLaunchSlot);
	}
}

void USRFacilityControlWidget::RefreshOutputInventorySlots(const FSRFacilityInstance& FacilityInstance)
{
	if (!OutputInventorySlotBox)
	{
		return;
	}

	const FString EmptyText = TEXT("No output slots");
	const FString NewSignature = BuildInventoryPanelSignature(TEXT("Output"), FacilityInstance.OutputPortInventories, EmptyText);
	if (OutputInventoryPanelSignature == NewSignature)
	{
		return;
	}

	OutputInventoryPanelSignature = NewSignature;
	OutputInventorySlotBox->ClearChildren();
	if (FacilityInstance.OutputPortInventories.IsEmpty())
	{
		AddInventoryInfoCard(WidgetTree, OutputInventorySlotBox, EmptyText, FLinearColor(0.82f, 1.0f, 0.88f, 1.0f));
		return;
	}

	for (int32 SlotIndex = 0; SlotIndex < FacilityInstance.OutputPortInventories.Num(); ++SlotIndex)
	{
		AddInventorySlotCard(
			WidgetTree,
			OutputInventorySlotBox,
			FacilityInstance.OutputPortInventories[SlotIndex],
			SlotIndex,
			TEXT("Output"),
			FLinearColor(0.82f, 1.0f, 0.88f, 1.0f));
	}
}

void USRFacilityControlWidget::RefreshHubRouteSection(USRFacilityNetworkComponent* FacilityNetwork, const FSRFacilityInstance& FacilityInstance)
{
	if (!HubRouteTextBlock || !HubDestinationButtonBox || !HubRouteStatusTextBlock)
	{
		return;
	}
	auto SetHubLayoutActive = [this](bool bActive)
	{
		if (HubRoutePanelBorder)
		{
			HubRoutePanelBorder->SetVisibility(
				bActive ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		}
		if (PanelBorder)
		{
			if (UCanvasPanelSlot* PanelSlot = Cast<UCanvasPanelSlot>(PanelBorder->Slot))
			{
				PanelSlot->SetSize(bActive
					? FVector2D(920.0f, 1030.0f)
					: FVector2D(920.0f, 660.0f));
			}
		}
	};

	const bool bIsHubFacility = IsValid(FacilityNetwork) && FacilityNetwork->IsHubFacility(FacilityInstance.OccupantId);
	if (!bIsHubFacility)
	{
		SetHubLayoutActive(false);
		HubRouteTextBlock->SetText(NSLOCTEXT("StarRoversFacilityControl", "HubRoutesUnavailableTitle", "Hub Routes"));
		HubRouteStatusTextBlock->SetText(NSLOCTEXT("StarRoversFacilityControl", "HubRoutesUnavailableStatus", "Only Hub facilities can launch spaceships."));
		if (HubRoutePanelSignature != TEXT("NotHub"))
		{
			HubRoutePanelSignature = TEXT("NotHub");
			ClearHubRouteButtonsAndActions(
				HubDestinationButtonBox,
				HubRouteDestinationActions,
				HubRouteLaunchActions,
				HubRouteRemovalActions,
				HubRouteDebugOrbitActions,
				HubStarFuelMissileLaunchActions,
				HubRouteSettingActions);
		}
		if (HubUtilityButtonBox)
		{
			HubUtilityButtonBox->ClearChildren();
		}
		return;
	}
	SetHubLayoutActive(true);

	UWorld* World = GetWorld();
	const USRAugmentSubsystem* AugmentSubsystem = IsValid(World)
		? World->GetSubsystem<USRAugmentSubsystem>()
		: nullptr;
	USRSpaceLogisticsSubsystem* SpaceLogisticsSubsystem = IsValid(World)
		? World->GetSubsystem<USRSpaceLogisticsSubsystem>()
		: nullptr;
	if (!IsValid(SpaceLogisticsSubsystem))
	{
		if (HubNetworkStatusBadge)
		{
			HubNetworkStatusBadge->SetBadge(
				NSLOCTEXT("StarRoversFacilityControl", "HubLogisticsOffline", "LOGISTICS OFFLINE"),
				ESRUIVisualState::Danger);
		}
		HubRouteTextBlock->SetText(NSLOCTEXT("StarRoversFacilityControl", "HubRoutesNoSubsystemTitle", "Hub Routes"));
		HubRouteStatusTextBlock->SetText(NSLOCTEXT("StarRoversFacilityControl", "HubRoutesNoSubsystemStatus", "Logistics subsystem unavailable."));
		if (HubRoutePanelSignature != TEXT("NoSubsystem"))
		{
			HubRoutePanelSignature = TEXT("NoSubsystem");
			ClearHubRouteButtonsAndActions(
				HubDestinationButtonBox,
				HubRouteDestinationActions,
				HubRouteLaunchActions,
				HubRouteRemovalActions,
				HubRouteDebugOrbitActions,
				HubStarFuelMissileLaunchActions,
				HubRouteSettingActions);
		}
		if (HubUtilityButtonBox)
		{
			HubUtilityButtonBox->ClearChildren();
		}
		return;
	}

	FSRSpaceLogisticsHubEndpoint SourceHub;
	if (!SpaceLogisticsSubsystem->GetHubEndpoint(FocusedActor.Get(), FocusedOccupantId, SourceHub))
	{
		if (HubNetworkStatusBadge)
		{
			HubNetworkStatusBadge->SetBadge(
				NSLOCTEXT("StarRoversFacilityControl", "HubEndpointOffline", "ENDPOINT OFFLINE"),
				ESRUIVisualState::Danger);
		}
		HubRouteTextBlock->SetText(NSLOCTEXT("StarRoversFacilityControl", "HubRoutesNoEndpointTitle", "Hub Routes"));
		HubRouteStatusTextBlock->SetText(NSLOCTEXT("StarRoversFacilityControl", "HubRoutesNoEndpointStatus", "Source Hub endpoint not found."));
		if (HubRoutePanelSignature != TEXT("NoSourceEndpoint"))
		{
			HubRoutePanelSignature = TEXT("NoSourceEndpoint");
			ClearHubRouteButtonsAndActions(
				HubDestinationButtonBox,
				HubRouteDestinationActions,
				HubRouteLaunchActions,
				HubRouteRemovalActions,
				HubRouteDebugOrbitActions,
				HubStarFuelMissileLaunchActions,
				HubRouteSettingActions);
		}
		if (HubUtilityButtonBox)
		{
			HubUtilityButtonBox->ClearChildren();
		}
		return;
	}

	TArray<FSRSpaceLogisticsHubEndpoint> HubEndpoints;
	SpaceLogisticsSubsystem->GetHubEndpoints(HubEndpoints);
	HubEndpoints.Sort([World](
		const FSRSpaceLogisticsHubEndpoint& Left,
		const FSRSpaceLogisticsHubEndpoint& Right)
	{
		auto ResolveEndpointOrder = [World](const FSRSpaceLogisticsHubEndpoint& Endpoint)
		{
			if (USRCelestialBodyRuntimeLibrary::IsCelestialStarActor(Endpoint.BodyActor.Get()))
			{
				return 0;
			}
			bool bIsSatellite = false;
			const int32 OrbitOrdinal = ResolveStarOrbitOrdinal(Endpoint.BodyActor.Get(), World, bIsSatellite);
			return OrbitOrdinal == INDEX_NONE
				? MAX_int32
				: OrbitOrdinal * 2 + (bIsSatellite ? 1 : 0);
		};
		const int32 LeftOrder = ResolveEndpointOrder(Left);
		const int32 RightOrder = ResolveEndpointOrder(Right);
		if (LeftOrder != RightOrder)
		{
			return LeftOrder < RightOrder;
		}
		const FString LeftBodyName = BuildCelestialBodyDisplayName(Left.BodyActor.Get());
		const FString RightBodyName = BuildCelestialBodyDisplayName(Right.BodyActor.Get());
		const int32 BodyNameComparison = LeftBodyName.Compare(RightBodyName, ESearchCase::IgnoreCase);
		return BodyNameComparison != 0
			? BodyNameComparison < 0
			: Left.HubOccupantId.LexicalLess(Right.HubOccupantId);
	});

	TArray<FSRSpaceLogisticsHubRoute> HubRoutes;
	SpaceLogisticsSubsystem->GetHubRoutes(HubRoutes);
	const FSRFleetCapacityReportV2 FleetCapacityReport =
		SpaceLogisticsSubsystem->GetHubFleetCapacityReport(SourceHub);
	int32 UnlockedConditionedModuleMask = 0;
	TArray<ESRConditionedTransitModuleV2> ConditionedModules;
	FSRConditionedTransitV2::GetConditionedModules(ConditionedModules);
	for (const ESRConditionedTransitModuleV2 Module : ConditionedModules)
	{
		if (IsConditionedTransitModuleUnlocked(Module, AugmentSubsystem))
		{
			UnlockedConditionedModuleMask |= 1 << static_cast<uint8>(Module);
		}
	}

	TArray<FSRSpaceLogisticsStarFuelMissile> StarFuelMissiles;
	SpaceLogisticsSubsystem->GetStarFuelMissiles(StarFuelMissiles);

	ASRStar* PrimaryStar = ResolvePrimaryStarForHubUI(World);
	const int32 StarFuelMissileCargoStackCount = CountAvailableStarFuelMissileCargoStacks(FacilityInstance, PrimaryStar);
	const bool bCanConfigureStarFuelMissileAutoLaunch = IsValid(PrimaryStar) && !FacilityInstance.InputPortInventories.IsEmpty();
	const int32 ActiveMissileCount = CountActiveStarFuelMissilesForHub(StarFuelMissiles, SourceHub);

	TArray<FName> AvailableCargoResourceIds;
	TArray<int32> AutoLaunchInputPortIndices;
	if (IsValid(FacilityNetwork))
	{
		FacilityNetwork->GetHubOutboundCargoResourceIds(FocusedOccupantId, AvailableCargoResourceIds);
		FacilityNetwork->GetHubStarFuelMissileAutoLaunchInputPorts(FocusedOccupantId, AutoLaunchInputPortIndices);
	}

	int32 DestinationCount = 0;
	int32 ConnectedRouteCount = 0;
	for (const FSRSpaceLogisticsHubEndpoint& HubEndpoint : HubEndpoints)
	{
		if (!HubEndpoint.IsValid() || AreHubEndpointKeysEqual(HubEndpoint, SourceHub))
		{
			continue;
		}
		++DestinationCount;
	}

	for (const FSRSpaceLogisticsHubRoute& HubRoute : HubRoutes)
	{
		if (HubRoute.bDebugLocalOrbit)
		{
			continue;
		}

		if (AreHubEndpointKeysEqual(HubRoute.SourceHub, SourceHub) || AreHubEndpointKeysEqual(HubRoute.DestinationHub, SourceHub))
		{
			++ConnectedRouteCount;
		}
	}

	if (bHasSelectedHubRouteDestination)
	{
		bool bFoundSelectedDestination = false;
		for (const FSRSpaceLogisticsHubEndpoint& HubEndpoint : HubEndpoints)
		{
			if (!HubEndpoint.IsValid() || AreHubEndpointKeysEqual(HubEndpoint, SourceHub))
			{
				continue;
			}

			if (AreHubEndpointKeysEqual(HubEndpoint, SelectedHubRouteDestination))
			{
				SelectedHubRouteDestination = HubEndpoint;
				bFoundSelectedDestination = true;
				break;
			}
		}

		if (!bFoundSelectedDestination)
		{
			SelectedHubRouteDestination = FSRSpaceLogisticsHubEndpoint();
			bHasSelectedHubRouteDestination = false;
			LastHubRouteStatus.Reset();
		}
	}
	const FSRSpaceLogisticsHubRoute* SelectedConnectedRoute = nullptr;
	if (bHasSelectedHubRouteDestination)
	{
		for (const FSRSpaceLogisticsHubRoute& HubRoute : HubRoutes)
		{
			if (!HubRoute.bDebugLocalOrbit
				&& DoesHubRouteConnectEndpoints(HubRoute, SourceHub, SelectedHubRouteDestination))
			{
				SelectedConnectedRoute = &HubRoute;
				break;
			}
		}
	}

	FSRHubNetworkPresentationInput NetworkPresentationInput;
	NetworkPresentationInput.bLogisticsAvailable = true;
	NetworkPresentationInput.FleetCapacity = FleetCapacityReport;
	NetworkPresentationInput.DestinationCount = DestinationCount;
	NetworkPresentationInput.ConnectedRouteCount = ConnectedRouteCount;
	NetworkPresentationInput.ActiveMissileCount = ActiveMissileCount;
	NetworkPresentationInput.AutoLaunchSlotCount = AutoLaunchInputPortIndices.Num();
	const FSRHubNetworkPresentation NetworkPresentation =
		FSRHubRoutePresentationBuilder::BuildNetwork(NetworkPresentationInput);
	const FString SourceBodyName = BuildCelestialBodyDisplayName(SourceHub.BodyActor.Get());
	HubRouteTextBlock->SetText(FText::FromString(FString::Printf(
		TEXT("INTERBODY LOGISTICS  |  Source: %s  |  %d active lane%s"),
		*SourceBodyName,
		ConnectedRouteCount,
		ConnectedRouteCount == 1 ? TEXT("") : TEXT("s"))));
	if (HubNetworkStatusBadge)
	{
		HubNetworkStatusBadge->SetBadge(NetworkPresentation.StatusLabel, NetworkPresentation.VisualState);
	}
	if (HubFleetInfoCard)
	{
		HubFleetInfoCard->SetCardData(
			NSLOCTEXT("StarRoversFacilityControl", "HubFleetRuntimeTitle", "FLEET LOAD"),
			NetworkPresentation.FleetValue,
			NetworkPresentation.FleetDetail,
			NetworkPresentation.VisualState);
	}
	if (HubQueueInfoCard)
	{
		HubQueueInfoCard->SetCardData(
			NSLOCTEXT("StarRoversFacilityControl", "HubQueueRuntimeTitle", "DEPARTURE QUEUE"),
			NetworkPresentation.QueueValue,
			NetworkPresentation.QueueDetail,
			FleetCapacityReport.QueuedDepartureCount > 0
				? ESRUIVisualState::Warning
				: ESRUIVisualState::Info);
	}
	if (HubMissileInfoCard)
	{
		HubMissileInfoCard->SetCardData(
			NSLOCTEXT("StarRoversFacilityControl", "HubMissileRuntimeTitle", "MISSILES IN FLIGHT"),
			NetworkPresentation.MissileValue,
			NetworkPresentation.MissileDetail,
			ActiveMissileCount > 0 ? ESRUIVisualState::Positive : ESRUIVisualState::Neutral);
	}
	const FString StatusText = !LastHubRouteStatus.IsEmpty()
		? LastHubRouteStatus
		: (bSelectingHubStarFuelMissileAutoLaunchSlot
			? FString(TEXT("Select an input inventory slot to toggle missile auto-launch."))
			: (bHasSelectedHubRouteDestination
				? (SelectedConnectedRoute
					? FString::Printf(
						TEXT("Lane selected: %s. Route controls are available on the selected card."),
						*BuildCelestialBodyDisplayName(SelectedHubRouteDestination.BodyActor.Get()))
					: FString::Printf(
						TEXT("Destination selected: %s. Press Create Route."),
						*BuildCelestialBodyDisplayName(SelectedHubRouteDestination.BodyActor.Get())))
				: (FleetCapacityReport.bRulesActive && FleetCapacityReport.QueuedDepartureCount > 0
					? FString::Printf(
						TEXT("Fleet Capacity full: %d departure(s) queued; cargo remains in the Hub Export Buffer."),
						FleetCapacityReport.QueuedDepartureCount)
					: FString::Printf(
						TEXT("%s"),
						AutoLaunchInputPortIndices.Num() > 0
							? TEXT("Missile auto-launch linked. Cargo entering linked slots will launch.")
							: (DestinationCount > 0
								? TEXT("Select destination Hub or link missile auto-launch.")
								: TEXT("No destination Hub available."))))));
	HubRouteStatusTextBlock->SetText(FText::FromString(StatusText));

	FString NewSignature = FString::Printf(
		TEXT("Hub:%s:%s:%d:%d:%d:%d:%d:%d:Fleet:%d:%d:%d:%d:Modules:%d:%s:Selected:%s:%s"),
		*GetNameSafe(SourceHub.BodyActor.Get()),
		*SourceHub.HubOccupantId.ToString(),
		DestinationCount,
		ConnectedRouteCount,
		ActiveMissileCount,
		AutoLaunchInputPortIndices.Num(),
		bSelectingHubStarFuelMissileAutoLaunchSlot ? 1 : 0,
		StarFuelMissileCargoStackCount,
		FleetCapacityReport.ReservedLoad,
		FleetCapacityReport.TotalCapacity,
		FleetCapacityReport.QueuedDepartureCount,
		FleetCapacityReport.ActiveFleetBerthCount,
		UnlockedConditionedModuleMask,
		*StatusText,
		bHasSelectedHubRouteDestination ? *GetNameSafe(SelectedHubRouteDestination.BodyActor.Get()) : TEXT("None"),
		bHasSelectedHubRouteDestination ? *SelectedHubRouteDestination.HubOccupantId.ToString() : TEXT("None"));
	for (const int32 AutoLaunchInputPortIndex : AutoLaunchInputPortIndices)
	{
		NewSignature += FString::Printf(TEXT("|MissileAuto:%d"), AutoLaunchInputPortIndex);
	}
	for (const FSRSpaceLogisticsHubEndpoint& HubEndpoint : HubEndpoints)
	{
		if (!HubEndpoint.IsValid() || AreHubEndpointKeysEqual(HubEndpoint, SourceHub))
		{
			continue;
		}

		NewSignature += FString::Printf(
			TEXT("|Endpoint:%s:%s"),
			*GetNameSafe(HubEndpoint.BodyActor.Get()),
			*HubEndpoint.HubOccupantId.ToString());
	}
	for (const FName AvailableCargoResourceId : AvailableCargoResourceIds)
	{
		NewSignature += FString::Printf(TEXT("|CargoOption:%s"), *AvailableCargoResourceId.ToString());
	}
	for (const FSRSpaceLogisticsHubRoute& HubRoute : HubRoutes)
	{
		if (HubRoute.bDebugLocalOrbit)
		{
			continue;
		}

		if (!AreHubEndpointKeysEqual(HubRoute.SourceHub, SourceHub) && !AreHubEndpointKeysEqual(HubRoute.DestinationHub, SourceHub))
		{
			continue;
		}

		NewSignature += FString::Printf(
			TEXT("|Route:%s:%d:%d:%d:%d:%s:%d:%d:%lld:%d:%d:%d"),
			*HubRoute.RouteId.ToString(),
			static_cast<int32>(HubRoute.Phase),
			HubRoute.bDebugLocalOrbit ? 1 : 0,
			HubRoute.MaxCargoStackCount,
			HubRoute.bReturnEmptyWhenNoCargo ? 1 : 0,
			*HubRoute.CargoResourceId.ToString(),
			static_cast<int32>(HubRoute.RouteProfile),
			static_cast<int32>(HubRoute.ConditionedTransitModule),
			HubRoute.FleetDepartureQueueSequence,
			HubRoute.FleetQueuePosition,
			FMath::FloorToInt(HubRoute.ConditioningProgressSeconds),
			FMath::CeilToInt(HubRoute.ConditioningDurationSeconds));
		// Route cards are rebuilt in 5% travel increments. This keeps progress legible
		// without reallocating a multi-card hierarchy every frame.
		NewSignature += FString::Printf(
			TEXT(":Live:%d:%d:%d:%s:%d:%.1f"),
			HubRoute.bEnabled ? 1 : 0,
			static_cast<int32>(HubRoute.CurrentDockSide),
			FMath::FloorToInt(FMath::Clamp(HubRoute.TravelProgressRatio, 0.0f, 1.0f) * 20.0f),
			*HubRoute.Cargo.ResourceId.ToString(),
			HubRoute.Cargo.StackCount,
			ResolveDisplayedEnergy(HubRoute.Cargo));
	}

	if (HubRoutePanelSignature == NewSignature)
	{
		return;
	}

	HubRoutePanelSignature = NewSignature;
	ClearHubRouteButtonsAndActions(
		HubDestinationButtonBox,
		HubRouteDestinationActions,
		HubRouteLaunchActions,
		HubRouteRemovalActions,
		HubRouteDebugOrbitActions,
		HubStarFuelMissileLaunchActions,
		HubRouteSettingActions);
	if (HubUtilityButtonBox)
	{
		HubUtilityButtonBox->ClearChildren();
	}
	HubRouteDestinationActions.Reserve(DestinationCount);
	HubRouteLaunchActions.Reserve(bHasSelectedHubRouteDestination ? 1 : 0);
	HubRouteRemovalActions.Reserve(ConnectedRouteCount);
	HubStarFuelMissileLaunchActions.Reserve(1);
	HubRouteSettingActions.Reserve(ConnectedRouteCount * (AvailableCargoResourceIds.Num() + 5));

	AddHubStarFuelMissileLaunchButton(
		WidgetTree,
		HubUtilityButtonBox,
		this,
		HubStarFuelMissileLaunchActions,
		bCanConfigureStarFuelMissileAutoLaunch);

	for (const FSRSpaceLogisticsHubEndpoint& HubEndpoint : HubEndpoints)
	{
		if (!HubEndpoint.IsValid() || AreHubEndpointKeysEqual(HubEndpoint, SourceHub))
		{
			continue;
		}

		const FSRSpaceLogisticsHubRoute* ExistingRoute = nullptr;
		for (const FSRSpaceLogisticsHubRoute& HubRoute : HubRoutes)
		{
			if (HubRoute.bDebugLocalOrbit)
			{
				continue;
			}

			if (DoesHubRouteConnectEndpoints(HubRoute, SourceHub, HubEndpoint))
			{
				ExistingRoute = &HubRoute;
				break;
			}
		}

		const bool bSelectedDestination = bHasSelectedHubRouteDestination
			&& AreHubEndpointKeysEqual(HubEndpoint, SelectedHubRouteDestination);
		FSRHubRouteCardPresentationInput CardInput;
		CardInput.bHasRoute = ExistingRoute != nullptr;
		CardInput.bSelected = bSelectedDestination;
		if (ExistingRoute)
		{
			CardInput.bSelectedHubIsSource = AreHubEndpointKeysEqual(ExistingRoute->SourceHub, SourceHub);
			CardInput.SourceName = BuildHubEndpointUILabel(ExistingRoute->SourceHub);
			CardInput.DestinationName = BuildHubEndpointUILabel(ExistingRoute->DestinationHub);
			CardInput.RouteId = ExistingRoute->RouteId;
			CardInput.bEnabled = ExistingRoute->bEnabled;
			CardInput.bReturnEmptyWhenNoCargo = ExistingRoute->bReturnEmptyWhenNoCargo;
			CardInput.MaxCargoStackCount = ExistingRoute->MaxCargoStackCount;
			CardInput.CargoResourceId = ExistingRoute->CargoResourceId;
			CardInput.RouteProfile = ExistingRoute->RouteProfile;
			CardInput.ConditionedTransitModule = ExistingRoute->ConditionedTransitModule;
			CardInput.Phase = ExistingRoute->Phase;
			CardInput.CurrentDockSide = ExistingRoute->CurrentDockSide;
			CardInput.FleetQueuePosition = ExistingRoute->FleetQueuePosition;
			CardInput.TravelProgressSeconds = ExistingRoute->TravelProgressSeconds;
			CardInput.TravelDurationSeconds = ExistingRoute->TravelDurationSeconds;
			CardInput.TravelProgressRatio = ExistingRoute->TravelProgressRatio;
			CardInput.ConditioningProgressSeconds = ExistingRoute->ConditioningProgressSeconds;
			CardInput.ConditioningDurationSeconds = ExistingRoute->ConditioningDurationSeconds;
			CardInput.Cargo = ExistingRoute->Cargo;
		}
		else
		{
			CardInput.bSelectedHubIsSource = true;
			CardInput.SourceName = BuildHubEndpointUILabel(SourceHub);
			CardInput.DestinationName = BuildHubEndpointUILabel(HubEndpoint);
		}

		FSRHubRouteCardPresentation CardPresentation =
			FSRHubRoutePresentationBuilder::BuildRoute(CardInput);
		if (ExistingRoute
			&& ExistingRoute->RouteProfile == ESRSpaceLogisticsRouteProfileV2::ConditionedHold)
		{
			CardPresentation.ModuleDetail = FText::FromString(
				BuildConditionedTransitPreviewLabel(*ExistingRoute, AugmentSubsystem));
		}
		UHorizontalBox* RouteActionRow = AddHubRouteCard(
			WidgetTree,
			HubDestinationButtonBox,
			this,
			HubRouteDestinationActions,
			HubEndpoint,
			CardPresentation,
			bSelectedDestination);
		if (bSelectedDestination && !ExistingRoute)
		{
			AddHubRouteLaunchButton(
				WidgetTree,
				RouteActionRow,
				this,
				HubRouteLaunchActions,
				HubEndpoint,
				true);
		}
		if (ExistingRoute && bSelectedDestination && RouteActionRow)
		{
			const FSRSpaceLogisticsRouteProfileRulesV2 ExistingRules =
				FSRFleetCapacityV2::GetRouteProfileRules(ExistingRoute->RouteProfile);
			if (FleetCapacityReport.bRulesActive)
			{
				const ESRSpaceLogisticsRouteProfileV2 NextProfile =
					ResolveNextUnlockedRouteProfile(
						ExistingRules.Profile,
						SpaceLogisticsSubsystem);
				const bool bProfileChangeAllowed = ExistingRoute->Phase != ESRSpaceLogisticsHubRoutePhase::TravelingToDestination
					&& ExistingRoute->Phase != ESRSpaceLogisticsHubRoutePhase::TravelingToSource
					&& (ExistingRoute->Cargo.ResourceId.IsNone() || ExistingRoute->Cargo.StackCount <= 0)
					&& NextProfile != ExistingRules.Profile;
				AddHubRouteProfileButton(
					WidgetTree,
					RouteActionRow,
					this,
					HubRouteSettingActions,
					ExistingRoute->RouteId,
					NextProfile,
					bProfileChangeAllowed);
				if (ExistingRules.Profile == ESRSpaceLogisticsRouteProfileV2::ConditionedHold)
				{
					const ESRConditionedTransitModuleV2 NextModule =
						ResolveNextUnlockedConditionedTransitModule(
							ExistingRoute->ConditionedTransitModule,
							AugmentSubsystem);
					AddHubRouteConditionedModuleButton(
						WidgetTree,
						RouteActionRow,
						this,
						HubRouteSettingActions,
						ExistingRoute->RouteId,
						NextModule,
						bProfileChangeAllowed && NextModule != ExistingRoute->ConditionedTransitModule);
				}
			}
			const int32 ProfileCargoCapacity = FleetCapacityReport.bRulesActive
				? ExistingRules.CargoCapacity
				: 5;
			const int32 NextMaxCargoStackCount = ExistingRoute->MaxCargoStackCount < ProfileCargoCapacity
				? ProfileCargoCapacity
				: 1;
			AddHubRouteMaxCargoStackCountButton(
				WidgetTree,
				RouteActionRow,
				this,
				HubRouteSettingActions,
				ExistingRoute->RouteId,
				NextMaxCargoStackCount);
			AddHubRouteReturnEmptyButton(
				WidgetTree,
				RouteActionRow,
				this,
				HubRouteSettingActions,
				ExistingRoute->RouteId,
				!ExistingRoute->bReturnEmptyWhenNoCargo);
			if (!ExistingRoute->CargoResourceId.IsNone())
			{
				AddHubRouteCargoResourceButton(
					WidgetTree,
					RouteActionRow,
					this,
					HubRouteSettingActions,
					ExistingRoute->RouteId,
					NAME_None);
			}
			for (const FName AvailableCargoResourceId : AvailableCargoResourceIds)
			{
				if (AvailableCargoResourceId.IsNone() || AvailableCargoResourceId == ExistingRoute->CargoResourceId)
				{
					continue;
				}

				AddHubRouteCargoResourceButton(
					WidgetTree,
					RouteActionRow,
					this,
					HubRouteSettingActions,
					ExistingRoute->RouteId,
					AvailableCargoResourceId);
			}
			AddHubRouteRemoveButton(
				WidgetTree,
				RouteActionRow,
				this,
				HubRouteRemovalActions,
				ExistingRoute->RouteId);
		}
	}
}

void USRFacilityControlWidget::RefreshControlText()
{
	FSRFacilityInstance FacilityInstance;
	USRFacilityNetworkComponent* FacilityNetwork = GetFocusedFacilityNetwork();
	const bool bHasFacility = bHasFocusedFacility
		&& IsValid(FacilityNetwork)
		&& FacilityNetwork->GetFacilityInstance(FocusedOccupantId, FacilityInstance);

	if (!bHasFacility)
	{
		if (TitleTextBlock)
		{
			TitleTextBlock->SetText(NSLOCTEXT("StarRoversFacilityControl", "NoFacility", "No facility selected"));
		}
		if (InputResourceSlotBox)
		{
			InputResourceSlotBox->ClearChildren();
			InputResourcePanelSignature.Reset();
		}
		if (OutputResourceSlotBox)
		{
			OutputResourceSlotBox->ClearChildren();
			OutputResourcePanelSignature.Reset();
		}
		if (InputInventorySlotBox)
		{
			InputInventorySlotBox->ClearChildren();
			InputSlotDebugActions.Reset();
			HubAutoMissileInventorySlotActions.Reset();
			InputInventoryPanelSignature.Reset();
		}
		if (OutputInventorySlotBox)
		{
			OutputInventorySlotBox->ClearChildren();
			OutputInventoryPanelSignature.Reset();
		}
		if (HubDestinationButtonBox)
		{
			ClearHubRouteButtonsAndActions(
				HubDestinationButtonBox,
				HubRouteDestinationActions,
				HubRouteLaunchActions,
				HubRouteRemovalActions,
				HubRouteDebugOrbitActions,
				HubStarFuelMissileLaunchActions,
				HubRouteSettingActions);
			HubRoutePanelSignature.Reset();
		}
		if (HubRouteTextBlock)
		{
			HubRouteTextBlock->SetText(NSLOCTEXT("StarRoversFacilityControl", "HubRoutesNoFacility", "Hub Routes"));
		}
		if (HubRouteStatusTextBlock)
		{
			HubRouteStatusTextBlock->SetText(FText::GetEmpty());
		}
		if (ResourceV2RecipeButton)
		{
			ResourceV2RecipeButton->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (OperationalPriorityButton)
		{
			OperationalPriorityButton->SetVisibility(ESlateVisibility::Collapsed);
		}
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	const USRFacilityDataAsset* FacilityDataAsset = FacilityInstance.FacilityDataAsset.Get();
	FString FacilityName = FocusedOccupantId.ToString();
	if (IsValid(FacilityInstance.StructureDataAsset.Get()))
	{
		const FSRStructureData StructureData = FacilityInstance.StructureDataAsset->BuildData();
		if (!StructureData.DisplayName.IsEmpty())
		{
			FacilityName = StructureData.DisplayName.ToString();
		}
		else if (!StructureData.StructureId.IsNone())
		{
			FacilityName = StructureData.StructureId.ToString();
		}
	}
	const float ProcessSeconds = ResolveProcessSeconds(FacilityInstance);
	const float ProgressRatio = ProcessSeconds > 0.0f
		? FMath::Clamp(FacilityInstance.ProcessProgressSeconds / ProcessSeconds, 0.0f, 1.0f)
		: 0.0f;
	const FSRRefinementResistanceResultV2 RefinementResistance =
		FSRFacilityProcessingRuleEvaluator::ResolveRefinementResistance(FacilityInstance);
	const FString RefinementTimingStatus =
		FacilityInstance.bProcessing && FacilityInstance.bHasResolvedProcessSeconds
		? FString::Printf(TEXT("\nCycle: %.2fs [start snapshot]"), ProcessSeconds)
		: RefinementResistance.bApplied
			? FString::Printf(
			TEXT("\nRefinement: %.2fs base x %.2f = %.2fs (above Seed %.1f Energy)"),
			RefinementResistance.BaseProcessSeconds,
			RefinementResistance.CycleMultiplier,
			RefinementResistance.EffectiveProcessSeconds,
			RefinementResistance.RefinementEnergy)
			: FString();

	FString ProcessReason;
	bool bCanToggleProcess = CanToggleProcess(FacilityInstance, GetWorld(), ProcessReason);
	const bool bHasOutputConveyor = FacilityNetwork->HasConnectedConveyorForFacilityPort(FocusedOccupantId, ESRFacilityPortKind::Output);
	const bool bCanDebugAddInput = HasAvailableInputPortCapacity(FacilityInstance);
	FName SelectedRecipeId;
	TArray<FName> AvailableRecipeIds;
	FString RecipeFailureReason;
	const bool bHasRecipeSelector = FacilityNetwork->GetFacilityResourceV2RecipeState(
		FocusedOccupantId,
		SelectedRecipeId,
		AvailableRecipeIds,
		RecipeFailureReason);
	const bool bSelectedRecipeUnlocked = AvailableRecipeIds.Contains(SelectedRecipeId);
	const bool bRecipeChangeSafe = !FacilityInstance.bProcessing
		&& FacilityInstance.ProcessingInventory.IsEmpty();
	const bool bCanCycleRecipe = bHasRecipeSelector
		&& !AvailableRecipeIds.IsEmpty()
		&& bRecipeChangeSafe
		&& (!bSelectedRecipeUnlocked || AvailableRecipeIds.Num() > 1);
	const bool bIsMiningFacility = IsValid(FacilityDataAsset) && FacilityDataAsset->OperationKind == ESRFacilityOperationKind::Mine;
	const bool bShowsOperationalCapacity = UsesResourceV2Display()
		&& IsValid(FacilityDataAsset)
		&& FacilityDataAsset->FacilityKind == ESRFacilityKind::Standard;
	const FSROperationalCapacityReportV2 CapacityReport = FacilityNetwork->GetOperationalCapacityReport();
	FSRResourceDepositInstance MiningTarget;
	const bool bHasMiningTarget = bIsMiningFacility && FacilityNetwork->GetFacilityMiningTarget(FocusedOccupantId, MiningTarget);
	if (bIsMiningFacility && !bHasMiningTarget)
	{
		bCanToggleProcess = false;
		ProcessReason = TEXT("No adjacent deposit");
	}

	const FFacilityInspectorPreviewSnapshot InspectorPreview =
		BuildFacilityInspectorPreviewSnapshot(FacilityNetwork, FacilityInstance, ProgressRatio);
	const bool bUsesFinalFuelFormula = IsValid(FacilityDataAsset)
		&& FSRStellarFuelFabricator::ShouldRouteThroughResourceV2(FacilityDataAsset);
	FSRStellarFuelBatchStatusV2 StellarFuelBatchStatus;
	const bool bHasStellarFuelBatchStatus = bUsesFinalFuelFormula
		&& FSRStellarFuelBatchPlanner::TryBuildStatus(
			FacilityInstance,
			StarRovers::Resources::ResolveCelestialBodyResourceId(FacilityNetwork->GetOwner()),
			StellarFuelBatchStatus);
	FSRFacilityInspectorPresentationInput InspectorInput;
	InspectorInput.bProcessEnabled = FacilityInstance.bProcessEnabled;
	InspectorInput.bCanOperate = bCanToggleProcess;
	InspectorInput.bIsMiningFacility = bIsMiningFacility;
	InspectorInput.bProcessing = FacilityInstance.bProcessing;
	InspectorInput.bOutputBlocked = InspectorPreview.bOutputBlocked;
	InspectorInput.bPreviewResolved = InspectorPreview.bPreviewResolved;
	InspectorInput.bUsesFinalFuelFormula = bUsesFinalFuelFormula;
	InspectorInput.bUsesStellarFuelBatch = bHasStellarFuelBatchStatus;
	if (bHasStellarFuelBatchStatus)
	{
		InspectorInput.StellarFuelBatchState = StellarFuelBatchStatus.State;
		InspectorInput.StellarFuelValidCardCount = StellarFuelBatchStatus.ValidCardCount;
		InspectorInput.StellarFuelRequiredCardCount = StellarFuelBatchStatus.RequiredCardCount;
		InspectorInput.StellarFuelBatchSummary = StellarFuelBatchStatus.Summary;
		InspectorInput.StellarFuelBatchDetail = StellarFuelBatchStatus.Detail;
	}
	InspectorInput.InputResourceCount = InspectorPreview.Inputs.Num();
	InspectorInput.OutputResourceCount = InspectorPreview.Outputs.Num();
	InspectorInput.ProgressRatio = ProgressRatio;
	InspectorInput.ProcessSeconds = ProcessSeconds;
	InspectorInput.OperationalSpeedFactor = FacilityInstance.OperationalSpeedFactor;
	InspectorInput.OperationReason = ProcessReason;
	InspectorInput.ProcessRuleSummary = BuildConciseProcessRuleSummary(FacilityInstance);
	InspectorInput.FormulaSummary = FString::Join(InspectorPreview.EnergyFormulaTexts, TEXT("\n\n"));
	InspectorInput.StateTransitionSummary = BuildInspectorStateTransitionSummary(
		InspectorPreview.Inputs,
		InspectorPreview.Outputs,
		bUsesFinalFuelFormula);
	InspectorInput.bHasEnergyTransition = !InspectorPreview.Inputs.IsEmpty()
		&& !InspectorPreview.Outputs.IsEmpty();
	for (const FSRResourceInstance& InputResource : InspectorPreview.Inputs)
	{
		InspectorInput.InputEnergy += ResolveDisplayedEnergy(InputResource);
	}
	if (!InspectorPreview.Outputs.IsEmpty())
	{
		InspectorInput.OutputEnergy = ResolveDisplayedEnergy(InspectorPreview.Outputs[0]);
	}
	const FSRFacilityInspectorPresentation InspectorPresentation =
		FSRFacilityInspectorPresentationBuilder::Build(InspectorInput);

	bUpdatingControls = true;
	if (ProcessCheckBox)
	{
		ProcessCheckBox->SetIsChecked(FacilityInstance.bProcessEnabled);
		ProcessCheckBox->SetIsEnabled(bCanToggleProcess);
	}
	if (ResourceV2RecipeButton)
	{
		ResourceV2RecipeButton->SetVisibility(
			bHasRecipeSelector ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		ResourceV2RecipeButton->SetIsEnabled(bCanCycleRecipe);
		FString RecipeToolTip;
		if (bHasRecipeSelector)
		{
			RecipeToolTip = RecipeFailureReason.IsEmpty()
				? TEXT("Cycle through recipes unlocked by Technology and Augment Packages.")
				: RecipeFailureReason;
		}
		ResourceV2RecipeButton->SetToolTipText(FText::FromString(RecipeToolTip));
	}
	if (OperationalPriorityButton)
	{
		OperationalPriorityButton->SetVisibility(
			bShowsOperationalCapacity ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		OperationalPriorityButton->SetIsEnabled(
			bShowsOperationalCapacity && FacilityDataAsset->OperationalLoad > 0);
		OperationalPriorityButton->SetToolTipText(FText::FromString(
			TEXT("Cycle Normal -> Background -> Critical. Over-capacity slows the lower tiers proportionally; it never destroys or rejects work.")));
	}
	if (DeliverCheckBox)
	{
		DeliverCheckBox->SetIsChecked(FacilityInstance.bDeliverEnabled);
		DeliverCheckBox->SetIsEnabled(bHasOutputConveyor);
	}
	if (DebugAddTerriteButton)
	{
		DebugAddTerriteButton->SetIsEnabled(bCanDebugAddInput);
	}
	if (DebugAddAquidButton)
	{
		DebugAddAquidButton->SetIsEnabled(bCanDebugAddInput);
	}
	if (DebugAddNitainButton)
	{
		DebugAddNitainButton->SetIsEnabled(bCanDebugAddInput);
	}
	if (DebugAddWasteButton)
	{
		DebugAddWasteButton->SetIsEnabled(bCanDebugAddInput);
	}
	bUpdatingControls = false;

	if (TitleTextBlock)
	{
		TitleTextBlock->SetText(FText::FromString(FString::Printf(
			TEXT("%s\nOccupant: %s  Temp: %s"),
			*FacilityName,
			*FocusedOccupantId.ToString(),
			GetFacilityTemperatureLabel(FacilityInstance.TemperatureState))));
	}
	if (FacilityStatusBadge)
	{
		FacilityStatusBadge->SetBadge(
			InspectorPresentation.StatusLabel,
			InspectorPresentation.StatusVisualState);
		FacilityStatusBadge->SetToolTipText(InspectorPresentation.StatusDetail);
	}
	if (ProcessStatusTextBlock)
	{
		ProcessStatusTextBlock->SetText(FText::FromString(FString::Printf(
			TEXT("Automation: %s\n%s%s"),
			FacilityInstance.bProcessEnabled ? TEXT("ON") : TEXT("OFF"),
			*InspectorPresentation.StatusDetail.ToString(),
			*RefinementTimingStatus)));
	}
	if (ResourceV2RecipeTextBlock)
	{
		if (bHasRecipeSelector)
		{
			FString AccessLabel;
			if (bSelectedRecipeUnlocked)
			{
				AccessLabel = AvailableRecipeIds.Num() > 1
					? FString::Printf(TEXT("%d unlocked | click to cycle"), AvailableRecipeIds.Num())
					: TEXT("Technology/Package recipe ready");
			}
			else
			{
				AccessLabel = AvailableRecipeIds.IsEmpty()
					? TEXT("Locked | choose an Augment Package")
					: TEXT("Locked | click to select available recipe");
			}
			ResourceV2RecipeTextBlock->SetText(FText::FromString(FString::Printf(
				TEXT("Recipe: %s\n%s"),
				*GetResourceV2RecipeDisplayName(SelectedRecipeId),
				*AccessLabel)));
		}
		else
		{
			ResourceV2RecipeTextBlock->SetText(FText::GetEmpty());
		}
	}
	if (OperationalPriorityTextBlock && bShowsOperationalCapacity)
	{
		const FString PriorityLabel = StaticEnum<ESROperationalPriorityV2>()
			->GetDisplayNameTextByValue(static_cast<int64>(FacilityInstance.OperationalPriority))
			.ToString();
		OperationalPriorityTextBlock->SetText(FText::FromString(FString::Printf(
			TEXT("Priority: %s | Load %d | Speed %.0f%%\nBody: Demand %d / Capacity %d | Cores %d"),
			*PriorityLabel,
			FacilityDataAsset->OperationalLoad,
			FacilityInstance.OperationalSpeedFactor * 100.0f,
			CapacityReport.TotalDemand,
			CapacityReport.TotalCapacity,
			CapacityReport.ActiveServiceCoreCount)));
	}
	RefreshInputResourceSlots(FacilityNetwork, FacilityInstance);
	if (EffectsTextBlock)
	{
		EffectsTextBlock->SetText(InspectorPresentation.ProcessRule);
		EffectsTextBlock->SetToolTipText(FText::FromString(BuildEffectsSummary(FacilityInstance, GetWorld())));
	}
	if (EnergyTransitionTextBlock)
	{
		EnergyTransitionTextBlock->SetText(InspectorPresentation.EnergyTransition);
		EnergyTransitionTextBlock->SetToolTipText(FText::FromString(
			InspectorInput.FormulaSummary.IsEmpty()
				? InspectorPresentation.EnergyTransition.ToString()
				: InspectorInput.FormulaSummary));
	}
	if (StateTransitionTextBlock)
	{
		StateTransitionTextBlock->SetText(InspectorPresentation.StateTransition);
	}
	if (ProcessProgressBar)
	{
		ProcessProgressBar->SetPercent(ProgressRatio);
	}
	if (ProcessTimeTextBlock)
	{
		ProcessTimeTextBlock->SetText(FText::FromString(FString::Printf(
			TEXT("Process Time: %.2f / %.2f sec"),
			FacilityInstance.ProcessProgressSeconds,
			ProcessSeconds)));
	}
	RefreshOutputResourceSlots(FacilityNetwork, FacilityInstance);
	if (InputStageBadge)
	{
		InputStageBadge->SetBadge(
			NSLOCTEXT("StarRoversFacilityControl", "InputStageRuntimeBadge", "1  INPUT"),
			InspectorPresentation.InputVisualState);
	}
	if (ProcessStageBadge)
	{
		ProcessStageBadge->SetBadge(
			FText::Format(
				NSLOCTEXT("StarRoversFacilityControl", "ProcessStageRuntimeBadge", "2  PROCESS  |  {0}"),
				InspectorPresentation.StatusLabel),
			InspectorPresentation.ProcessVisualState);
	}
	if (OutputStageBadge)
	{
		OutputStageBadge->SetBadge(
			NSLOCTEXT("StarRoversFacilityControl", "OutputStageRuntimeBadge", "3  OUTPUT"),
			InspectorPresentation.OutputVisualState);
	}
	if (InputResourceTextBlock)
	{
		InputResourceTextBlock->SetText(InspectorPresentation.InputCaption);
	}
	if (OutputPreviewTextBlock)
	{
		OutputPreviewTextBlock->SetText(InspectorPresentation.OutputCaption);
	}
	if (InputInventoryTextBlock)
	{
		InputInventoryTextBlock->SetText(FText::FromString(
			bIsMiningFacility
				? TEXT("Input Inventory (Mining)")
				: FString::Printf(TEXT("Input Inventory (%d slots)"), FacilityInstance.InputPortInventories.Num())));
	}
	RefreshInputInventorySlots(FacilityNetwork, FacilityInstance, bIsMiningFacility);
	if (OutputInventoryTextBlock)
	{
		OutputInventoryTextBlock->SetText(FText::FromString(FString::Printf(
			TEXT("Output Inventory (%d slots)"),
			FacilityInstance.OutputPortInventories.Num())));
	}
	RefreshOutputInventorySlots(FacilityInstance);
	if (DeliverStatusTextBlock)
	{
		DeliverStatusTextBlock->SetText(FText::FromString(FString::Printf(
			TEXT("Deliver: %s  %s"),
			FacilityInstance.bDeliverEnabled ? TEXT("ON") : TEXT("OFF"),
			bHasOutputConveyor ? TEXT("Output conveyor connected") : TEXT("No output conveyor"))));
	}
	RefreshHubRouteSection(FacilityNetwork, FacilityInstance);
}
