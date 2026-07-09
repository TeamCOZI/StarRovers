#include "Celestial/SRCelestialBody.h"

#include "SRCelestialBodyLog.h"
#include "Celestial/SRCelestialBodyDynamicMeshPipeline.h"
#include "Celestial/SRDynamicMeshBaseDataAsset.h"
#include "Components/DynamicMeshComponent.h"
#include "Templates/Function.h"
#include "Utility/SRTimingLog.h"

using namespace StarRovers::Celestial::DynamicMesh;

namespace
{
	bool HasDynamicMeshBuildSource(
		const UDynamicMeshComponent* DynamicMeshComponent,
		const UStaticMesh* StaticMesh,
		const USRDynamicMeshBaseDataAsset* DynamicMeshBaseDataAsset)
	{
		return IsValid(DynamicMeshComponent)
			&& (IsValid(StaticMesh) || IsValid(DynamicMeshBaseDataAsset));
	}

	void LogDynamicMeshAlreadyBuilt(const FString& BodyName, double TotalStart)
	{
		FSRTimingLog::AddLine(FString::Printf(
			TEXT("DynamicMesh '%s' AlreadyBuilt %.2f ms"),
			*BodyName,
			GetDynamicMeshTimingElapsedMilliseconds(TotalStart)));
	}

	bool ShouldGenerateMetadataTerrain(
		ESRCelestialBodyCategory BodyCategory,
		const FSRDynamicMeshGeneration& DynamicMeshGeneration)
	{
		return (BodyCategory == ESRCelestialBodyCategory::Planet || BodyCategory == ESRCelestialBodyCategory::Moon)
			&& DynamicMeshGeneration.bDynamicMeshGeneration
			&& DynamicMeshGeneration.DynamicMeshHeight > KINDA_SMALL_NUMBER;
	}

	bool BuildDynamicMeshFromBaseMetadata(ASRCelestialBody& Body, uint32 DynamicMeshBuildHash, double TotalStart)
	{
		(void)DynamicMeshBuildHash;
		FSRCelestialBodyPreparedDynamicMesh PreparedMesh;
		if (!Body.BuildPreparedCelestialBodyDynamicMesh(PreparedMesh))
		{
			return false;
		}
		return Body.ApplyPreparedCelestialBodyDynamicMesh(MoveTemp(PreparedMesh), TotalStart);
	}

	bool TryApplyDynamicMeshRuntimeCache(
		const FString& BodyName,
		FSRCelestialBodyDynamicMeshRuntimeState& DynamicMeshState,
		UDynamicMeshComponent* PrimaryDynamicMeshComponent,
		const TArray<TObjectPtr<UDynamicMeshComponent>>& FaceDynamicMeshComponents,
		USRPlanetSurfaceGrid* SurfaceGrid,
		uint32 DynamicMeshBuildHash,
		double TotalStart,
		bool& bOutApplied)
	{
		if (!bEnableGlobalDynamicMeshRuntimeCache)
		{
			return false;
		}

		const FSRCelestialBodyDynamicMeshRuntimeCacheEntry* CacheEntry = FindCelestialBodyDynamicMeshRuntimeCache(DynamicMeshBuildHash);
		if (!CacheEntry)
		{
			return false;
		}

		bOutApplied = ApplyDynamicMeshRuntimeCacheEntry(
			BodyName,
			DynamicMeshState,
			PrimaryDynamicMeshComponent,
			FaceDynamicMeshComponents,
			SurfaceGrid,
			*CacheEntry,
			DynamicMeshBuildHash);
		FSRTimingLog::AddLine(FString::Printf(
			TEXT("DynamicMesh '%s' RuntimeCacheHit Total=%.2f ms"),
			*BodyName,
			GetDynamicMeshTimingElapsedMilliseconds(TotalStart)));
		return true;
	}

	bool BuildDynamicMeshFromCurrentSource(
		const FString& BodyName,
		ESRCelestialBodyCategory BodyCategory,
		const FSRDynamicMeshGeneration& DynamicMeshGeneration,
		const USRDynamicMeshBaseDataAsset* DynamicMeshBaseDataAsset,
		uint32 DynamicMeshBuildHash,
		double TotalStart,
		TFunctionRef<bool(uint32 InDynamicMeshBuildHash, double InTotalStart)> BuildStaticMeshFallback,
		TFunctionRef<bool(uint32 InDynamicMeshBuildHash, double InTotalStart)> BuildBaseMetadata)
	{
		if (!ShouldGenerateMetadataTerrain(BodyCategory, DynamicMeshGeneration))
		{
			return BuildStaticMeshFallback(DynamicMeshBuildHash, TotalStart);
		}

		if (IsValid(DynamicMeshBaseDataAsset))
		{
			return BuildBaseMetadata(DynamicMeshBuildHash, TotalStart);
		}

		UE_LOG(LogStarRoversCelestial, Error, TEXT("Celestial body '%s' requires DynamicMeshBaseDataAsset for metadata terrain generation."), *BodyName);
		return false;
	}
}

bool ASRCelestialBody::BuildCelestialBodyDynamicMesh()
{
	FSRTimingLogSession TimingLogSession(FString::Printf(TEXT("DynamicMesh '%s'"), *GetName()));
	const double TotalStart = GetDynamicMeshTimingSeconds();
	if (!HasDynamicMeshBuildSource(CelestialBodyDynamicMesh.Get(), StaticMesh.Get(), DynamicMeshBaseDataAsset.Get()))
	{
		return false;
	}

	const uint32 DynamicMeshBuildHash = ComputeDynamicMeshBuildHash();
	if (DynamicMeshState.HasBuildHash(DynamicMeshBuildHash))
	{
		LogDynamicMeshAlreadyBuilt(GetName(), TotalStart);
		return true;
	}

	bool bAppliedRuntimeCache = false;
	if (TryApplyDynamicMeshRuntimeCache(
		GetName(),
		DynamicMeshState,
		CelestialBodyDynamicMesh.Get(),
		CelestialBodyDynamicMeshFaces,
		GetSurfaceGrid(),
		DynamicMeshBuildHash,
		TotalStart,
		bAppliedRuntimeCache))
	{
		return bAppliedRuntimeCache;
	}

	ResetDynamicMeshCellColorData();
	return BuildDynamicMeshFromCurrentSource(
		GetName(),
		BodyCategory,
		DynamicMeshGeneration,
		DynamicMeshBaseDataAsset.Get(),
		DynamicMeshBuildHash,
		TotalStart,
		[this](uint32 InDynamicMeshBuildHash, double InTotalStart)
		{
			return BuildDynamicMeshFromStaticMeshFallback(InDynamicMeshBuildHash, InTotalStart);
		},
		[this](uint32 InDynamicMeshBuildHash, double InTotalStart)
		{
			return BuildDynamicMeshFromBaseMetadata(*this, InDynamicMeshBuildHash, InTotalStart);
		});
}
