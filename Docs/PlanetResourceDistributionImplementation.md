# 행성 자원 분포 구현

## 목적

기존에는 모든 행성이 Resource V2 광맥 7종을 낮은 확률로나마 전부 생성했다. 환경별 대표 자원은 더 많았지만, 어느 행성에서든 모든 자동화 Line을 만들 수 있어 천체 탐색과 행성 간 물류의 필요성이 약했다.

이번 단계는 다음 두 조건을 동시에 만족하도록 개편했다.

- 한 행성은 분명한 자원 정체성을 가진다.
- 한 Run의 Solar System 전체는 항성 연료와 운영 경제에 필요한 자원을 잃지 않는다.

## 최종 분포

| 환경 | 주산 카드 | 부산 카드 | 운영 자원 |
|---|---|---|---|
| Temperate | Verdant Spore | Helios Iron | Biomass Feedstock |
| Arid Desert | Helios Iron | Echo Quartz | Common Ore |
| Frozen Ocean | Echo Quartz | Null Pearl | Common Ore |
| Badlands | Null Pearl | Verdant Spore | Common Ore |
| Lava Ocean | Aurora Plasma | Helios Iron | Common Ore |
| Toxic Wetland | Verdant Spore | Aurora Plasma | Biomass Feedstock |

한 행성에는 위 3종만 생성된다. 이전처럼 나머지 자원이 미량으로 섞이지 않는다.

Family 출처 수는 Metal 3, Organic 3, Crystal 2, Plasma 2, Void 2다. 운영 자원은 Common Ore 4, Biomass Feedstock 2개 환경에서 나온다. 어느 필수 자원도 단 하나의 환경에만 의존하지 않는다.

## 광맥 수 계약

| 역할 | 최소 보장 | 최대 | 최소 Cell 간격 |
|---|---:|---:|---:|
| 주산 카드 | 6 | 10 | 6 |
| 부산 카드 | 4 | 7 | 8 |
| 운영 자원 | 5 | 8 | 7 |
| 부재 | 0 | 0 | - |

자연 구조물 규칙에 `MinimumGuaranteedCount`를 추가했다. 일반 확률 Pass 뒤 최소 수가 부족하면 결정적 후보 순서를 다시 순회하면서 확률만 생략한다. 유효 Cell, Footprint, 점유, 간격 검사는 그대로 적용한다.

이 값은 “광맥 총량”과 다르다. 각 광맥의 매장량은 기존 Resource V2 Deposit DA에 유지되며, 이번 수치는 행성에서 발견할 수 있는 광맥 군집 수를 제한한다.

## Solar System 포트폴리오 선택

Generator의 `RequiredSystemResourceRuleIds`에는 다음 7개 규칙이 저장된다.

- Helios Iron
- Echo Quartz
- Verdant Spore
- Aurora Plasma
- Null Pearl
- Common Ore
- Biomass Feedstock

선택기는 각 환경의 Profile 규칙과 Planet override를 합쳐 실제 활성 자원을 계산한다. 이후 요청된 5~7개 슬롯 안에서 필수 7종을 모두 덮는 최소 고유 환경 부분집합을 먼저 찾고, 최소 환경 다양성 4종을 채운 다음 남은 슬롯을 가중치 추첨한다.

동일 seed에서는 가중 순서, 포트폴리오, 최종 행성 순서가 동일하다. 가능한 조합이 없을 때는 best-effort 행성 집합을 반환하되 누락된 Rule ID를 Selection Report와 오류 로그에 남긴다. 후속 초기 진행·복구 단계에서는 이 진단을 실제 복구 행동과 연결한다.

## 실제 Editor 상태

Commandlet가 다음을 함께 갱신했다.

- 행성 DA 6개: 환경별 3종 활성 override와 최소/최대 수.
- Terrain Profile 6개: 공통 Resource V2 규칙 카탈로그.
- `BP_SolarSystemGenerator` CDO: 필수 7종 Rule ID.
- `/Game/Levels/SolarSystem` Generator 인스턴스: 같은 필수 7종 Rule ID와 5~7행성, 4종 이상 환경 계약.

## 검증

2026-07-27 기준 결과다.

- `StarRoversEditor Win64 Development` 빌드 성공.
- 환경 저작 Commandlet: 오류 0, 경고 0.
- 96 seed 저작 검증: 모든 Run이 5~7행성, 4종 이상 환경, 필수 자원 7종 충족.
- `StarRovers.SolarSystem.PlanetEnvironment`: 4/4 성공.
- 실제 PIE: 7개 행성, 5종 환경, 행성마다 정확히 자원 3종, 각 자원 최소 4개 이상, 시스템 필수 7종 확인.
- `StarRovers.ResourceSystem`: 62/62 성공.
- `StarRovers.UI`: 62/62 성공.
- `StarRovers.UI.Integration.PIE.ProjectHUDContract`: 1/1 성공.

## 후속 단계와 경계

첫 진입 접근성, 유한 광맥, 잘못된 첫 선택과 철거 복구, 시작 System Scan의 포트폴리오 진단, 첫 Card 전 1회성 비상 재탐사는 `InitialProgressRecoveryImplementation.md`에서 구현했다.

- Moon을 희귀·보조 자원 거점으로 확장할지 여부.
- 중후반 광맥 고갈을 Family 처리량과 천체 간 물류 경제로 해결하는 규칙.
- Water Role과 위험 지형을 실제 건설 불가능 Cell로 연결하는 Placement 계약.
- 광맥 잔량의 Save 영속화와 Migration.
