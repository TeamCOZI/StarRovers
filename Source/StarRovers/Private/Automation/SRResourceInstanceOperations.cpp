#include "Automation/SRResourceInstanceOperations.h"

#include "Automation/SRResourceSystemContent.h"
#include "Celestial/SRCelestialBodyRuntimeLibrary.h"
#include "GameFramework/Actor.h"
#include "Simulation/SRSimulationSettings.h"

namespace
{
	int32 GetAllowedFamilyStateMask(ESRResourceFamily Family)
	{
		using StarRovers::Resources::GetFamilyStateBit;
		switch (Family)
		{
		case ESRResourceFamily::Metal:
			return GetFamilyStateBit(ESRResourceFamilyState::Tempered)
				| GetFamilyStateBit(ESRResourceFamilyState::Fatigued);
		case ESRResourceFamily::Crystal:
			return GetFamilyStateBit(ESRResourceFamilyState::Resonant)
				| GetFamilyStateBit(ESRResourceFamilyState::Fractured);
		case ESRResourceFamily::Organic:
			return GetFamilyStateBit(ESRResourceFamilyState::Matured)
				| GetFamilyStateBit(ESRResourceFamilyState::Depleted);
		case ESRResourceFamily::Plasma:
			return GetFamilyStateBit(ESRResourceFamilyState::Energized)
				| GetFamilyStateBit(ESRResourceFamilyState::Overloaded);
		case ESRResourceFamily::Void:
			return GetFamilyStateBit(ESRResourceFamilyState::Echoing)
				| GetFamilyStateBit(ESRResourceFamilyState::Collapsed);
		case ESRResourceFamily::None:
		default:
			return 0;
		}
	}

	void PopulateDefinitionFieldsIfAvailable(FSRResourceInstance& ResourceInstance)
	{
		const USRResourceDataAsset* ResourceDataAsset = ResourceInstance.ResourceDataAsset.Get();
		if (!IsValid(ResourceDataAsset)
			|| ResourceDataAsset->ResourceDefinitionVersion < StarRovers::Resources::CurrentResourceDefinitionVersion)
		{
			return;
		}

		if (ResourceInstance.ResourceId.IsNone())
		{
			ResourceInstance.ResourceId = ResourceDataAsset->ResourceId;
		}
		if (ResourceInstance.ResourceClass == ESRResourceClass::Unknown)
		{
			ResourceInstance.ResourceClass = ResourceDataAsset->ResourceClass;
		}
		if (ResourceInstance.Family == ESRResourceFamily::None)
		{
			ResourceInstance.Family = ResourceDataAsset->Family;
		}
		if (ResourceInstance.Spectrum == ESRResourceSpectrum::None)
		{
			ResourceInstance.Spectrum = ResourceDataAsset->NativeSpectrum;
		}
	}

	void PopulateDefinitionFieldsForLegacyPayload(FSRResourceInstance& ResourceInstance)
	{
		const USRResourceDataAsset* ResourceDataAsset = ResourceInstance.ResourceDataAsset.Get();
		if (!IsValid(ResourceDataAsset)
			|| ResourceDataAsset->ResourceDefinitionVersion < StarRovers::Resources::CurrentResourceDefinitionVersion)
		{
			return;
		}

		ResourceInstance.ResourceClass = ResourceDataAsset->ResourceClass;
		ResourceInstance.Family = ResourceDataAsset->Family;
		ResourceInstance.Spectrum = ResourceDataAsset->NativeSpectrum;
		ResourceInstance.Grade = ResourceDataAsset->NativeGrade;
	}

	void NormalizeProcessTagSlot(FSRResourceProcessTagSlot& ProcessTagSlot)
	{
		ProcessTagSlot.RemainingTriggers = FMath::Max(0, ProcessTagSlot.RemainingTriggers);
		if (ProcessTagSlot.TagId.IsNone() || ProcessTagSlot.Lifecycle == ESRResourceSlotLifecycle::Empty)
		{
			ProcessTagSlot.TagId = NAME_None;
			ProcessTagSlot.Lifecycle = ESRResourceSlotLifecycle::Empty;
			ProcessTagSlot.RemainingTriggers = 0;
		}
		else if (ProcessTagSlot.Lifecycle == ESRResourceSlotLifecycle::Spent)
		{
			ProcessTagSlot.RemainingTriggers = 0;
		}
		else if (ProcessTagSlot.RemainingTriggers == 0)
		{
			ProcessTagSlot.Lifecycle = ESRResourceSlotLifecycle::Spent;
		}
	}

	void NormalizeResourceV2Fields(FSRResourceInstance& ResourceInstance)
	{
		ResourceInstance.ResourceSchemaVersion = StarRovers::Resources::CurrentResourceSchemaVersion;
		ResourceInstance.Grade = FMath::Clamp(
			ResourceInstance.Grade,
			StarRovers::Resources::MinimumGrade,
			StarRovers::Resources::MaximumGrade);
		ResourceInstance.ProcessingMemory.ConsecutiveSameArchetypeCount =
			FMath::Max(0, ResourceInstance.ProcessingMemory.ConsecutiveSameArchetypeCount);
		ResourceInstance.ProcessingMemory.ConsecutiveSameFamilyActionCount =
			FMath::Max(0, ResourceInstance.ProcessingMemory.ConsecutiveSameFamilyActionCount);
		ResourceInstance.ProcessingMemory.GeneralProcessesSinceReset =
			FMath::Max(0, ResourceInstance.ProcessingMemory.GeneralProcessesSinceReset);
		ResourceInstance.ProcessingMemory.StoredFamilyMagnitude =
			FMath::Max(0.0, ResourceInstance.ProcessingMemory.StoredFamilyMagnitude);
		ResourceInstance.ProcessingMemory.TransitCountAtLastEnergyChange =
			FMath::Max(0, ResourceInstance.ProcessingMemory.TransitCountAtLastEnergyChange);
		ResourceInstance.ProcessingMemory.ProcessCount =
			FMath::Max(0, ResourceInstance.ProcessingMemory.ProcessCount);
		ResourceInstance.ProcessingMemory.EnergyChangeCount =
			FMath::Max(0, ResourceInstance.ProcessingMemory.EnergyChangeCount);
		ResourceInstance.LogisticsMetadata.TransitCount =
			FMath::Max(0, ResourceInstance.LogisticsMetadata.TransitCount);
		NormalizeProcessTagSlot(ResourceInstance.ProcessTagSlot);

		if (ResourceInstance.ResourceClass != ESRResourceClass::Card)
		{
			ResourceInstance.Family = ESRResourceFamily::None;
			ResourceInstance.Spectrum = ESRResourceSpectrum::None;
			ResourceInstance.Grade = StarRovers::Resources::MinimumGrade;
			ResourceInstance.ActiveFamilyStateFlags = 0;
			ResourceInstance.ProcessTagSlot = FSRResourceProcessTagSlot();
			ResourceInstance.FuelImprintSlot = FSRResourceFuelImprintSlot();
		}
		else
		{
			ResourceInstance.ActiveFamilyStateFlags &= GetAllowedFamilyStateMask(ResourceInstance.Family);
		}
		StarRovers::Resources::EnsureResourceSeedEnergySnapshot(ResourceInstance);
	}

	bool AreProcessingMemoriesEquivalent(
		const FSRResourceProcessingMemory& Left,
		const FSRResourceProcessingMemory& Right)
	{
		return Left.LastProcessArchetype == Right.LastProcessArchetype
			&& Left.LastTemperature == Right.LastTemperature
			&& Left.LastFamilyAction == Right.LastFamilyAction
			&& Left.ConsecutiveSameArchetypeCount == Right.ConsecutiveSameArchetypeCount
			&& Left.ConsecutiveSameFamilyActionCount == Right.ConsecutiveSameFamilyActionCount
			&& Left.GeneralProcessesSinceReset == Right.GeneralProcessesSinceReset
			&& FMath::IsNearlyEqual(Left.StoredFamilyMagnitude, Right.StoredFamilyMagnitude)
			&& Left.TransitCountAtLastEnergyChange == Right.TransitCountAtLastEnergyChange
			&& Left.ProcessCount == Right.ProcessCount
			&& Left.EnergyChangeCount == Right.EnergyChangeCount;
	}

	bool AreLogisticsMetadataEquivalent(
		const FSRResourceLogisticsMetadata& Left,
		const FSRResourceLogisticsMetadata& Right)
	{
		return Left.OriginBodyId == Right.OriginBodyId
			&& Left.LastProcessedBodyId == Right.LastProcessedBodyId
			&& Left.LastTransitSourceBodyId == Right.LastTransitSourceBodyId
			&& Left.LastTransitDestinationBodyId == Right.LastTransitDestinationBodyId
			&& Left.TransitCount == Right.TransitCount
			&& Left.bHasBeenProcessedOutsideOrigin == Right.bHasBeenProcessedOutsideOrigin;
	}
}

int32 StarRovers::Resources::GetFamilyStateBit(ESRResourceFamilyState FamilyState)
{
	return 1 << static_cast<uint8>(FamilyState);
}

FName StarRovers::Resources::ResolveCelestialBodyResourceId(const AActor* BodyActor)
{
	if (!IsValid(BodyActor))
	{
		return NAME_None;
	}

	const FString VariableName = USRCelestialBodyRuntimeLibrary::GetCelestialVariableName(BodyActor).ToString();
	return VariableName.IsEmpty() ? BodyActor->GetFName() : FName(*VariableName);
}

void StarRovers::Resources::InitializeResourceOrigin(FSRResourceInstance& ResourceInstance, FName BodyId)
{
	if (ResourceInstance.LogisticsMetadata.OriginBodyId.IsNone() && !BodyId.IsNone())
	{
		ResourceInstance.LogisticsMetadata.OriginBodyId = BodyId;
	}
}

void StarRovers::Resources::RecordResourceProcessedOnBody(FSRResourceInstance& ResourceInstance, FName BodyId)
{
	if (BodyId.IsNone())
	{
		return;
	}

	InitializeResourceOrigin(ResourceInstance, BodyId);
	ResourceInstance.LogisticsMetadata.LastProcessedBodyId = BodyId;
	if (ResourceInstance.LogisticsMetadata.OriginBodyId != BodyId)
	{
		ResourceInstance.LogisticsMetadata.bHasBeenProcessedOutsideOrigin = true;
	}
}

void StarRovers::Resources::RecordResourceTransit(
	FSRResourceInstance& ResourceInstance,
	FName SourceBodyId,
	FName DestinationBodyId)
{
	if (SourceBodyId.IsNone() || DestinationBodyId.IsNone())
	{
		return;
	}

	InitializeResourceOrigin(ResourceInstance, SourceBodyId);
	ResourceInstance.LogisticsMetadata.LastTransitSourceBodyId = SourceBodyId;
	ResourceInstance.LogisticsMetadata.LastTransitDestinationBodyId = DestinationBodyId;
	ResourceInstance.LogisticsMetadata.TransitCount =
		FMath::Max(0, ResourceInstance.LogisticsMetadata.TransitCount);
	if (ResourceInstance.LogisticsMetadata.TransitCount < MAX_int32)
	{
		++ResourceInstance.LogisticsMetadata.TransitCount;
	}
}

bool StarRovers::Resources::TryResolveResourceSeedEnergy(
	const FSRResourceInstance& ResourceInstance,
	double& OutSeedEnergy)
{
	OutSeedEnergy = 0.0;
	if (ResourceInstance.ResourceClass != ESRResourceClass::Card)
	{
		return false;
	}
	if (ResourceInstance.bHasSeedEnergySnapshot
		&& FMath::IsFinite(ResourceInstance.SeedEnergySnapshot))
	{
		OutSeedEnergy = FMath::Max(0.0, ResourceInstance.SeedEnergySnapshot);
		return true;
	}

	const USRResourceDataAsset* ResourceDataAsset = ResourceInstance.ResourceDataAsset.Get();
	if (IsValid(ResourceDataAsset)
		&& ResourceDataAsset->ResourceDefinitionVersion >= CurrentResourceDefinitionVersion
		&& (ResourceInstance.ResourceId.IsNone()
			|| ResourceDataAsset->ResourceId == ResourceInstance.ResourceId)
		&& FMath::IsFinite(ResourceDataAsset->SeedEnergy))
	{
		OutSeedEnergy = FMath::Max(0.0, ResourceDataAsset->SeedEnergy);
		return true;
	}

	FSRReferenceResourceDefinitionV2 ReferenceDefinition;
	if (FSRResourceSystemContent::TryGetReferenceResourceDefinition(
			ResourceInstance.ResourceId,
			ReferenceDefinition)
		&& FMath::IsFinite(ReferenceDefinition.SeedEnergy))
	{
		OutSeedEnergy = FMath::Max(0.0, ReferenceDefinition.SeedEnergy);
		return true;
	}

	// A genuinely dynamic Card has no external definition. Its energy at the
	// creation/migration boundary becomes the stable Seed rather than granting
	// a permanent no-resistance fallback.
	if (FMath::IsFinite(ResourceInstance.CurrentEnergy))
	{
		OutSeedEnergy = FMath::Max(0.0, ResourceInstance.CurrentEnergy);
		return true;
	}
	return false;
}

void StarRovers::Resources::EnsureResourceSeedEnergySnapshot(FSRResourceInstance& ResourceInstance)
{
	if (ResourceInstance.ResourceClass != ESRResourceClass::Card)
	{
		ResourceInstance.SeedEnergySnapshot = 0.0;
		ResourceInstance.bHasSeedEnergySnapshot = false;
		return;
	}

	double ResolvedSeedEnergy = 0.0;
	if (TryResolveResourceSeedEnergy(ResourceInstance, ResolvedSeedEnergy))
	{
		ResourceInstance.SeedEnergySnapshot = ResolvedSeedEnergy;
		ResourceInstance.bHasSeedEnergySnapshot = true;
	}
}

void StarRovers::Resources::SynchronizeLegacyRuntimeStateToResourceV2(FSRResourceInstance& ResourceInstance)
{
	PopulateDefinitionFieldsIfAvailable(ResourceInstance);
	ResourceInstance.CurrentEnergy = ResourceInstance.EnergyValue;
	ResourceInstance.ProcessingMemory.ProcessCount = FMath::Max(0, ResourceInstance.ProcessCount);
	ResourceInstance.ProcessingMemory.EnergyChangeCount = FMath::Max(0, ResourceInstance.EnergyChangeCount);
	NormalizeResourceV2Fields(ResourceInstance);
}

void StarRovers::Resources::SynchronizeResourceV2RuntimeStateToLegacy(FSRResourceInstance& ResourceInstance)
{
	ResourceInstance.EnergyValue = ResourceInstance.CurrentEnergy;
	ResourceInstance.ProcessCount = FMath::Max(0, ResourceInstance.ProcessingMemory.ProcessCount);
	ResourceInstance.EnergyChangeCount = FMath::Max(0, ResourceInstance.ProcessingMemory.EnergyChangeCount);
}

void StarRovers::Resources::UpgradeResourceInstanceToCurrentSchema(
	FSRResourceInstance& ResourceInstance,
	bool bTreatAsLegacyPayload)
{
	const bool bNeedsLegacyBridge = bTreatAsLegacyPayload
		|| ResourceInstance.ResourceSchemaVersion < InitialResourceV2SchemaVersion;
	if (bNeedsLegacyBridge)
	{
		PopulateDefinitionFieldsForLegacyPayload(ResourceInstance);
		SynchronizeLegacyRuntimeStateToResourceV2(ResourceInstance);
		return;
	}

	PopulateDefinitionFieldsIfAvailable(ResourceInstance);
	NormalizeResourceV2Fields(ResourceInstance);
}

void StarRovers::Resources::PrepareResourceInstanceForSave(FSRResourceInstance& ResourceInstance)
{
	if (ResourceInstance.ResourceId.IsNone() && !IsValid(ResourceInstance.ResourceDataAsset.Get()))
	{
		return;
	}

	const USRSimulationSettings* SimulationSettings = GetDefault<USRSimulationSettings>();
	if (!SimulationSettings || SimulationSettings->ResourceRulesetVersion == ESRResourceRulesetVersion::Legacy)
	{
		SynchronizeLegacyRuntimeStateToResourceV2(ResourceInstance);
		return;
	}

	UpgradeResourceInstanceToCurrentSchema(ResourceInstance);
}

bool StarRovers::Resources::AreResourceV2RuntimeFieldsEquivalent(
	const FSRResourceInstance& Left,
	const FSRResourceInstance& Right)
{
	return Left.ResourceSchemaVersion == Right.ResourceSchemaVersion
		&& Left.ResourceClass == Right.ResourceClass
		&& Left.Family == Right.Family
		&& FMath::IsNearlyEqual(Left.CurrentEnergy, Right.CurrentEnergy)
		&& Left.bHasSeedEnergySnapshot == Right.bHasSeedEnergySnapshot
		&& (!Left.bHasSeedEnergySnapshot
			|| FMath::IsNearlyEqual(Left.SeedEnergySnapshot, Right.SeedEnergySnapshot))
		&& Left.Spectrum == Right.Spectrum
		&& Left.Grade == Right.Grade
		&& Left.ActiveFamilyStateFlags == Right.ActiveFamilyStateFlags
		&& Left.ProcessTagSlot.TagId == Right.ProcessTagSlot.TagId
		&& Left.ProcessTagSlot.Lifecycle == Right.ProcessTagSlot.Lifecycle
		&& Left.ProcessTagSlot.RemainingTriggers == Right.ProcessTagSlot.RemainingTriggers
		&& Left.FuelImprintSlot.ImprintId == Right.FuelImprintSlot.ImprintId
		&& AreProcessingMemoriesEquivalent(Left.ProcessingMemory, Right.ProcessingMemory)
		&& AreLogisticsMetadataEquivalent(Left.LogisticsMetadata, Right.LogisticsMetadata);
}
