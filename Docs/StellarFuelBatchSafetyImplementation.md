# 항성 연료 5장 Batch 안전성과 Inspector 구현

## 목적

항성 연료 제작기는 다섯 개의 독립 Line이 만나는 최종 합성 지점이다. 이전 구현도 다섯 입력을 한 Cycle 단위로 예약했지만 다음 문제가 남아 있었다.

- 정상 Card 1~4장이 도착한 상태가 `RECIPE MISMATCH`처럼 보였다.
- 어느 입력 Lane이 비었는지, 현재 Grade 조합이 어떤 족보인지 알 수 없었다.
- Utility, 구 Schema Card, 잘못된 Energy, 알 수 없는 Fuel Imprint가 입력에 들어가면 해당 Lane이 영구 정체될 수 있었다.
- 다섯 장이 Processing Inventory로 이동했다는 사실과 최종 예상 Energy가 Inspector에 드러나지 않았다.

Phase 19는 최종 `A + B × C` 공식과 기존 족보 수치는 바꾸지 않고, 합성 직전의 입장 검사·Batch 분석·예약·표시 계약을 완성한다.

## Batch 상태 기계

~~~text
EMPTY 0/5
   ↓ 유효 Card 도착
COLLECTING 1~4/5
   ↓ 다섯 Lane 충족
READY 5/5 + 최종 공식 Preview
   ↓ Cycle 시작 시 원자적 이동
RESERVED 5/5 + Processing 진행
   ↓ Cycle 완료
Stellar Fuel 출력

어느 단계에서든 기존 Save의 잘못된 입력 발견
   → CONTAMINATED + Lane 번호와 원인 표시
~~~

`FSRStellarFuelBatchPlanner`는 Runtime을 변경하지 않는 순수 분석기다. 대기 중에는 다섯 Input Port의 다음 자원 하나씩을 읽고, 처리 중에는 `ProcessingInventory`의 예약된 다섯 장을 읽는다.

분석 결과에는 다음 정보가 포함된다.

- 유효 Card 수와 빈 Lane 번호
- 각 Lane의 `R2`, `B2`, `G4` 형태 Card Key
- 현재 Grade 조합이 만드는 족보
- 정확히 같은 `Spectrum + Grade` 중복 수
- 입력 Energy 합
- 완성된 다섯 장의 B, C, 최종 Fuel Energy
- 오염된 Lane 번호와 거부 원인

## 입력 오염 차단

Stellar Fuel Fabricator가 허용하는 입력은 다음 조건을 모두 만족해야 한다.

1. 현재 Resource Schema를 사용하는 `Card`다.
2. 지원하는 Family와 Spectrum을 가진다.
3. Grade가 현재 허용 범위 안에 있다.
4. Current Energy가 유한하며 0 이상이다.
5. Fuel Imprint가 비어 있거나 Catalog에 등록되어 있다.

`FSRStellarFuelFabricator::ValidateInputCard`가 최종 계산과 입력 경계에서 같은 규칙을 사용한다. `FSRFacilityInputAcceptancePolicy`는 이를 직접 투입과 Conveyor 수신 양쪽에 적용한다.

거부는 목적지 Inventory를 수정하기 전에 일어난다. 따라서 잘못된 자원은 사라지거나 제작기 안으로 이동하지 않고 출발지에 남는다. 다른 Facility와 Legacy Ruleset의 입력 정책은 이번 단계에서 변경하지 않았다.

이전 Save나 Debug 조작으로 이미 들어간 잘못된 자원은 자동 삭제하지 않는다. Inspector가 `BATCH CONTAMINATED`, `Lane 2 blocked`처럼 정확한 위치와 원인을 표시하여 플레이어가 회수하거나 경로를 수정하게 한다.

## 원자적 예약

다중 입력 합성은 실제 Input Port에서 차례로 자원을 꺼내지 않는다.

1. Input Port 전체를 복사한다.
2. 복사본에서 필요한 모든 입력을 꺼내 본다.
3. 다섯 Lane이 모두 성공한 경우에만 Port 복사본과 Processing Inventory를 한 번에 Commit한다.
4. 하나라도 실패하면 Runtime 상태를 전혀 변경하지 않는다.

이미 예약된 Processing Inventory가 있으면 두 번째 예약 시도도 거부한다. 따라서 불완전 Batch가 일부만 소비되거나 진행 중 Batch가 덮어써지는 경로가 없다.

## 중복 Card Key 규칙

정확히 같은 `Spectrum + Grade` Card도 유효한 Energy 입력이다. 중복 Card의 Energy는 B에 모두 더해지지만 족보 판정에는 Card Key 하나로만 센다.

이 선택은 Soft-lock을 막기 위해 의도적으로 유지한다. 좋은 족보를 만들지 못한 초기 Run도 다섯 장을 모으면 Unranked 연료를 만들 수 있다. Inspector는 중복을 차단하지 않고 다음과 같이 비용을 설명한다.

~~~text
DUPLICATE * | 1 Card adds Energy but scores once
~~~

## Inspector 표시

| Runtime 상태 | 상단 상태 | 즉시 보이는 정보 |
|---|---|---|
| 0장 | `ASSEMBLING 0/5` | 다섯 Card Lane 연결 안내 |
| 1~4장 | `ASSEMBLING 3/5` | 현재 족보, 빈 Lane 번호, Lane별 Card Key |
| 완성 | `READY` | 완성 족보, 예상 B/C와 Fuel Energy |
| 처리 중 | `BATCH RESERVED` | 다섯 장 잠금, 같은 최종 Preview, Cycle 진행률 |
| 오염 | `BATCH CONTAMINATED` | 막힌 Lane, 거부 원인, 회수·재배선 지시 |

예시는 다음과 같다.

~~~text
5-CARD SYNTHESIS | Hand + Seals + Imprints
BATCH 3/5 | Current: One Pair | Empty lanes: 4, 5

CARD LANES | 1:R2  2:B2  3:G4  4:EMPTY  5:EMPTY
WAITING | Fill lanes 4, 5
~~~

완성된 Reference Batch는 소비 전에 다음 계산을 보여 준다.

~~~text
READY 5/5 | Full House | Fuel E 1180.0
FINAL | A 0.0 + B 236.0 x C 5.0 = 1180.0
~~~

## 설계상 지켜야 할 규칙

- 1~4장의 정상 입력을 오류로 취급하지 않는다.
- Unranked와 중복 Batch를 금지하지 않는다. 경고와 기회비용만 보여 준다.
- Preview와 실제 완성은 반드시 같은 `FSRStellarFuelFabricator` 계산기를 사용한다.
- 오염 해결을 위해 입력을 자동 삭제하거나 다른 Lane으로 몰래 옮기지 않는다.
- 예약 이후에는 도착한 다음 Card가 현재 Cycle의 족보나 Energy를 바꾸지 못한다.
- 일반 가공에는 곱연산을 추가하지 않는다. `A + B × C`는 여전히 최종 제작기에서 한 번만 실행한다.

## 자동 검증

- `StarRovers.ResourceSystem.Phase19.StellarFuelBatch.PlannerStates`
- `StarRovers.ResourceSystem.Phase19.StellarFuelBatch.NonDestructiveAdmission`
- `StarRovers.ResourceSystem.Phase19.StellarFuelBatch.AtomicReservation`
- `StarRovers.UI.FacilityInspector.StellarFuelBatchStates`
- 전체 `StarRovers.ResourceSystem`, `StarRovers.UI` 회귀
- D3D12 `StarRovers.UI.FacilityInspector.PIE.NativeFlow`

2026-07-28 검증 결과:

- `StarRoversEditor Win64 Development`: 성공
- Phase 19 ResourceSystem: 3/3 성공
- Phase 19 Inspector Presentation: 1/1 성공
- 전체 `StarRovers.ResourceSystem`: 72/72 성공
- 전체 `StarRovers.UI`: 63/63 성공
- D3D12 Facility Inspector PIE: 1/1 성공

## 수동 PIE 체크리스트

1. 제작기에 Card Conveyor를 하나씩 연결하고 `0/5`부터 `5/5`까지 증가하는지 본다.
2. 3장 상태에서 현재 족보와 비어 있는 Lane 번호가 맞는지 확인한다.
3. 같은 Card Key를 두 Lane에 보내 중복 경고가 나오되 합성이 막히지 않는지 본다.
4. Utility Conveyor가 제작기 입력에서 거부되고 출발 쪽에 남는지 본다.
5. 다섯 장이 모였을 때 표시된 최종 Energy와 실제 출력 Energy가 같은지 본다.
6. Cycle이 시작되면 `BATCH RESERVED`와 진행률이 보이고, 다음 Card가 현재 Preview를 바꾸지 않는지 본다.
