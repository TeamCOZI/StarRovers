# 유한 광맥 경제와 Reserve 가시성 구현

## 목적

Resource V2 광맥은 이미 기술적으로 고갈될 수 있었지만, 실제 authored Deposit은 모두 50,000개를 담고 있었다. 기본 Miner가 4초마다 1개를 채굴해도 한 광맥이 약 55시간 지속되므로, 25~35분 Run에서는 사실상 무한 자원이었다.

이번 단계는 다음 계약을 하나로 묶는다.

1. 광맥량을 Resource Catalog의 명시적인 Balance 값으로 관리한다.
2. 항성 연료 Line의 실제 Energy와 10초 제작 주기로 Run 가능성을 검증한다.
3. 플레이어가 행성별 잔량과 고갈 압력을 즉시 볼 수 있게 한다.
4. Telemetry가 Capacity나 물류 문제와 자원 고갈을 구분한다.
5. 다음 단계의 Save/Migration이 저장해야 할 광맥 상태를 명확히 한다.

## 최종 authored 수치

| 자원 | 분류 | 광맥 하나의 총량 |
|---|---|---:|
| Helios Iron | Metal Card | 120 |
| Echo Quartz | Crystal Card | 120 |
| Verdant Spore | Organic Card | 120 |
| Aurora Plasma | Plasma Card | 120 |
| Null Pearl | Void Card | 120 |
| Common Ore | Utility 원료 | 180 |
| Biomass Feedstock | Utility 원료 | 180 |
| Industrial Supply | 제작 Utility | 자연 광맥 없음 |

`SRGenerateResourceV2Content`는 더 이상 Template 값과 50,000 중 큰 값을 사용하지 않는다. 각 Deposit Data Asset에 Catalog 값을 그대로 기록한다. authored-content 검증도 단순히 양수인지만 보지 않고 정확히 Catalog 값과 같은지 검사한다.

### Card 120개의 의미

항성 연료 한 Batch는 Card 5종을 하나씩 소비한다. 따라서 각 Card 광맥을 하나씩 확보한 “완전한 광맥 세트”는 120 Batch를 만든다.

~~~text
120 Batch × 제작 주기 10초 = 1,200초 = 20분
~~~

기본 Line의 첫 연료 도착을 05:30으로 두면 마지막 기본 Batch는 25:20에 도착한다. 이는 25분 확장 판단까지 첫 Line을 유지하지만, 기본 Line을 그대로 방치하면 승리하지 못하게 하는 길이다.

초기 후보였던 96개는 Simulation에서 기각했다. 기본 공급이 21:20에 끝난 뒤 항성이 23:29에 붕괴하여, 기존 25:00 확장 판단에 도달하지 못했기 때문이다.

### Utility 180개의 의미

Common Ore와 Biomass는 1:1로 Supply Fabricator에 들어간다. 한 원료 광맥 쌍은 180번의 Industrial Supply 제작 입력을 제공한다. Card보다 긴 지원 여유를 주되, Service Core와 Fleet Berth를 무제한 유지하는 영구 자원은 아니다.

비상 재탐사의 25 Card는 그대로다. 정상 광맥의 약 21%이며 첫 수직 Slice 복구에는 충분하지만, 20분짜리 정상 Line을 대체하지 못한다.

## 유한 공급 Run 계약

Balance Harness의 `SupplyStage`에 `MaximumDeliveryCount`를 추가했다.

- `0`: 기존과 같은 무제한 집계 공급
- 양수: 정확히 지정된 Batch 수를 전달한 뒤 공급 종료
- 결과: 전체 전달 Batch 수, 고갈된 Stage 수, 첫 고갈 Simulation 시각 기록

`FSRFiniteResourceEconomyModel`은 별도 숫자를 복사하지 않는다. 다음 권위 값을 읽어 계약을 만든다.

- Card 5종의 Catalog 광맥량
- Stellar Fuel Fabricator의 authored 10초 Cycle
- 실제 Fabricator 계산기의 기본 Full House 824 Energy
- 실제 Fabricator 계산기의 Distributed Convergence 1,180 Energy

따라서 Resource, Facility 또는 연료 공식을 바꾸면 유한 경제 테스트도 함께 변한다.

## 기준 Simulation 결과

### 기본 광맥 세트 하나

| 항목 | 결과 |
|---|---:|
| 생산 시작 | 05:00 |
| 첫 도착 | 05:30 |
| 연료 | 824 / 10초, 82.4/s |
| 최대 Batch | 120 |
| 마지막 도착 | 25:20 |
| 결과 | 패배 |
| 패배 시각 | 26:47 |

기본 Line은 충분한 학습·복구 시간을 주지만 최종 요구 처리량 100/s보다 낮다. 누적량만 쌓아 승리할 수도 없다.

### 25분 도착을 위한 원격 확장

| 항목 | 결과 |
|---|---:|
| 기본 Line | 05:00부터 824 / 10초, 최대 120 Batch |
| 최적화 Line | 23:00 생산 시작, 120초 운송 후 25:00 첫 도착, 별도 광맥 세트 |
| 결과 | 승리 |
| 승리 시각 | 25:39 |
| 총 전달 Batch 이벤트 | 124 |

이 기준은 “같은 기본 Line을 반복 배치”하는 대신 다음 중 하나를 요구한다.

- 다음 광맥을 개척해 최적화 Line을 추가한다.
- 첫 광맥이 남아 있을 때 Tag·Fuel Imprint를 더 일찍 완성해 Card당 Energy를 높인다.
- 여러 천체의 Card 공급을 하나의 고효율 제작기로 합친다.

원격 Line을 25:00에 생산하기 시작하면 120초 운송 뒤 첫 Batch가 27:00에 도착하여 26:47 붕괴보다 늦다. 23:00 선행 출발은 Route 지연을 실제 예측 문제로 남긴다.

Phase 22에서 실제 여섯 환경을 512 Seed로 검사한 결과, 모든 Seed가 4~10개의 완전 Card Front를 보장했고 실제 D3D12 PIE의 모든 광맥이 접근 가능했다. 따라서 120은 현재 기준 Balance로 유지한다. 세부 분포와 A/B/A Seed 재생 결과는 [ResourceEconomySeedSoakImplementation.md](ResourceEconomySeedSoakImplementation.md)에 정리되어 있다.

## Reserve 계산 모델

`FSRResourceReserveModel`은 광맥을 변경하지 않는 순수 집계기다.

계산 항목:

- 전체·활성·고갈·Legacy 무한 광맥 수
- 유한 총량과 잔량
- Card 잔량과 Common Ore/Biomass 잔량
- 접근 가능한 기준 Card 종류 수
- 현재 만들 수 있는 완전한 5장 Fuel Batch 수
- 가장 적어 Batch 수를 제한하는 Card Resource ID
- 만들 수 있는 Industrial Supply 입력 쌍 수
- 전체 유한 잔량 비율과 압력 단계

압력 경계:

| 잔량 비율 | 상태 |
|---:|---|
| 25% 초과 | Healthy |
| 10% 초과, 25% 이하 | Low |
| 0% 초과, 10% 이하 | Critical |
| 0% | Depleted |

Legacy 무한 광맥은 큰 정수 합계로 섞지 않고 별도 Flag로 유지하여 합산 Overflow와 가짜 비율을 막는다.

## UI

행성 Overview의 운영 배지는 다음처럼 표시된다.

~~~text
L 18/30 | R 24%
~~~

- `L`: 현재 Operational Load / Capacity
- `R`: 해당 천체의 유한 광맥 잔량 비율

천체 Focus의 Operations 패널은 다음 행을 추가한다.

~~~text
RESERVES 24% | VEINS 5/7 | CARD 180 | RAW 60
LOW RESERVES - prepare the next Miner
~~~

Low는 다음 Miner 준비, Critical은 즉시 재배선, Depleted는 현재 천체가 Line을 유지할 수 없다는 행동 문구를 사용한다. Tooltip은 총량, 활성·고갈 광맥 수와 Card/원료 잔량을 설명한다.

## Telemetry

전역 Run Snapshot은 모든 천체의 광맥을 합쳐 다음을 기록한다.

- 최소 Reserve 비율
- 남은 완전 Fuel Batch 수
- 제한 Card Resource ID
- 전역 Deposit/고갈 상태

최종 처리량 실패 다음, Capacity 진단 전에 `ResourceDepletion` 병목을 판정한다. 완전한 5장 Batch가 하나도 남지 않았거나 전역 Reserve가 Critical/Depleted이면 `핵심 자원 고갈`로 보고한다.

~~~text
ReserveMin=8% FuelBatches=0 Limiting=NullPearl Bottleneck=핵심 자원 고갈
~~~

## 검증

- Phase20 순수 테스트 6개: Catalog, Reserve 집계, Legacy 무한, Run Envelope, UI 문자열, Telemetry 병목
- 전체 `StarRovers.ResourceSystem`: 78/78 성공
- 전체 `StarRovers.UI`: 63/63 성공
- authored-content Catalog: 실제 7개 Deposit Data Asset이 120/180과 정확히 일치
- D3D12 SolarSystem PIE: 생성된 모든 V2 광맥이 유한 Catalog 값이며 고갈 광맥 0
- 같은 PIE: 전역에서 Card 5종이 모두 존재하고 완전 Batch 수가 양수
- 같은 PIE: Focus Operations에 실제 `ResourceReserveTextBlock`이 생성되고 표시

## Save/Migration 연결

11단계에서 Deposit 총량·잔량 DTO, 50,000 Placeholder의 비율 보존 이관, 고갈 광맥 복원, Legacy 무한과 Resource V2 유한 광맥의 명시적 분리를 구현했다. Run Telemetry는 권위 상태가 아니므로 저장하지 않고 Load 직후 Reserve Snapshot을 재구축한다.

구체 Schema와 원자적 복원 순서는 [ResourceV2RunSaveMigrationImplementation.md](ResourceV2RunSaveMigrationImplementation.md)에 정리되어 있다.

12단계 다중 Seed Soak를 완료했다. 512/512 Seed가 행성 수·환경 다양성·자원 Coverage·유한 Spawn Envelope를 통과했고, 실제 PIE에서 같은 Seed의 Resource Cell까지 재현됨을 확인했다. Card 120 / Utility 180은 유지하며 결과는 [ResourceEconomySeedSoakImplementation.md](ResourceEconomySeedSoakImplementation.md)를 따른다.
