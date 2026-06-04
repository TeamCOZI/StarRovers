#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Simulation/SRNaturalStructureSpawnTypes.h"
#include "Surface/SRPlanetBiomeDataAsset.h"
#include "Surface/SRPlanetTerrainTypes.h"
#include "SRPlanetTerrainProfileDataAsset.generated.h"

#if WITH_EDITOR
struct FPropertyChangedEvent;
#endif

USTRUCT(BlueprintType)
struct STARROVERS_API FSRPlanetProfileBiomeEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Terrain Profile", meta = (DisplayName = "BiomeDataAsset"))
	TObjectPtr<USRPlanetBiomeDataAsset> BiomeDataAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Natural Structures", meta = (DisplayName = "NaturalStructureSpawnRuleOverrides"))
	TArray<FSRNaturalStructureSpawnRuleOverride> NaturalStructureSpawnRuleOverrides;
};

UCLASS(BlueprintType)
class STARROVERS_API USRPlanetTerrainProfileDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	USRPlanetTerrainProfileDataAsset();

	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	void ApplyToDynamicMeshGeneration(FSRDynamicMeshGeneration& InOutDynamicMeshGeneration) const;
	TArray<TObjectPtr<USRPlanetBiomeDataAsset>> GetAllowedBiomeDataAssets() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Terrain Profile", meta = (DisplayName = "ProfileId"))
	FName ProfileId = FName(TEXT("Default"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Terrain Profile", meta = (DisplayName = "Biomes"))
	TArray<FSRPlanetProfileBiomeEntry> Biomes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Natural Structures", meta = (DisplayName = "ProfileNaturalStructureSpawnRules"))
	TArray<FSRProfileNaturalStructureSpawnRule> ProfileNaturalStructureSpawnRules;

private:
	void NormalizeProfile();
};
