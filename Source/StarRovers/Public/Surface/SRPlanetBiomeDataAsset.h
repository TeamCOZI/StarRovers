#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Simulation/SRNaturalStructureSpawnTypes.h"
#include "Surface/SRPlanetBiomeTypes.h"
#include "SRPlanetBiomeDataAsset.generated.h"

UCLASS(BlueprintType)
class STARROVERS_API USRPlanetBiomeDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	USRPlanetBiomeDataAsset();

	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Biome", meta = (DisplayName = "BiomeId", ToolTip = "이 Biome을 식별하는 이름입니다. Profile의 BiomeMaterials, SurfaceGrid cell, 자연 구조물 배치가 이 값을 기준으로 연결됩니다."))
	FName BiomeId = FName(TEXT("Plains"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Biome", meta = (DisplayName = "WaterRole", ToolTip = "이 Biome이 물 판정에서 어떤 역할을 하는지 정합니다. PlacementRules는 배치 위치를 정하고, WaterRole은 Ocean/River/Lake/Coast 같은 물 처리 의미만 따로 제공합니다."))
	ESRBiomeWaterRole WaterRole = ESRBiomeWaterRole::None;

	UPROPERTY()
	TArray<ESRBiomePlacementRestriction> PlacementRestrictions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Biome", meta = (DisplayName = "PlacementRules", ToolTip = "이 Biome이 생성될 수 있는 지형/기후 조건입니다. 여러 rule을 넣으면 모두 통과해야 후보가 됩니다."))
	TArray<FSRBiomePlacementRule> PlacementRules;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Biome", meta = (DisplayName = "SpawnWeight", ClampMin = "0.01", ToolTip = "조건을 통과한 뒤 점수 경쟁에서 곱해지는 가중치입니다. 값을 키우면 같은 조건 안에서 더 자주 이기고, 줄이면 덜 선택됩니다."))
	float SpawnWeight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Biome", meta = (DisplayName = "RegionSize", ClampMin = "0.01", ClampMax = "1.0", ToolTip = "이 Biome의 고정 anchor region 크기입니다. 값을 키우면 넓은 덩어리로 퍼지고, 줄이면 작은 패치처럼 나타납니다."))
	float RegionSize = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Biome Priority", meta = (DisplayName = "Priority", ToolTip = "priority override 때 비교하는 우선순위입니다. 값이 높은 Biome이 낮은 Biome을 덮을 수 있습니다."))
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Biome Priority", meta = (DisplayName = "bCanOverrideLowerPriorityBiomes", ToolTip = "켜면 이 Biome이 충분한 점수를 얻었을 때 일반 점수 우승 Biome보다 높은 Priority로 덮어쓸 수 있습니다. 눈처럼 위에 덮이는 Biome에 사용합니다."))
	bool bCanOverrideLowerPriorityBiomes = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Biome Priority", meta = (DisplayName = "OverrideMinScore", ClampMin = "0.0", EditCondition = "bCanOverrideLowerPriorityBiomes", ToolTip = "priority override가 발동하기 위한 최소 점수입니다. 값을 키우면 더 확실한 조건에서만 덮어쓰고, 낮추면 더 쉽게 덮어씁니다."))
	float OverrideMinScore = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Natural Structures", meta = (DisplayName = "NaturalStructureSpawnRules"))
	TArray<FSRProfileNaturalStructureSpawnRule> NaturalStructureSpawnRules;

private:
	void MigratePlacementRestrictionsToRules();
	void RefreshPlacementRuleDefaults(bool bApplyMetricDefaults);
};
