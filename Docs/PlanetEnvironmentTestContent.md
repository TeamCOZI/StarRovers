# 행성 환경 테스트 콘텐츠 구현

## 목적

한 Run에서 생성되는 행성들이 이름만 다르고 같은 지형을 사용하는 문제를 해결하고, Resource V2 자동화 Line을 여러 기후와 지형에서 시험할 수 있도록 행성 환경 카탈로그를 확장했다.

핵심 목표는 다음과 같다.

- 한 Run에 행성 5~7개와 행성별 위성 1~3개를 생성한다.
- 한 Run의 행성에는 최소 4종의 서로 다른 환경이 반드시 등장한다.
- 각 환경은 지형 높낮이, 바다 비율, 온도, 습도, Biome, 색조와 유리한 자원이 실제로 다르다.
- 환경마다 실제 생성 자원을 3종으로 제한하되, 선택된 Solar System 전체에는 필수 자원 7종이 반드시 존재한다.

## 적용 전 Editor 실제 상태

읽기 전용 검사 Commandlet로 Blueprint CDO와 `SolarSystem` 레벨 인스턴스를 함께 조사했다.

- `DA_Planet_Temperate`와 `DA_Planet_LavaOcean`은 이름을 제외하면 같은 `DA_Profile_Earth`, 같은 Cube Sphere Shape, 같은 지형 수치를 사용했다.
- `DA_Planet_BadLands`는 존재했지만 Shape와 Terrain Profile이 비어 있어 생성 후보로 사용할 수 없었다.
- `BP_SolarSystemGenerator` CDO의 `PlanetDataAssets`는 비어 있었다.
- 실제 생성에 사용되는 `SolarSystem` 레벨 인스턴스에는 Temperate와 Lava Ocean 두 개만 등록돼 있었다.
- 레벨 인스턴스의 행성 수는 5개, 위성 수는 행성당 1개로 고정돼 있었다.

즉 Lava Ocean이 별도 이름으로 선택돼도 지형과 Biome 구성이 Temperate와 같았기 때문에 사실상 한 종류의 행성만 등장했다.

## 최종 환경 카탈로그

| 환경 | 가중치 | 바다 | 지형 높이 | Ocean Threshold | 온도 Bias | 습도 Bias | 표본 물 비율 | 실제 자원: 주산 / 부산 / 운영 |
|---|---:|:---:|---:|---:|---:|---:|---:|---|
| Temperate | 1.35 | O | 16 | 0.86 | +0.00 | +0.05 | 9.1% | Verdant Spore / Helios Iron / Biomass Feedstock |
| Arid Desert | 1.15 | X | 18 | -0.38 | +0.18 | -0.48 | 0.0% | Helios Iron / Echo Quartz / Common Ore |
| Frozen Ocean | 1.00 | O | 20 | 0.92 | -0.42 | +0.05 | 82.9% | Echo Quartz / Null Pearl / Common Ore |
| Badlands | 1.05 | X | 30 | -0.32 | +0.12 | -0.30 | 0.0% | Null Pearl / Verdant Spore / Common Ore |
| Lava Ocean | 0.85 | O | 26 | 0.96 | +0.38 | -0.35 | 78.6% | Aurora Plasma / Helios Iron / Common Ore |
| Toxic Wetland | 0.80 | O | 13 | 0.82 | +0.08 | +0.42 | 47.7% | Verdant Spore / Aurora Plasma / Biomass Feedstock |

표본 물 비율은 각 DA의 최종 Terrain 설정으로 구 표면 방향 768개를 결정적으로 샘플링한 결과다. `OceanThreshold`는 현재 지형식에서 값이 클수록 대륙 노이즈가 해수면 아래로 이동해 물 지형이 많아진다.

### 환경별 Biome

- Temperate: 기존 `DA_Profile_Earth`의 Plain, Ocean, Coast, Snow를 유지한다.
- Arid Desert: Dune Sea, Salt Flat, Mesa.
- Frozen Ocean: Snowfield, Cryo Ocean, Ice Shelf, Glacier.
- Badlands: Dust Barrens, Eroded Canyon, Iron Crag.
- Lava Ocean: Basalt Plain, Magma Ocean, Ash Coast, Obsidian Ridge.
- Toxic Wetland: Toxic Mire, Acid Ocean, Fungal Coast, Spore Marsh.

각 환경은 별도 Terrain Profile을 사용하며 Biome placement rule, 온도 상태 색조, 산맥·침식·강·호수·detail/noise 값도 서로 다르다. 따라서 이름이나 UI 표기만 다른 환경이 아니다.

## Resource V2와의 연결

새 Profile들은 기존 `DA_Profile_Earth`의 Resource V2 규칙 7개를 자원 카탈로그로 복사하지만, 각 행성 DA override에서 실제 생성 규칙은 정확히 3개만 켠다. 나머지 4개는 낮은 확률이 아니라 완전히 비활성화한다.

- 주산 카드: 최소 6개, 최대 10개, Cell 간격 6.
- 부산 카드: 최소 4개, 최대 7개, Cell 간격 8.
- 운영 자원: 최소 5개, 최대 8개, Cell 간격 7.
- 부재 자원: `bEnabled=false`, 확률·최소·최대 개수 0.

일반 확률 배치가 최소 개수에 도달하지 못하면 같은 결정적 후보 순서에서 확률 판정만 제거한 보장 Pass를 수행한다. 유효 Cell과 간격 조건은 유지하므로 자원 보장이 서로 겹치는 잘못된 배치를 허용하지 않는다.

카탈로그 전체에서 Helios Iron과 Verdant Spore는 각각 3개 환경, 나머지 카드 자원은 각각 2개 환경에 존재한다. Common Ore는 4개 환경, Biomass Feedstock은 2개 환경에 존재한다. 따라서 특정 환경 하나가 유일한 자원 출처가 되지 않는다.

## SolarSystemGenerator 선택 규칙

`BP_SolarSystemGenerator`의 CDO와 `/Game/Levels/SolarSystem`에 배치된 실제 인스턴스 양쪽에 같은 값을 저장했다.

- `PlanetDataAssets`: 위 6종 전체.
- `MinPlanet = 5`, `MaxPlanet = 7`.
- `MinMoon = 1`, `MaxMoon = 3`.
- `MinimumUniquePlanetTypes = 4`.
- 레벨 인스턴스의 Run별 seed 무작위화는 유지한다.

행성 선택은 다음 순서로 동작한다.

1. null, 중복 포인터, 가중치 0인 후보를 제거한다.
2. 각 후보의 DA override를 읽어 실제로 켜진 `ResourceV2.*` 규칙을 계산한다.
3. 5개 카드와 Common Ore, Biomass Feedstock을 모두 덮는 최소 고유 환경 조합을 결정적으로 찾는다.
4. 가능한 조합이 여러 개면 Run seed와 환경 가중치로 순서를 결정한다.
5. 최소 4종 환경이 될 때까지 중복 없이 보충한 뒤, 남는 행성 슬롯만 가중치 중복 추첨으로 채운다.
6. 카탈로그가 필수 자원을 제공할 수 없으면 누락 Rule ID를 명시적으로 오류 로그에 남긴다.

따라서 각 환경의 희귀도를 유지하면서도 환경 반복과 필수 카드·운영 자원 누락을 동시에 막는다. 같은 seed는 같은 행성 순서와 같은 포트폴리오를 만든다.

## Editor 에셋 위치

- 행성 DA: `/Game/StarRovers/Celestial/DataAssets/Planets`
- 환경별 Terrain Profile: `/Game/StarRovers/Surface/DataAssets/TerrainProfiles`
- 환경별 Biome: `/Game/StarRovers/Surface/DataAssets/Biomes/PlanetEnvironments`
- Generator Blueprint: `/Game/StarRovers/Generation/Blueprints/BP_SolarSystemGenerator`
- 실제 Generator 인스턴스: `/Game/Levels/SolarSystem`

기존 Temperate, Lava Ocean, BadLands DA는 갱신했고 Arid Desert, Frozen Ocean, Toxic Wetland DA는 새로 생성했다. 기존 Earth Profile은 Temperate와 Resource rule의 기준으로 재사용하며 생성 도구가 덮어쓰지 않는다.

## 재생성 및 검사

환경 에셋과 Generator 설정은 Editor 전용 Commandlet로 반복 생성할 수 있다.

```powershell
& 'D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'D:\Unreal Projects\StarRovers\StarRovers.uproject' `
  -run=SRGeneratePlanetEnvironmentContent `
  -unattended -nop4 -nosplash -nullrhi -DDC-ForceMemoryCache -log
```

저장된 에셋을 변경하지 않고 현재 값을 확인할 때는 `-InspectOnly`를 추가한다.

```powershell
& 'D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'D:\Unreal Projects\StarRovers\StarRovers.uproject' `
  -run=SRGeneratePlanetEnvironmentContent -InspectOnly `
  -unattended -nop4 -nosplash -nullrhi -DDC-ForceMemoryCache -log
```

Commandlet는 생성 후 다음 계약을 검사하며 하나라도 위반되면 실패 코드로 종료한다.

- 행성 DA 6개와 고유 Profile 6개가 존재한다.
- 모든 행성에 유효한 Shape, Profile, 양수 가중치가 있다.
- Profile마다 Biome 3개 이상과 Resource V2 카탈로그 규칙 7개가 있다.
- 각 행성은 그중 정확히 카드 2종과 운영 자원 1종만 활성화하고, 모두 양수 최소 생성 수를 갖는다.
- 필수 7개 자원은 카탈로그에서 각각 최소 2개 환경에 출처가 있다.
- 기후/지형 signature 6개가 서로 다르다.
- 96개 seed의 5~7행성 추첨에서 매번 최소 4종과 필수 자원 7종이 나오고, 전체 sweep에서 모든 환경이 등장한다.

## 자동 검증 결과

2026-07-27 기준으로 다음을 통과했다.

- `StarRoversEditor Win64 Development` 빌드 성공.
- `StarRovers.SolarSystem.PlanetEnvironment`: 4/4 성공.
  - 가중치 선택과 seed 결정성.
  - 필수 자원 포트폴리오 조합과 불가능 카탈로그 진단.
  - 실제 6개 DA 및 지형 분포 계약.
  - `/Game/Levels/SolarSystem` PIE 환경 다양성 및 실제 광맥 수.
- PIE 실측 예시: 7개 행성에서 5종 환경 생성, 모든 행성에 자원 3종, 시스템 전체 필수 자원 7종 확인.
- `StarRovers.ResourceSystem`: 62/62 성공.
- `StarRovers.UI`: 62/62 성공.
- `StarRovers.UI.Integration.PIE.ProjectHUDContract`: 1/1 성공.
- 최종 `-InspectOnly`: 오류 0, 경고 0, 레벨 Generator 인스턴스 정확히 1개 확인.

자동화 PIE 검증은 생성된 행성 Actor의 Body Category와 실제 `VariableName`을 읽어 행성 수와 고유 환경 수를 검사한다. 따라서 Blueprint 배열만 확인하고 실제 Run 생성을 건너뛰는 테스트가 아니다.
