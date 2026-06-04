#pragma once

#include "CoreMinimal.h"
#include "SRPlanetBiomeTypes.generated.h"

UENUM(BlueprintType)
enum class ESRPlanetBiome : uint8
{
	Ocean,
	Coast,
	Snow,
	Plains
};

UENUM(BlueprintType)
enum class ESRBiomePlacementRestriction : uint8
{
	None UMETA(DisplayName = "None"),

	OceanOnly UMETA(DisplayName = "Ocean Only"),
	CoastOnly UMETA(DisplayName = "Coast Only"),
	InlandOnly UMETA(DisplayName = "Inland Only"),

	LowAltitudeOnly UMETA(DisplayName = "Low Altitude Only"),
	HighAltitudeOnly UMETA(DisplayName = "High Altitude Only"),
	MountainOnly UMETA(DisplayName = "Mountain Only"),

	EquatorialOnly UMETA(DisplayName = "Equatorial Only"),
	TemperateLatitudeOnly UMETA(DisplayName = "Temperate Latitude Only"),
	PolarOnly UMETA(DisplayName = "Polar Only"),

	HotOnly UMETA(DisplayName = "Hot Only"),
	ColdOnly UMETA(DisplayName = "Cold Only"),

	DryOnly UMETA(DisplayName = "Dry Only"),
	HumidOnly UMETA(DisplayName = "Humid Only"),

	RiverOnly UMETA(DisplayName = "River Only"),
	LakeOnly UMETA(DisplayName = "Lake Only"),

	HighErosionOnly UMETA(DisplayName = "High Erosion Only"),
	LowErosionOnly UMETA(DisplayName = "Low Erosion Only"),

	RareRegionOnly UMETA(DisplayName = "Rare Region Only")
};

UENUM(BlueprintType)
enum class ESRBiomeWaterRole : uint8
{
	None UMETA(DisplayName = "None"),
	Ocean UMETA(DisplayName = "Ocean"),
	Coast UMETA(DisplayName = "Coast"),
	River UMETA(DisplayName = "River"),
	Lake UMETA(DisplayName = "Lake")
};

UENUM(BlueprintType)
enum class ESRBiomePlacementMetric : uint8
{
	HeightAlpha UMETA(DisplayName = "Height Alpha"),
	Continentalness UMETA(DisplayName = "Continentalness"),
	LandMask UMETA(DisplayName = "Land Mask"),
	CoastMask UMETA(DisplayName = "Coast Mask"),
	OceanDepthMask UMETA(DisplayName = "Ocean Depth Mask"),
	InlandMask UMETA(DisplayName = "Inland Mask"),
	MountainMask UMETA(DisplayName = "Mountain Mask"),
	RiverMask UMETA(DisplayName = "River Mask"),
	LakeMask UMETA(DisplayName = "Lake Mask"),
	Erosion UMETA(DisplayName = "Erosion"),
	Temperature UMETA(DisplayName = "Temperature"),
	Moisture UMETA(DisplayName = "Moisture"),
	AbsLatitudeDegrees UMETA(DisplayName = "Abs Latitude Degrees"),
	RareRegionNoise UMETA(DisplayName = "Rare Region Noise")
};

UENUM(BlueprintType)
enum class ESRBiomePlacementComparison : uint8
{
	GreaterThan UMETA(DisplayName = "Greater Than"),
	GreaterOrEqual UMETA(DisplayName = "Greater Or Equal"),
	LessThan UMETA(DisplayName = "Less Than"),
	LessOrEqual UMETA(DisplayName = "Less Or Equal"),
	BetweenInclusive UMETA(DisplayName = "Between Inclusive"),
	OutsideInclusive UMETA(DisplayName = "Outside Inclusive")
};

USTRUCT(BlueprintType)
struct STARROVERS_API FSRBiomePlacementRule
{
	GENERATED_BODY()

	FSRBiomePlacementRule();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Biome", meta = (DisplayName = "Metric", ToolTip = "이 Biome을 배치할 때 비교할 지형/기후 값입니다. 아래 MetricDescription에서 선택한 Metric의 의미와 임계값 조정 효과를 확인할 수 있습니다."))
	ESRBiomePlacementMetric Metric = ESRBiomePlacementMetric::HeightAlpha;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Biome", meta = (DisplayName = "Comparison", ToolTip = "Metric 값을 Threshold와 어떻게 비교할지 정합니다. Between/Outside는 Threshold와 MaxThreshold 사이 범위를 사용합니다."))
	ESRBiomePlacementComparison Comparison = ESRBiomePlacementComparison::GreaterOrEqual;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Biome", meta = (DisplayName = "bUseMetricDefaultThresholds", ToolTip = "켜져 있으면 Metric을 바꾸거나 rule을 갱신할 때 참고 기본값을 Threshold/MaxThreshold에 복사합니다. 직접 수치를 조정하려면 끄는 것이 안전합니다."))
	bool bUseMetricDefaultThresholds = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Biome", meta = (DisplayName = "Threshold", ToolTip = "비교에 사용하는 첫 번째 임계값입니다. Greater/Less 계열은 이 값 하나만 사용하고, Between/Outside는 MaxThreshold와 함께 범위를 만듭니다."))
	float Threshold = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "StarRovers|Biome", meta = (DisplayName = "MaxThreshold", ToolTip = "Between/Outside 비교에서 사용하는 두 번째 임계값입니다. Threshold보다 작게 입력해도 내부에서는 작은 값을 하한, 큰 값을 상한으로 해석합니다."))
	float MaxThreshold = 0.34f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Biome", meta = (DisplayName = "MetricDefaultThreshold", ToolTip = "선택한 Metric의 참고 하한값입니다. 이 값은 비교용 기본 수치이며, 실제 판정은 Threshold 값을 사용합니다."))
	float MetricDefaultThreshold = 0.18f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Biome", meta = (DisplayName = "MetricDefaultMaxThreshold", ToolTip = "선택한 Metric의 참고 상한값입니다. Between/Outside rule을 만들 때 시작점으로 쓰기 위한 값입니다."))
	float MetricDefaultMaxThreshold = 0.34f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StarRovers|Biome", meta = (DisplayName = "MetricDescription", MultiLine = "true", ToolTip = "선택한 Metric이 무엇을 뜻하는지, 값을 키우거나 줄이면 배치 결과가 어떻게 바뀌는지 설명합니다."))
	FText MetricDescription;

	void RefreshMetricDefaults(bool bApplyToThresholds);
};
