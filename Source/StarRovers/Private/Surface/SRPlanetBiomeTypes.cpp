#include "Surface/SRPlanetBiomeTypes.h"

namespace
{
	void GetBiomePlacementMetricDefaults(ESRBiomePlacementMetric Metric, float& OutThreshold, float& OutMaxThreshold)
	{
		switch (Metric)
		{
		case ESRBiomePlacementMetric::HeightAlpha:
			OutThreshold = 0.18f;
			OutMaxThreshold = 0.34f;
			break;
		case ESRBiomePlacementMetric::Continentalness:
			OutThreshold = 0.0f;
			OutMaxThreshold = 0.36f;
			break;
		case ESRBiomePlacementMetric::LandMask:
			OutThreshold = 0.45f;
			OutMaxThreshold = 1.0f;
			break;
		case ESRBiomePlacementMetric::CoastMask:
			OutThreshold = 0.48f;
			OutMaxThreshold = 1.0f;
			break;
		case ESRBiomePlacementMetric::OceanDepthMask:
			OutThreshold = 0.62f;
			OutMaxThreshold = 1.0f;
			break;
		case ESRBiomePlacementMetric::InlandMask:
			OutThreshold = 0.5f;
			OutMaxThreshold = 1.0f;
			break;
		case ESRBiomePlacementMetric::MountainMask:
			OutThreshold = 0.42f;
			OutMaxThreshold = 1.0f;
			break;
		case ESRBiomePlacementMetric::RiverMask:
			OutThreshold = 0.5f;
			OutMaxThreshold = 1.0f;
			break;
		case ESRBiomePlacementMetric::LakeMask:
			OutThreshold = 0.34f;
			OutMaxThreshold = 1.0f;
			break;
		case ESRBiomePlacementMetric::Erosion:
			OutThreshold = 0.62f;
			OutMaxThreshold = 1.0f;
			break;
		case ESRBiomePlacementMetric::Temperature:
			OutThreshold = 0.62f;
			OutMaxThreshold = 1.0f;
			break;
		case ESRBiomePlacementMetric::Moisture:
			OutThreshold = 0.62f;
			OutMaxThreshold = 1.0f;
			break;
		case ESRBiomePlacementMetric::AbsLatitudeDegrees:
			OutThreshold = 58.0f;
			OutMaxThreshold = 90.0f;
			break;
		case ESRBiomePlacementMetric::RareRegionNoise:
			OutThreshold = 0.68f;
			OutMaxThreshold = 1.0f;
			break;
		default:
			OutThreshold = 0.0f;
			OutMaxThreshold = 1.0f;
			break;
		}
	}

	FText GetBiomePlacementMetricDescription(ESRBiomePlacementMetric Metric)
	{
		switch (Metric)
		{
		case ESRBiomePlacementMetric::HeightAlpha:
			return FText::FromString(TEXT("지형 높이입니다. -1에 가까울수록 기준 반지름보다 낮고, 0은 기준 표면 근처, 1에 가까울수록 높은 지형입니다. 임계값을 키우면 산지/고지대만 통과하고, 낮추면 낮은 지형도 더 많이 통과합니다."));
		case ESRBiomePlacementMetric::Continentalness:
			return FText::FromString(TEXT("바다에서 내륙으로 이어지는 축입니다. OceanThreshold가 반영된 값이며, 낮거나 음수이면 바다, 0 근처이면 해안, 높을수록 내륙입니다. 임계값을 키우면 더 깊은 내륙만 통과하고, 낮추면 해안이나 바다 쪽도 통과합니다."));
		case ESRBiomePlacementMetric::LandMask:
			return FText::FromString(TEXT("육지 여부를 0..1로 부드럽게 만든 값입니다. 0은 바다, 1은 확실한 육지, 중간값은 해안 전환부입니다. 임계값을 키우면 확실한 육지만 통과하고, 낮추면 얕은 바다/해안도 통과합니다."));
		case ESRBiomePlacementMetric::CoastMask:
			return FText::FromString(TEXT("해안선에 가까운 정도입니다. 값이 높을수록 바다와 육지의 경계에 가깝고, 낮으면 깊은 바다이거나 내륙입니다. 임계값을 키우면 좁고 선명한 해안만 통과하고, 낮추면 해안 주변 폭이 넓어집니다."));
		case ESRBiomePlacementMetric::OceanDepthMask:
			return FText::FromString(TEXT("깊은 바다 정도입니다. 값이 높을수록 육지에서 멀고 깊은 바다이며, 0에 가까우면 해안이나 육지입니다. 임계값을 키우면 더 깊은 바다만 통과하고, 낮추면 얕은 바다도 통과합니다."));
		case ESRBiomePlacementMetric::InlandMask:
			return FText::FromString(TEXT("내륙 정도입니다. 값이 높을수록 해안에서 멀고 대륙 안쪽입니다. 임계값을 키우면 깊은 내륙만 통과하고, 낮추면 해안 가까운 육지도 통과합니다."));
		case ESRBiomePlacementMetric::MountainMask:
			return FText::FromString(TEXT("산악 지형 가능성입니다. 봉우리/능선 노이즈, 침식 억제, 육지 마스크가 반영됩니다. 임계값을 키우면 더 뚜렷한 산지만 통과하고, 낮추면 완만한 구릉도 통과합니다."));
		case ESRBiomePlacementMetric::RiverMask:
			return FText::FromString(TEXT("강 경로 강도입니다. 값이 높을수록 생성된 강줄기에 가깝습니다. 임계값을 키우면 강 중심부만 통과하고, 낮추면 강 주변부까지 통과합니다."));
		case ESRBiomePlacementMetric::LakeMask:
			return FText::FromString(TEXT("호수/분지 강도입니다. 값이 높을수록 생성된 호수나 분지 후보에 가깝습니다. 임계값을 키우면 확실한 호수 중심만 통과하고, 낮추면 주변 낮은 지형도 통과합니다."));
		case ESRBiomePlacementMetric::Erosion:
			return FText::FromString(TEXT("침식 정도입니다. 값이 높을수록 산이 깎이고 완만한 지형이 되기 쉽습니다. 임계값을 키우면 많이 침식된 평탄 지형만 통과하고, 낮추면 거칠고 산지가 남은 지형도 통과합니다."));
		case ESRBiomePlacementMetric::Temperature:
			return FText::FromString(TEXT("기온입니다. 0은 추운 지역, 1은 더운 지역이며 위도, 노이즈, 고도 영향을 받습니다. 임계값을 키우면 더운 지역만 통과하고, 낮추면 온대/한대 지역도 통과합니다."));
		case ESRBiomePlacementMetric::Moisture:
			return FText::FromString(TEXT("습도입니다. 0은 건조, 1은 습윤입니다. 임계값을 키우면 비가 많고 습한 지역만 통과하고, 낮추면 건조한 지역도 통과합니다."));
		case ESRBiomePlacementMetric::AbsLatitudeDegrees:
			return FText::FromString(TEXT("적도에서 떨어진 절대 위도입니다. 0도는 적도, 90도는 극지입니다. 임계값을 키우면 극지에 더 가까운 곳만 통과하고, 낮추면 중위도나 적도 쪽도 통과합니다."));
		case ESRBiomePlacementMetric::RareRegionNoise:
			return FText::FromString(TEXT("희귀 지역 마스크입니다. BiomeId와 Seed로 결정되는 0..1 값이며, 높을수록 드문 지역입니다. 임계값을 키우면 더 희귀하게 생성되고, 낮추면 더 자주 생성됩니다."));
		default:
			return FText::GetEmpty();
		}
	}
}

FSRBiomePlacementRule::FSRBiomePlacementRule()
{
	RefreshMetricDefaults(true);
}

void FSRBiomePlacementRule::RefreshMetricDefaults(bool bApplyToThresholds)
{
	GetBiomePlacementMetricDefaults(Metric, MetricDefaultThreshold, MetricDefaultMaxThreshold);
	MetricDescription = GetBiomePlacementMetricDescription(Metric);

	if (bApplyToThresholds && bUseMetricDefaultThresholds)
	{
		Threshold = MetricDefaultThreshold;
		MaxThreshold = MetricDefaultMaxThreshold;
	}
}
