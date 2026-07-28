# 초기 진행 보장과 Soft-lock 복구 구현

## 목표

첫 Run에서 플레이어가 실제로 `Card 광맥 → 채굴기 → 같은 Family 가공 설비 → 항성 연료 제작기 → Hub`로 진행할 수 있어야 한다. 잘못된 설비를 먼저 짓거나 진행 중 설비를 철거해도 Run을 다시 시작할 필요가 없어야 한다.

복구 장치는 정상적인 탐사와 천체 간 물류를 대체하지 않는다. 따라서 광맥을 무한 재생시키지 않고, 생성 보장·실시간 재판정·첫 Card 이전의 1회성 안전망으로 역할을 분리했다.

## 유한 광맥 계약

- Resource V2 광맥은 Structure Data Asset의 `DepositTotalAmount`를 실제 총량으로 사용한다.
- 채굴 완료마다 `RemainingAmount`가 정확히 1 감소한다.
- 잔량이 0이면 채굴 대상 탐색, Miner 강조, System Scan 추천에서 제외된다.
- 여러 Miner가 마지막 Card를 경쟁하면 먼저 완료한 Miner만 소비·출력하고, 대상 상실 Miner의 임시 Processing Inventory는 즉시 정리된다.
- `DepositTotalAmount == 0`인 Legacy 광맥만 기존 무한량 계약을 유지한다.
- 실제 Resource V2 광맥은 Card 120개, Common Ore와 Biomass 180개를 사용한다. 수치 근거와 Run Envelope는 [FiniteResourceEconomyBalanceImplementation.md](FiniteResourceEconomyBalanceImplementation.md)를 따른다.
- Miner Inspector는 `Remaining / Total`, `DEPLETED / Total`, `Infinite`를 구분해 표시한다.

## 생성 시 접근성 보장

자연 구조물 생성기는 광맥을 놓기 전에 최소 한 개의 인접 Miner 접근 Cell을 확인한다.

- 비어 있고 건설 가능한 Cell은 즉시 유효하다.
- 건설 시 제거할 수 있는 일반 자연 구조물이 있는 Cell도 유효하다.
- 인접 접근 공간이 하나도 없는 후보에는 광맥을 생성하지 않는다.
- `MinimumGuaranteedCount` 보정 Pass에도 같은 접근성 규칙을 적용한다.

이 규칙은 광맥 수량 보장과 별개다. 즉, “광맥은 존재하지만 Miner를 붙일 수 없는” 가짜 보장을 허용하지 않는다.

## System Scan 안전 보고서

시작 System Scan은 다음 수치를 함께 계산한다.

- 전체 Card 광맥 수
- 잔량이 있는 Card 광맥 수
- 고갈된 Card 광맥 수
- 인접 건설 공간이 없는 Card 광맥 수
- 실제로 시작 가능한 후보 수
- 항성 연료용 기준 Card 5종 중 접근 가능한 종류 수와 누락 Resource ID
- 각 추천 광맥의 총량과 잔량

후보가 유효하려면 Resource Data, Family, 잔량, Family 가공 설비 접근성, 인접 Miner 공간을 모두 충족해야 한다. 고갈 광맥은 높은 Seed Energy나 항성 접근성 점수를 가져도 유효 후보보다 앞설 수 없다.

최초 추천은 계속 흔들리지 않는다. 다만 첫 Card 생산 전에 추천 광맥이 고갈되거나 접근 불가능해지고 유효한 채굴기가 없다면 Scan을 다시 계산해 다른 실제 후보로 전환한다.

## 잘못된 첫 선택과 철거 복구

첫 연료 Milestone의 완료 이력은 유지하지만, 직접 행동 대상은 매 Refresh마다 현재 World 상태로 다시 검증한다.

- Card 광맥에 붙지 않은 Miner와 운영 자원용 Miner는 첫 Card 채굴기로 세지 않는다.
- 첫 Card가 실제로 생산되면 System Scan의 추천 Family 대신 그 Card의 실제 Family를 확정한다.
- 첫 Card와 다른 Family 전용 가공 설비는 첫 가공 Milestone을 충족하지 않는다.
- 채굴기, 같은 Family 가공기, 항성 연료 제작기, Hub가 철거되면 죽은 Inspect 버튼을 남기지 않는다.
- 해당 단계에서 `다시 배치` 행동을 열어 Build Dock의 정확한 Role과 Family를 다시 선택한다.
- 이미 생산된 Card와 항성 연료, 완료된 과거 Milestone은 되돌리지 않는다.

따라서 잘못된 시설을 지은 행위는 공간과 Capacity 비용을 만들지만 Run 자체를 폐기시키지는 않는다.

## 비상 재탐사

정상 생성 보장과 대체 후보 재탐색으로도 시작 후보가 0개일 때만 Command Lane에 `비상 재탐사 1회 사용`을 표시한다.

기본 규칙은 다음과 같다.

- 설정: `bEnableEmergencyProspectingRecoveryV2 = true`
- 제공량: `EmergencyProspectingCardAmountV2 = 25`
- 사용 가능 시점: 첫 Card 생산 전
- 사용 횟수: Run당 1회 시도
- 우선 복구: 접근 가능한 고갈 광맥 중 Seed Energy가 높은 광맥
- 대체 복구: 기존 Card 광맥 Template을 사용해 항성에 가까운 건설 천체의 접근 가능한 Cell에 비상 광맥 생성
- 최종 Fallback: Helios Iron 광맥 Template
- 성공 후 System Scan을 즉시 다시 실행하고 새 광맥을 정상 추천 후보로 사용

25장은 첫 자동화 수직 Slice를 복구하기 위한 양이다. 무한 광맥, 주기적 무료 보급, 첫 Card 이후의 상시 보험으로 확장하지 않는다. 중후반 고갈은 여러 광맥 활용과 천체 간 물류가 해결해야 한다.

## 현재 경계

- Water Role 자체가 아직 Surface Grid의 `bCanConstruct`를 막지 않는다. 바다·위험 지형의 실제 건설 금지는 Terrain/Placement 단계에서 별도로 연결해야 한다.
- 광맥 잔량과 비상 재탐사 사용 여부는 Save Schema에 포함된다. 따라서 Load로 25장 안전망을 다시 받거나 이미 고갈된 광맥을 되살릴 수 없다.
- 중후반 광맥 고갈은 실제 Run 병목이며 행성 Operations와 전역 Telemetry에 표시된다. Load 직후 Reserve Telemetry도 복원된 광맥에서 다시 계산된다.
- 비상 재탐사는 기술적 Soft-lock 안전망이며 최적 전략으로 사용해도 이득이 되지 않도록 제공량을 작게 유지한다.

## 검증

2026-07-28 기준 다음 계약을 자동화했다.

- 양수 총량은 유한, 0인 Legacy 총량은 무한으로 해석된다.
- 유한 광맥은 채굴마다 1 감소하고 0에서 추가 채굴을 거부한다.
- 고갈 후보는 System Scan 유효 후보가 아니다.
- 5종 Card 포트폴리오 누락을 Scan이 식별한다.
- 후보가 0개이면 비상 재탐사 직접 행동을 표시한다.
- 철거된 채굴기는 Inspect가 아니라 재배치 행동을 표시한다.
- 실제 SolarSystem PIE에서 모든 Resource V2 광맥이 Data Asset의 유한량으로 등록된다.
- 실제 추천 광맥을 1회 채굴하면 잔량이 1 감소하고 Resource ID가 Scan 결과와 일치한다.
- 실제 생성 System은 접근 가능한 추천 광맥과 항성 연료용 Card 5종 포트폴리오를 제공한다.

검증 명령 결과:

- `StarRoversEditor Win64 Development`: 성공
- `StarRovers.ResourceSystem`: 78/78 성공
- `StarRovers.UI`: 63/63 성공
- `StarRovers.SolarSystem.PlanetEnvironment`: 4/4 성공
