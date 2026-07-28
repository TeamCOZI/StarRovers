#pragma once

#include "CoreMinimal.h"
#include "Automation/SRResourceDataAsset.h"

class AActor;

namespace StarRovers::Resources
{
	STARROVERS_API int32 GetFamilyStateBit(ESRResourceFamilyState FamilyState);
	STARROVERS_API FName ResolveCelestialBodyResourceId(const AActor* BodyActor);
	STARROVERS_API void InitializeResourceOrigin(FSRResourceInstance& ResourceInstance, FName BodyId);
	STARROVERS_API void RecordResourceProcessedOnBody(FSRResourceInstance& ResourceInstance, FName BodyId);
	STARROVERS_API void RecordResourceTransit(
		FSRResourceInstance& ResourceInstance,
		FName SourceBodyId,
		FName DestinationBodyId);
	STARROVERS_API bool TryResolveResourceSeedEnergy(
		const FSRResourceInstance& ResourceInstance,
		double& OutSeedEnergy);
	STARROVERS_API void EnsureResourceSeedEnergySnapshot(FSRResourceInstance& ResourceInstance);

	// Copies the currently authoritative Legacy runtime values into their V2 mirrors.
	// This does not change Legacy gameplay behavior.
	STARROVERS_API void SynchronizeLegacyRuntimeStateToResourceV2(FSRResourceInstance& ResourceInstance);
	// Keeps temporary Legacy readers coherent after a Resource V2 calculation.
	STARROVERS_API void SynchronizeResourceV2RuntimeStateToLegacy(FSRResourceInstance& ResourceInstance);

	// Upgrades an instance in place. bTreatAsLegacyPayload must be true for payloads
	// written before Resource schema version 2, even if a newly-added schema field
	// received its current C++ default during tagged-property loading.
	STARROVERS_API void UpgradeResourceInstanceToCurrentSchema(
		FSRResourceInstance& ResourceInstance,
		bool bTreatAsLegacyPayload = false);

	// Normalizes an outbound payload and mirrors Legacy values while Legacy remains active.
	STARROVERS_API void PrepareResourceInstanceForSave(FSRResourceInstance& ResourceInstance);

	// Compares every V2 field that can affect future processing, fuel scoring, or routing.
	STARROVERS_API bool AreResourceV2RuntimeFieldsEquivalent(
		const FSRResourceInstance& Left,
		const FSRResourceInstance& Right);
}
