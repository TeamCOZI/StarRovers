# Family Facility 역할 및 처리량 재조정

## 1. 목표

이 단계는 공용 가공 설비만 반복해도 모든 Family의 긍정 State 보상을 얻을 수 있던 문제를 제거하고, 각 Family가 서로 다른 처리량·Operational Load 선택을 갖도록 만든다.

핵심 원칙은 다음과 같다.

1. `Pulse Processor`와 `Compression Mill`은 막힌 Line을 이어 주는 `Universal Bridge`다.
2. Bridge도 가공 이력과 부정 State 압력을 진행시키지만, 긍정 Family Merit를 활성화하거나 소비하지 못한다.
3. 긍정 Merit는 해당 Family 전용 Facility 또는 Family 전용 Conditioned Hold만 사용할 수 있다.
4. 모든 일반 가공은 합연산이고, 곱연산은 항성 연료 제작기에만 남는다.
5. 가공 횟수 제한은 없으며, 긴 직렬 Line은 Refinement Resistance·시간·Load로 비용을 낸다.

## 2. Facility Line Role

Family 가공 설비는 Build Dock과 Data Asset에서 다음 역할 중 하나를 명시한다.

| Role | 의미 | 대표 설비 |
|---|---|---|
| Universal Bridge | 모든 Card를 처리하는 저효율 연결·복구 수단 | Pulse Processor, Compression Mill |
| Primer | 다음 전용 Payoff를 준비 | Induction Forge, Growth Vat |
| Payoff | 준비된 긍정 State를 Energy로 회수 | Cryo Press, Enzyme Loom, Spore Press, Echo Chamber |
| Repeater | 제한된 반복 묶음으로 이득을 축적 | Resonance Mill |
| Recovery | 부정 State 또는 반복 이력을 초기화 | Annealing Chamber, Facet Shifter |
| Burst | 짧은 시간에 높은 Load로 증폭 | Arc Amplifier |
| Stabilizer | Burst의 부정 State를 해제 | Grounding Coil |
| Sacrifice | 현재 Energy를 의도적으로 다음 보상으로 전환 | Null Sink |

`Line Role`은 UI와 밸런스 계약을 위한 메타데이터다. 실제 State 판정은 계속 공용 Resource Processing Kernel 한 곳에서 수행한다.

## 3. 공용 Bridge 규칙

| Facility | Energy | 기본 Cycle | Load | 용도 |
|---|---:|---:|---:|---|
| Pulse Processor | +1 | 2초 | 1 | 가장 싼 단일 연결, 낮은 지연 |
| Compression Mill | +3 | 5초 | 3 | 한 단계당 높은 기본 증가, 느리고 무거움 |

Bridge가 Family별로 만드는 결과는 다음과 같다.

- Metal: Work Strain은 증가하지만 Hot -> Cold Tempered를 완성하지 못한다.
- Crystal: 같은 Bridge를 반복하면 반복 횟수와 Fractured 위험은 쌓이지만 Resonant는 켜지지 않는다.
- Organic: Growth로 준비한 Matured를 운반할 수는 있지만 보너스를 소비하지 못한다. Growth 없는 일반 가공 횟수는 증가한다.
- Plasma: Amplification Action이 아니므로 Burst를 만들 수 없다. 증폭 순서를 벗어나면 기존 Energized는 끊긴다.
- Void: 양의 Energy Bridge는 Collapsed 압력을 진행시키지만 준비된 Echo를 소비하거나 Echo 보너스를 받지 못한다.

따라서 Bridge는 잘못 연결된 카드도 완전히 막지 않는 안전망이지만, 어느 Family에서도 전용 주기의 최적 대체재가 아니다.

## 4. Family Facility 기준값

| Facility | Role | Energy | 기본 Cycle | Load | 핵심 선택 |
|---|---|---:|---:|---:|---|
| Induction Forge | Primer | +4 | 4초 | 3 | Hot 준비 |
| Cryo Press | Payoff | +3 | 4초 | 3 | Cold에서 Tempered +5 가능 |
| Annealing Chamber | Recovery | +0 | 6초 | 2 | Work Strain과 Metal State 초기화 |
| Resonance Mill | Repeater | +3 | 3초 | 2 | 같은 Archetype 2~3회 묶음 |
| Facet Shifter | Recovery | +2 | 3초 | 2 | Archetype 전환으로 Fractured 회복 |
| Growth Vat | Primer | +0 | 5초 | 1 | Matured 준비, Depleted 해제 |
| Enzyme Loom | Payoff | +2 | 2초 | 2 | 빠른 Organic 처리량 |
| Spore Press | Payoff | +5 | 5초 | 1 | 느리지만 카드당 Energy와 Load 효율 우수 |
| Arc Amplifier | Burst | +4 | 2초 | 5 | 최고 속도, 높은 Capacity 비용 |
| Grounding Coil | Stabilizer | +1 | 3초 | 1 | Energized·Overloaded 해제 |
| Null Sink | Sacrifice | 최대 -3 | 2초 | 1 | 실제 희생량 저장 |
| Echo Chamber | Payoff | +5 | 5초 | 3 | 저장한 Echo 보너스 소비 |

Organic의 두 Payoff는 의도적으로 우열이 한 축으로 정해지지 않는다. Enzyme Loom은 빠르고 Spore Press는 느리지만 낮은 Load와 높은 카드당 결과를 제공한다. Plasma는 가장 빠르지만 기준 주기 전체에 Load 11을 약속해야 한다. Void는 낮은 Load로 시작하지만 먼저 Energy를 포기하고 Echo 단계가 완료되어야 이득을 얻는다.

## 5. 결정론적 기준 Cycle

`FSRFamilyLineBalanceV2`는 World와 Tick 없이 실제 Kernel, Facility Catalog, Refinement Resistance Scale 40을 사용해 한 카드의 기준 주기를 계산한다.

| Family | 기준 주기 | Energy 순증가 | 유효 시간 | Load-seconds | 설치 단계 Load 합 |
|---|---|---:|---:|---:|---:|
| Metal | Forge -> Cryo -> Anneal | +12 | 14.400초 | 37.200 | 8 |
| Crystal | Mill x3 -> Shifter | +19 | 14.250초 | 28.500 | 8 |
| Organic | Growth -> Enzyme | +8 | 7.000초 | 9.000 | 3 |
| Plasma | Amplifier x2 -> Ground | +14 | 8.175초 | 24.975 | 11 |
| Void | Sink -> Echo | +7 | 7.000초 | 17.000 | 4 |

이 표의 목적은 모든 Family를 같은 수치로 만드는 것이 아니다.

- Metal은 안정적인 열 순환과 명시적인 회복 설비를 요구한다.
- Crystal은 중간 Load에서 가장 큰 한 묶음 이득을 얻지만 네 번째 반복 전에 전환해야 한다.
- Organic은 Energy/Load-second가 가장 높고 속도형·수율형 Payoff를 선택한다.
- Plasma는 Energy/초가 가장 높지만 설치 단계 Load도 가장 높다.
- Void는 낮은 설치 Load와 강한 후속 보상을 얻는 대신 Sacrifice가 중단 위험을 만든다.

## 6. UI와 Data Asset

- `FSRFacilityProcessDefinitionV2.LineRole`이 실제 Facility Data Asset에 저장된다.
- Build Catalog가 Line Role을 캐시하고, Build Dock 흐름도는 `PRIMER`, `PAYOFF`, `BURST`, `BRIDGE`처럼 한 단어로 표시한다.
- Bridge는 `NO FAMILY MERIT`를 함께 표시하며 Tooltip에서 부정 State 압력은 계속 진행된다고 설명한다.
- Facility Preview는 실행 입력에 대해 `Family Specialist` 또는 `Universal Bridge`를 표시한다.
- `SRGenerateResourceV2Content` commandlet이 21개 Facility와 대응 Structure 설명을 새 계약으로 다시 저장한다.

## 7. 검증 계약

자동화 테스트는 다음을 고정한다.

1. 14개 공용·Family 가공 설비의 Role, Energy, Cycle, Load 행렬
2. 다섯 Family 기준 Cycle의 Energy, 실제 저항 적용 시간, Load-seconds
3. Organic이 기준 Capacity 효율형이고 Plasma가 고속·고부하형이라는 상대 관계
4. Universal Bridge가 Metal Tempered, Crystal Resonant, Organic Matured, Void Echo를 가로채지 못한다는 규칙
5. 실제 authored `.uasset`의 Bridge·Organic·Plasma·Void 대표 계약
6. 기존 항성 연료 Vertical Slice와 PIE ResourceSystem Baseline

다음 단계인 Tag·Augment 조정은 이 수치를 직접 덮어쓰는 전역 배율보다, 어느 Primer·Payoff·Recovery 지점을 선택할지 바꾸는 방식으로 설계한다.
