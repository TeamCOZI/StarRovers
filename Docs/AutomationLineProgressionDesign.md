# 자동화 Line 및 진행 시스템 설계

## 1. 문서 범위

이 문서는 다음 시스템의 현재 권고안을 정의한다.

- Tag의 역할과 제한
- Facility의 역할과 진행 단계
- Technology와 Augment의 책임 분리
- 지역 및 천체 간 자동화 Line
- Augment에 따른 Line 변화
- 진행 불가 Run과 매번 같은 Run을 방지하는 규칙
- 대규모 물류망의 관리 및 Simulation 요구사항

자원 정체성, Family State, Energy 계산, Spectrum, Rank는 [ResourceSystemDesign.md](ResourceSystemDesign.md)에 정의되어 있다.

구체적인 Resource, Tag, Facility, Augment 수치와 세 가지 완성형 자동화망은 [AutomationLineReferenceRun.md](AutomationLineReferenceRun.md)에 정리되어 있다.

이 문서는 기획 문서다. 정확한 수치와 구현 구조는 아직 최종 확정이 아니다.

## 2. 규모 전제

현재 게임 방향은 다음과 같다.

- 한 Run에 행성 약 5-7개
- 위성 약 1-3개
- Cube Sphere 형태의 천체
- 천체당 Face 6개
- Face당 512 x 512 Surface Cell
- 바다 등으로 약 절반은 설치 불가
- 지표면 Conveyor와 Facility
- Hub 간 우주선 Route
- Star Fuel Missile 발사

한 천체의 전체 Cell 수:

~~~text
6 * 512 * 512 = 1,572,864
~~~

절반만 건설 가능해도 약 786,432개의 Cell이 남는다. 따라서 전체 지도 면적은 가공 Line 길이를 제어하는 주된 제약이 될 수 없다.

실제로 희소한 공간은 다음과 같은 국소 지점이다.

- 광맥 주변
- 가치 있는 온도대 또는 Biome
- Hub와 Route 종점 주변
- 바다 사이의 좁은 육지
- 공용 지원 Facility 주변
- 유용한 행성 환경이 있는 지역

설계는 다음 두 형태를 모두 지원해야 한다.

1. 하나의 천체 안에서 완성되는 Line
2. 여러 행성과 위성에 순서대로 가공 구간이 나뉜 Line

## 3. 핵심 설계 원칙

진행 시스템의 중심 원칙은 다음과 같다.

> 반드시 필요한 기능은 확정적으로 제공하고, 무작위 선택은 효율과 Line 형태를 특화한다.

Technology 또는 확정 Discovery는 작동하는 Line에 필요한 모든 기능을 제공한다. Augment는 조건부 효율, 대체 배치 방식, Run의 개성을 제공한다.

최소 Line은 언제나 작동해야 한다.

~~~text
Extractor
-> Core Processor
-> Hub 또는 지역 Buffer
-> 항성 연료 제작기
~~~

유효한 자원 5개는 항상 Star Fuel을 만든다.

- 족보 보너스는 선택 사항이다.
- Tag는 선택 사항이다.
- 중복 Card Key는 족보 품질을 낮추지만 Batch를 무효화하지 않는다.
- 부정 Family State는 효율을 낮추지만 가공을 막지 않는다.
- 기본 생산에 천체 간 운송이 필수는 아니다.

## 4. 시스템별 책임

| 시스템 | 주된 책임 | 통제해서는 안 되는 것 |
|---|---|---|
| Resource Family | 자원 고유의 가공 문법 | 기본 생산 기능에 대한 무작위 접근 |
| Family State | 긍정·부정 가공 주기 | 자원의 영구적 실패 |
| Facility | 명확한 가공·State·Tag·Routing·물류 행동 하나 | 모든 행동을 수행하는 만능 기계 |
| Process Tag | 지역 조건부 Combo 선택 | 모든 가공에서 무한히 누적되는 영구 이득 |
| Fuel Imprint | 최종 족보 또는 출신지 전략 선택 | 연료 제작 전 반복 곱연산 |
| Technology | 기본 기능과 회복 수단 보장 | Run을 정의하는 무작위 특화 |
| Augment | 특화 Package, Retrofit, Doctrine 선택 | Family를 가공할 수 있는지 자체 |
| Hub와 Route | Line 구간 연결과 물류 처리량 제약 | 자원 이력의 암묵적 초기화 |

## 5. 자동화 Line 계층

대규모 Line은 하나의 끊임없는 지표면 경로가 아니라 계층으로 표현해야 한다.

~~~text
Surface Segment
    한 천체에서 수행하는 가공

Transit Segment
    Hub -> 우주선 Route -> Hub

Inter-Celestial Line
    카드 자원 하나를 위한 Surface/Transit Segment의 순서

Fuel Network
    항성 연료 제작기 하나로 수렴하는 카드 자원 Line 5개
~~~

이 계층은 세 가지 주요 Macro 배치를 지원한다.

### 5.1 지역 완결형 Line

채굴, 모든 가공, Imprint, 연료 조립을 하나의 천체에서 수행한다.

장점:

- 이동 지연이 적다.
- 동기화가 단순하다.
- Route Capacity 비용이 없다.
- 물류가 막혀도 복구하기 쉽다.

비용:

- 원격 자원과 환경 조건을 이용하기 어렵다.
- 가치 있는 지역 Cell과 지원 Facility를 두고 경쟁한다.
- 물류 전용 Tag와 Augment를 활용할 기회가 적다.

### 5.2 분산형 카드 생산

각 행성 또는 위성이 하나 이상의 자원 카드를 완성하고, 완성된 카드를 중앙 조립 천체로 보낸다.

장점:

- 각 천체가 자신의 광맥, 온도, Family에 특화된다.
- 높은 Current Energy를 운송하므로 화물 한 칸의 가치가 높다.
- 지역 Line을 이해하기 쉽다.

비용:

- 최종 조립이 여러 Route Schedule에 의존한다.
- 중앙 Buffer와 Card Key Filter가 필요하다.
- 한 Route가 멈추면 특정 족보 Slot이 고갈될 수 있다.

### 5.3 순차형 천체 간 가공

한 자원이 여러 천체를 순서대로 방문하며 각 천체에서 서로 다른 유용한 가공을 받는다.

장점:

- 극단적인 환경과 특수 Facility를 활용할 수 있다.
- 물류 Tag와 Macro Doctrine을 사용할 수 있다.
- 실제로 성계 전체를 잇는 생산 Line이 된다.

비용:

- 지연 시간이 길다.
- 선적과 하역이 반복된다.
- Route Capacity 사용량이 크다.
- State, Tag, Batch 추적이 복잡하다.

지역 완결형과 분산형은 서로 대등한 선택지여야 한다. 천체 경계를 넘는 것 자체가 조건 없는 무료 Energy 보너스가 되어서는 안 된다.

## 6. 대규모 물류에서의 자원 규칙

### 6.1 기본 운송은 중립

일반 운송은 자원 Instance 전체를 보존한다.

- Current Energy를 보존한다.
- Family State를 보존한다.
- Tag와 Primed/Spent 상태를 보존한다.
- Spectrum과 Rank를 보존한다.
- Family Processing History를 보존한다.
- 운송은 가공으로 세지 않는다.
- 운송은 부정 State를 해제하지 않는다.

따라서 한 행성의 Metal Hot 가공을 다른 위성의 Cold 가공으로 이어갈 수 있다. 동시에 운송 자체가 Fatigued나 Fractured를 무료로 초기화하는 것도 막는다.

명시적인 규칙만 운송 중 자원을 바꿀 수 있다. 예를 들어 Cryogenic Hold, Bio-Culture Hold, Grounding Hold, Void Sling Augment 등이 있다.

### 6.2 최소 물류 이력

선택적 물류 전략은 다음 정보를 확인할 수 있다.

- Origin Body
- Last Processed Body
- 마지막 가공 이후 천체 간 이동이 있었는지
- 현재 가공이 수입 후 첫 가공인지
- 활성 Pilgrim 규칙에서 이미 방문한 천체의 작은 집합

활성 규칙에 필요한 Metadata만 존재해야 한다. 전체 이동 이력은 필요하지 않다.

### 6.3 동질 Batch

Current Energy, State, Tag, 관련 이력이 다른 자원은 하나의 기계적 Stack으로 합칠 수 없다. 대규모 생산에서는 다음과 같이 동질 Batch를 유도한다.

- 하나의 Route는 하나의 생산 Profile을 운반한다.
- 하나의 Hub Lane은 하나의 Card Key 또는 가공 단계만 대상으로 한다.
- 하나의 Facility 묶음은 같은 Line을 거친 Batch를 가공한다.
- 서로 갈라진 자원은 공용 Storage에 들어가기 전에 분리한다.

## 7. 물류 Manifest

현재 Route는 Cargo를 Resource ID로 선택할 수 있다. 개편 후에는 같은 자원의 여러 가공 버전이 잘못된 Line에 들어가지 않도록 속성 기반 Manifest가 필요하다.

Route 또는 Hub Lane은 다음 조건으로 Filter할 수 있다.

- Resource ID
- Family
- Spectrum
- Rank
- Current Energy 최소·최대값
- 필요한 또는 금지된 Family State
- 필요한 Process Tag와 Primed/Spent 상태
- 필요한 Fuel Imprint
- Raw, Intermediate, Completed Card, Star Fuel 단계

권장 Manifest Preset:

- Raw Resource
- Family Intermediate
- Tagged Intermediate
- Completed Card
- Fuel Input Slot 1-5
- Star Fuel Missile Cargo

Manifest는 저장하고 재사용할 수 있어야 한다. 조건에 맞는 화물이 없으면 Route는 잘못된 화물을 싣지 않고 대기한다.

## 8. Tag 모델

### 8.1 Slot 제한

표준 자원은 다음 두 Slot을 가진다.

1. Process Tag Slot 하나
2. Fuel Imprint Slot 하나

이 제한은 모든 Line이 유리한 Tag를 전부 붙이는 상황을 막는다.

### 8.2 Process Tag 생명주기

일반적인 Process Tag는 다음 순서를 따른다.

~~~text
Applied -> Primed -> Triggered -> Spent -> 제거 또는 교체
~~~

- Process Tag는 Energy 합연산만 수행한다.
- 일반 가공에서 `C`를 바꾸지 않는다.
- Spent Tag는 Scrubber 또는 명시적인 교체가 제거할 때까지 보인다.
- 하나의 Tag를 적용·발동·제거·재적용하는 과정이 무료 지배 Loop가 되어서는 안 된다.
- Tag는 자신을 적용한 Facility에서 즉시 발동할 수 없다.

### 8.3 Fuel Imprint 생명주기

- Fuel Imprint는 일반 가공에 반복 적용되지 않는다.
- 항성 연료 제작기에서 한 번만 판정된다.
- 표준 Fuel Imprint는 `B`를 더한다.
- HighTech Catalyst Imprint는 `C`를 더할 수 있다.
- 한 연료 Batch에는 Catalyst Imprint 하나만 `C`에 기여한다.

### 8.4 핵심 Tag 예시

아래 수치는 임시값이다.

| Tag | Slot | Trigger | 효과 |
|---|---|---|---|
| Overtone | Process | 긍정 Family State 활성화 | Energy +5 후 Spent |
| Reclamation | Process | 부정 Family State 해제 | Energy +7 후 Spent |
| Crosslink | Process | 현재 Process Archetype이 직전과 다름 | Energy +4 후 Spent |
| Crimson Charge | Process | Red 자원이 다음 유효 가공을 완료 | Energy +3 후 Spent |
| Landing Charge | Process | 천체 간 수입 후 첫 가공 | Energy +5 후 Spent |
| Pilgrim Charge | Process | Origin Body가 아닌 천체에서 첫 가공 | Energy +6 후 Spent |
| Crimson Imprint | Fuel | 입력 자원이 Red | 최종 B +4 |
| Twin Seal | Fuel | 해당 자원이 같은 Rank 묶음에 참여 | 최종 B +6 |
| Sequence Seal | Fuel | 해당 자원이 Sequence 족보에 참여 | 최종 B +7 |
| Local Purity | Fuel | 모든 일반 가공을 Origin Body에서 끝낸 뒤 최종 수출 | 최종 B +6 |
| Convergence Seal | Fuel | 서로 다른 Origin의 카드 3장 이상이 각자의 Origin에서 마지막 일반 가공 완료 | Batch당 최종 B +12 |
| Foundry Seal | Fuel | 카드 5장이 모두 제작기 천체에서 마지막 일반 가공 완료 | Batch당 최종 B +12 |
| Pilgrim Seal | Fuel | 카드 5장이 모두 Origin 밖에서 유효 가공을 한 번 이상 완료 | Batch당 최종 B +12 |
| Prismatic Catalyst | Fuel Catalyst | Batch에 Spectrum 4종이 모두 존재 | 최종 C +1, Catalyst 하나만 적용 |

Local Purity와 Convergence Seal은 같은 Fuel Imprint Slot을 두고 경쟁한다. 지역 전략과 분산 전략의 보상을 한 자원에서 동시에 얻을 수 없다.

### 8.5 Tag 안전 규칙

다음 효과는 피한다.

- 가공할 때마다 조건 없이 영구 Energy를 주는 효과
- 상한 없이 전체 Process Count에 비례하는 효과
- 서로 다른 천체 또는 1회 제한 없이 전체 Route 횟수에 비례하는 효과
- 자신을 생산하거나 다시 Primed로 만드는 Tag
- 쉽게 사용할 수 있는 영구 Spectrum 또는 Rank 변경
- 숨겨진 확률 Trigger
- 모든 입력 자원에 붙일 수 있는 최종 `C` 보너스
- 한 Batch에서 여러 Topology Seal을 중첩하는 효과
- 생산기나 Trigger가 보장되지 않는 Tag

## 9. Facility 모델

### 9.1 Facility 계약

각 Facility는 다음 내용을 선언해야 한다.

- 하나의 주 행동
- Process Archetype
- 선택적 Family Action
- 기본 합연산 Energy 변화량
- 읽기 쉬운 조건부 보너스 하나
- 가공 시간
- Footprint
- 입력·출력 방식
- Tag 입력·출력·Trigger
- 온도 또는 환경 Context
- 운영 비용 또는 Operational Load

Facility 하나가 관련 없는 효과를 무질서하게 많이 가져서는 안 된다.

### 9.2 확정 제공 Foundation Facility

| 역할 | 예시 | 접근 방식 |
|---|---|---|
| 채굴 | Universal Extractor | 시작 시 제공 |
| 공용 Bridge A | Pulse Processor | 시작 시 제공 |
| 공용 Bridge B | Compression Mill | 시작 시 제공 |
| Tag 제거 | Tag Scrubber | Tag가 열릴 때 확정 제공 |
| 지표면 운송 | Conveyor, Router, Filter | 시작 또는 확정 Technology |
| 우주 연결 | Hub | 확정 진행 |
| 최종 조립 | 항성 연료 제작기 | 확정 진행 |
| 항성 전달 | Star Fuel Missile Launcher 또는 Hub 발사 Mode | 확정 진행 |

Pulse와 Compression은 어떤 Card도 통과시킬 수 있는 복구용 Archetype이다. 부정 Family State 압력은 진행하지만 긍정 Family Merit를 활성화하거나 소비하지 못한다. 첫 Family가 등장할 때 해당 Family Foundation Facility가 함께 확정 제공되므로 Bridge 반복이 정상 생산의 정답이 되지 않는다.

### 9.3 Family Foundation Facility

해당 Family를 처음 사용할 수 있게 될 때 필수 제어 Facility도 함께 확정 해금한다.

| Family | Foundation Facility | 목적 |
|---|---|---|
| Metal | Induction Forge, Cryo Press, Annealing Chamber | Hot -> Cold와 명시적인 Work Strain 회복 |
| Crystal | Resonance Mill, Facet Shifter | 짧은 반복과 Fractured 회복 |
| Organic | Growth Vat | 명시적인 Growth Cycle |
| Plasma | Arc Amplifier, Grounding Coil | 증폭 Burst와 Discharge |
| Void | Null Sink, Echo Chamber | 명시적인 Sacrifice와 Echo Gain |

이 Facility들은 무작위 Augment 보상이 아니다.

### 9.4 지표면 Facility 진행 단계

#### Core

- 작고 조건 없는 Energy 증가
- 단순한 작업
- 안정적인 처리량
- Tag 의존성 없음

#### Conditional

- 작고 안전한 기본 증가량
- State, Tag, 환경, Manifest 조건을 만족하면 큰 합연산 보너스
- 조건을 만족하지 않아도 자원을 거부하지 않음

#### Integrated

- 이미 알려진 작업 두 개를 결합
- Cell, 시간 또는 조작 부담 대비 효율 향상
- 낮은 처리 속도, 높은 Operational Load, 넓은 Footprint, 좁은 조건 중 하나를 대가로 지불

#### Capstone

- Build 전체를 정의하는 행동
- 까다로운 선행 조건
- 희귀한 최종 Catalyst 또는 Macro 물류 규칙 생성 가능
- 기본 연료 생산에 필수는 아님

### 9.5 물류 Facility와 Module

| Facility 또는 Module | 목적 |
|---|---|
| Manifest Router | Family, Spectrum, Rank, State, Tag, Energy 조건으로 Cargo 분배 |
| Export Buffer | 동질 Cargo Batch 생성 |
| Import Buffer | 도착 Batch를 지표면 Routing 전에 분리 |
| Arrival Processor | 수입 후 첫 가공 Tag 판정 |
| Card Warehouse | Card Key 또는 연료 입력 역할별 Inventory 예약 |
| Fuel Assembly Buffer | 잘못된 자원을 소비하지 않고 완성된 5장 Set 대기 |
| Bulk Hold | 화물 수를 늘리지만 이동 속도 감소 |
| Express Hold | 적은 화물을 더 빠르게 운반 |
| Cryogenic Hold | 승인된 규칙에서 명시적인 Cold 운송 작업 수행 |
| Bio-Culture Hold | 유효한 이동 중 Organic Growth Cycle 수행 |
| Grounding Hold | 명시적인 Plasma Discharge 수행 |

기본 Cargo Hold는 중립이다. 특수 Hold는 선택적 Facility, Route Module, Technology 또는 Augment 보상이다.

## 10. 행성 규모에서의 효율

효율은 여러 축으로 평가해야 한다.

| 지표 | 의미 |
|---|---|
| 자원당 Energy | 완성된 카드 하나의 가치 |
| 초당 Energy | 항성 연료 시간 제한에 대응하는 처리량 |
| 운영 입력당 Energy | 대형 공장의 지속 가능성 |
| 가치 있는 지역 Cell당 Energy | 광맥, 극단 온도, Hub, 좁은 육지 활용도 |
| Cargo Slot-Second당 Energy | 제한된 천체 간 물류로 이동하는 가치 |
| 족보 안정성 | 의도한 5장 조합이 만들어지는 빈도 |
| 회복 비용 | State 또는 Route 장애 복구에 필요한 시간과 시설 |

Advanced Facility는 한두 지표를 개선해야 하며 모든 지표를 동시에 개선해서는 안 된다.

Sidegrade 예시:

| Facility | 개별 Energy | 처리량 | Footprint | Operational Load | 조건 |
|---|---:|---:|---:|---:|---|
| Bulk Reactor | 높음 | 낮음 | 큼 | 중간 | 쉬움 |
| Pulse Array | 낮음 | 높음 | 작음 | 높음 | 쉬움 |
| Conditional Resonator | 매우 높음 | 중간 | 중간 | 중간 | 어려움 |
| Integrated Processor | 중간 | 중간 | 매우 작음 | 높음 | State 필요 |

### 10.1 직렬 반복을 제어하는 Refinement Resistance

Operational Capacity는 동시에 가동하는 설비 수를 제어하지만, Capacity 안에서 같은 자원 한 장을 무한 직렬 가공하는 선택 자체는 막지 않는다. 이를 위해 Resource V2의 Energy 변화 Family 가공은 다음 Cycle 배율을 공유한다.

~~~text
Refinement = max(0, Current Energy - Seed Energy)
Cycle Multiplier = 1 + Refinement / 40
~~~

결과 Energy에는 곱연산을 추가하지 않는다. 채굴·각인·회복·최종 합성은 제외하고, 같은 입력 Instance를 Runtime과 Preview가 함께 사용한다. 따라서 병렬 Line은 처리량 확장 수단으로 남고, 직렬 Line은 뒤쪽 설비일수록 긴 Cycle을 감수한다.

## 11. 확장형 운영 모델

지표면이 매우 넓고 가공 횟수 제한도 없기 때문에 Family 주기와 Footprint만으로는 무한히 긴 합연산 Line을 막을 수 없다. 주된 확장 압력으로 천체별 `Operational Capacity`와 하나의 자동화 지원 물자 `Industrial Supply`를 함께 사용하는 방식을 권장한다.

### 11.1 Operational Capacity

각 천체는 최소한의 지역 연료망을 운영할 수 있는 무료 기본 Operational Capacity를 가진다. Facility는 실제로 작업 중일 때만 `Operational Load`를 소비한다.

~~~text
사용 가능한 Operational Capacity
    = 천체 기본 Capacity
    + 보급 중인 Service Core의 Capacity
    + 명시적인 Augment 보정

현재 Operational Demand
    = 작업 중인 Facility의 Operational Load 합
~~~

Demand가 Capacity를 초과해도 작업을 거부하거나 공장 전체를 끄지 않는다. 해당 우선순위 Tier의 속도를 비례 감속한다.

~~~text
Speed Factor = min(1, 해당 Tier에 남은 Capacity / 해당 Tier의 Demand)
~~~

플레이어는 `Critical`, `Normal`, `Background` 우선순위를 지정할 수 있다. 높은 Tier부터 Capacity를 배정하고, 같은 Tier 안에서는 남은 Capacity를 비례 공유한다. 따라서 Hub, 항성 연료 제작기, Industrial Supply 복구 Line을 보호할 수 있다.

이 시스템을 Power나 Energy라고 부르지 않는다. `Energy`는 연료 카드 자원이 운반하는 값으로 남겨 UI와 수식의 혼동을 막는다.

### 11.2 Industrial Supply와 Service Core

`Service Core`는 지역 Buffer의 Industrial Supply를 소비하는 동안 해당 천체의 Operational Capacity를 늘린다. Industrial Supply는 일반 자동화 물자처럼 생산·운송·저장·분배할 수 있지만 연료 카드로 사용할 수 없다.

- 모든 천체는 무료 기본 Capacity를 가진다.
- 첫 Line에는 Service Core가 필요하지 않다.
- 예상 수요가 기본 Capacity를 넘기기 전에 Industrial Supply Recipe와 첫 Service Core를 Technology로 확정 제공한다.
- Service Core는 보이는 Reserve Buffer를 가진다.
- Buffer가 비면 추가 Capacity가 사라지고 낮은 우선순위 작업이 감속하지만 자원이나 Facility는 파괴되지 않는다.
- 모든 Service Core가 꺼져도 기본 Capacity만으로 Supply 생산기를 재가동할 수 있어야 한다.
- 첫 버전에서는 지원 물자 하나만 사용한다. 이 모델이 부족하다고 검증되기 전에는 Power, Coolant, Maintenance Part, Durability, Workforce를 동시에 추가하지 않는다.

이 구조는 건물 수 Hard Cap 없이 새로운 자동화 문제를 만든다. 거대한 중앙 공장은 큰 지원 Line이 필요하고, 여러 중형 행성 공장은 각 천체의 무료 Capacity 안에서 운영할 수 있다.

### 11.3 Fleet Capacity

천체 간 물류에는 별도의 `Fleet Capacity` Budget을 사용한다.

- Route는 Ship 또는 Hold 종류에 따라 Fleet Load를 예약한다.
- 기본 Hub는 소수의 일반 Route를 운영할 Capacity를 가진다.
- 보급 중인 Fleet Berth는 물류 중심 Hub의 Capacity를 확장한다.
- Fleet Demand가 너무 높으면 출발 순서를 Queue에 넣는다. Cargo를 삭제하거나 이유 없이 Route를 멈추지 않는다.
- Fleet Berth도 Industrial Supply를 사용할 수 있지만 첫 버전에 별도 Propellant 물자를 추가하지 않는다.

Operational Capacity는 공장 집중도를, Fleet Capacity는 천체 간 분산을 제어한다. 둘을 분리해야 하나의 만능 Budget이 모든 배치를 결정하지 않는다.

### 11.4 이 모델이 만들어야 하는 선택

| 범위 | 제약 | 주된 질문 |
|---|---|---|
| 자원 하나 | Family State 주기 | 이 카드를 어떤 순서로 가공할 것인가? |
| 천체 하나 | Operational Capacity와 Industrial Supply | 공장을 이곳에 얼마나 집중할 것인가? |
| Hub 물류망 | Fleet Capacity, Cargo Capacity, 이동 시간 | 어떤 가공 단계를 운송할 가치가 있는가? |
| Run 전체 | 항성 연료 Cycle 압력 | 제한 시간 안에 어떤 5개 Line을 개선할 것인가? |

Advanced Facility는 Operational Load당 Energy, 처리량, Footprint, Cargo 효율 같은 선택된 축을 개선해야 한다. 모든 지표에서 동시에 우월해서는 안 된다.

다음 요소는 도입하지 않는다.

- Hard Building Cap
- 작은 과부하로 인한 천체 전체 정지
- Facility별 수동 수리 또는 Durability 관리
- 여러 필수 지원 물자를 한꺼번에 추가
- 비용 없이 무한 확장되는 Capacity Generator
- Baseline Line부터 요구되는 Industrial Supply

구체적인 시험 수치와 Capacity 계산은 [AutomationLineReferenceRun.md](AutomationLineReferenceRun.md)에 있다.

## 12. Technology와 Augment의 분리

### 12.1 Technology

Technology는 기능을 보장한다.

- Core Facility 역할
- Family Foundation 및 회복 Facility
- Hub와 기본 Route 생성
- 기본 Manifest와 Filter
- Tag Scrubber
- 항성 연료 제작기
- 증가하는 수요에 필요한 최소 Route와 Storage Upgrade

### 12.2 Augment

Augment는 Run을 특화한다.

- Conditional Facility 해금
- Tag 적용 Recipe 해금
- 기존 Facility Retrofit
- 물류 Module 해금
- 한 효율 축 개선
- Macro Doctrine 생성
- 고위험 Capstone 제공

Tag를 사용할 방법 없이 추상적인 항목 하나로 해금하지 않는다. Augment Package는 적용 방법과 즉시 접근 가능한 Trigger 또는 Payoff를 함께 제공해야 한다.

### 12.3 설비별 Recipe 선택 규칙

Tag Imprinter와 Fuel Imprinter는 Recipe마다 별도 건물을 요구하지 않는다. 하나의 설치된 설비가 현재 해금된 Recipe 목록을 가지며, 플레이어는 설비 UI에서 사용할 Recipe를 선택한다.

- 선택값은 공유 Data Asset이 아니라 설치된 Facility Instance에 저장한다. 같은 종류의 설비라도 서로 다른 Recipe를 사용할 수 있다.
- Technology는 Tag Imprinter와 기본 `Crosslink` Recipe를 보장한다. 따라서 Augment가 없어도 Tag 기능을 시험하고 기본 Line을 만들 수 있다.
- Overtone, Reclamation, 물류 Tag와 모든 Fuel Imprint는 해당 Augment Package가 구체적으로 해금한다.
- 제작 중이거나 Processing Inventory에 자원이 들어간 설비는 Recipe를 바꿀 수 없다. 이미 투입한 자원의 결과가 UI 조작으로 변하는 일을 막기 위한 규칙이다.
- 선택된 Recipe가 잠겨 있으면 설비는 입력을 소비하지 않는다. Preview와 Process 상태에는 필요한 Package를 함께 표시한다.
- 새 Tag Imprinter의 작성 기본값이 잠겨 있으면 현재 사용 가능한 Technology Recipe로 자동 보정한다. 사용 가능한 Fuel Imprint가 하나도 없다면 Fuel Imprinter는 Package를 얻을 때까지 잠긴 상태를 명시적으로 보여 준다.
- Recipe 순환 UI에는 해금된 수와 현재 선택을 표시한다. Package를 얻은 뒤 기존 설비를 재건하지 않고 새 Recipe로 전환할 수 있어야 한다.

## 13. Augment Package 모델

각 Augment Package는 다음 Metadata를 가진다.

- Strategy ID
- 역할: Enabler, Engine, Payoff, Pivot, Capstone
- 호환 Family
- 호환 Spectrum 또는 족보
- 필요한 Facility와 Tag
- 제공하는 Facility, Recipe, Retrofit
- 즉시 사용 가능한지
- 상호 배타적인 Doctrine
- 예시 Line Preview

### 13.1 지역 Package

#### State Resonator

- Overtone을 해금한다.
- Tag Imprinter에 Overtone Recipe를 추가한다.
- 긍정 State를 활성화할 수 있는 모든 Family와 작동한다.
- Family 주기를 정확히 반복하는 Line을 보상한다.

#### Recovery Dividend

- Reclamation을 해금한다.
- 부정 Family State를 해제할 때 보상한다.
- 회복 작업은 더 느리거나 비싸진다.
- 부정 State를 삭제하는 대신 의도적으로 실패와 회복 구간을 만드는 전략이다.

#### Crimson Circuit

- Crimson Charge와 Crimson Imprint를 해금한다.
- 접근 가능한 Red 자원이 있을 때만 Offer에 등장한다.
- 일반 가공 Energy와 최종 Red 카드 가치를 함께 특화한다.

#### Integrated Quenching

- Integrated Quench Press를 해금한다.
- Cold Action, 새로운 Process Archetype, 합연산 가공을 결합한다.
- Metal Line의 지역 Footprint 효율을 높이는 대신 처리량을 낮춘다.

### 13.2 Macro 물류 Doctrine

후반 Run이 모든 보너스로 수렴하지 않도록 Macro Doctrine은 활성 Slot 한두 개로 제한한다.

#### Origin Foundries

- 채굴 천체에서 모든 가공을 끝내도록 유도한다.
- Local Purity를 사용할 수 있게 한다.
- 완성된 고 Energy 카드의 수출 Batch 효율을 높인다.
- 여러 지역 특화 공장이 중앙 조립을 공급하는 형태를 장려한다.

#### Central Convergence

- Raw Resource Cargo의 Stack Capacity를 높인다.
- 수입 후 첫 가공에서 Landing Charge를 사용할 수 있다.
- Foundry Seal로 카드 5개를 조립 천체에서 완성하는 것을 보상한다.
- 중앙 Card Warehouse와 Assembly Buffer를 개선한다.
- 여러 채굴 천체가 하나의 Mega Factory를 공급하는 형태를 장려한다.

#### Pilgrim Circuit

- Pilgrim Charge 또는 상한이 있는 Pilgrim 규칙을 해금한다.
- Pilgrim Charge는 Origin 밖의 첫 유효 가공을 한 번 보상한다.
- 상위 변형이 여러 천체를 보상한다면 눈에 보이는 방문 천체 상한을 가져야 한다.
- 방문한 천체를 Route Seal로 표시한다.
- 높은 지연과 Route Demand를 감수하는 순차형 Line을 장려한다.

#### Convergence Protocol

- Convergence Seal을 해금한다.
- 서로 다른 Origin의 카드 3장 이상이 각자의 Origin에서 마지막 일반 가공을 마치면 최종 B를 높인다.
- Local Purity와 같은 Fuel Imprint와 경쟁한다.
- 순차 순회가 아니라 분산 카드 생산을 보상한다.

### 13.3 물류 Engine Package

#### Deep-Space Tempering

- Cryogenic Hold를 해금한다.
- 승인된 Hot Metal Cargo가 도착 시 Cold Action을 완료할 수 있다.
- Hot 행성과 얼음 위성을 잇는 Metal Line을 가능하게 한다.
- Cargo Capacity 또는 이동 속도는 중립 Hold보다 낮다.

#### Bio-Ark Freight

- Bio-Culture Hold를 해금한다.
- 조건을 만족하는 긴 이동이 Organic Growth Cycle 하나를 완료한다.
- 위성과 행성에 분산된 Organic 생산을 장려한다.
- Bulk Hold보다 적은 Stack을 운반한다.

#### Grounded Transit

- Grounding Hold를 해금한다.
- 조건을 만족하는 Plasma Cargo가 도착 시 명시적인 Discharge를 완료한다.
- 한 천체의 Amplification Burst를 다른 천체에서 이어갈 수 있게 한다.
- 중립 Shuttle보다 적은 Stack을 운반한다.

### 13.4 운영 Economy Package

#### Distributed Grid

- Service Core가 하나 이하인 천체에 기본 Operational Capacity +6을 준다.
- 무한 Facility가 아니라 유한한 천체 수에 적용된다.
- 여러 중형 공장을 장려한다.
- Service Core를 많이 쌓은 Core World에는 이득이 없다.

#### Core World

- 천체 하나를 지정한다.
- 해당 천체의 Service Core가 더 많은 Capacity를 제공하지만 Industrial Supply도 더 소비한다.
- 중앙 처리량과 기반 시설 밀도를 개선한다.
- 중앙망을 확정한 뒤 자유롭게 옮길 수 없다.

#### Overclock Protocol

- 선택한 Facility Archetype 하나의 처리량을 높인다.
- Operational Load는 더 큰 비율로 증가한다.
- Capacity 여유는 있지만 시간이 부족할 때 유용하다.
- 가공당 무료 Energy를 주지 않는다.

#### Efficient Fabrication

- 선택한 Archetype의 Operational Load를 최소 1까지 낮춘다.
- Cycle Time 또는 Footprint가 조금 증가한다.
- 전면적인 상위 Tier가 아니라 지속 가능성을 개선한다.

#### Orbital Industry

- Fleet Berth Capacity 또는 Cargo Turnaround를 개선한다.
- 해당 Hub의 Industrial Supply 소비량을 높인다.
- 물류 비중이 큰 분산형 또는 순차형 망을 장려한다.
- 이동했다는 이유만으로 Energy를 주지 않는다.

## 14. Augment Offer 안전장치

Rarity보다 현재 Build Context를 먼저 검사한다.

3개 선택지는 다음 역할을 갖는다.

1. Immediate: 현재 Network에서 즉시 사용 가능
2. Synergy: 이미 고른 Strategy ID를 발전시킴
3. Pivot: 다른 실행 가능한 전략을 Enabler와 함께 시작

후반에는 Pivot을 호환되는 Capstone으로 바꿀 수 있다.

규칙:

- 최소 하나는 즉시 사용 가능해야 한다.
- 접근 불가능한 Family 또는 Spectrum 선택지를 제거한다.
- Tag Payoff는 생산기 없이 등장하지 않으며 필요하면 생산기를 Package에 포함한다.
- Route Augment는 사용할 수 있는 Hub 종점이 둘 이상 생긴 뒤 등장한다. 그 전이라면 누락된 기능을 함께 확정 제공해야 한다.
- 선택지 3개가 모두 같은 Family 또는 같은 Macro 배치만 대상으로 하지 않는다.
- 이미 선택했지만 완성되지 않은 Strategy의 후속 Package 가중치를 높인다.
- 사용 가능성과 의존성을 Filter한 뒤 Rarity를 판정한다.
- 제한된 Reroll 또는 `Skip -> Research` 선택지를 제공한다.
- 필수 Foundation Facility를 무작위 Pool에 넣지 않는다.
- Engine이 작동하기 전에 Capstone을 제시하지 않는다.

HighTech Pity만으로는 부족하다. Strategy 완성을 돕는 Pity도 필요하다.

## 15. 진행 단계

| 단계 | 플레이 경험 | 확정 제공 | 무작위 특화 |
|---|---|---|---|
| Foundation | 지역에서 기본 연료 생산 | Core Processor, Family 제어, 기본 Hub | 없음 또는 단순 Package 하나 |
| Specialization | 카드 Line 한두 개 개선 | Filter, Scrubber, 기본 Route Upgrade | Process Tag, Conditional Facility |
| Distribution | 행성과 위성에 생산 역할 배정 | Manifest, Card Buffer, Route UI | Local, Central, Transit Package |
| Integration | Footprint와 물류 비용 절감 | 개선된 공용 기반 시설 | Integrated Facility, Route Module |
| Composition | 목표 족보 안정화 | Fuel Assembly Buffer, 족보 Target | Fuel Imprint, 족보 Package |
| Capstone | Run의 정체성 확정 | 새로운 필수 기능 없음 | Doctrine, Catalyst |

진행이 개선하는 요소:

- 운영 비용당 Energy
- Route Capacity당 Energy
- 처리량
- State 제어
- Tag 통합
- 족보 안정성
- Network 관리 편의성

모든 구형 Facility를 수치상 완전히 우월한 신형 Facility로 교체하는 구조가 되어서는 안 된다.

## 16. Line 예시

모든 수치는 설명용이다. 완전한 수치 예시는 [AutomationLineReferenceRun.md](AutomationLineReferenceRun.md)에 있다.

### 16.1 초기 지역 Line

한 행성의 Metal 자원:

~~~text
Star Iron 광맥
-> Miner
-> Hot Induction Forge
-> Cold Cryo Press
   Tempered 활성화
-> Local Hub
-> 항성 연료 제작기
~~~

Tag나 Augment가 없어도 작동한다.

### 16.2 State Resonator Line

~~~text
Star Iron 광맥
-> Miner
-> Overtone Imprinter
-> Hot Induction Forge
-> Cold Cryo Press
   Tempered 활성화
   Overtone 발동 후 Spent
-> Hub
~~~

긍정 State를 안정적으로 활성화하는 Run이다.

### 16.3 Recovery Dividend Line

~~~text
Star Iron 광맥
-> Miner
-> Reclamation Imprinter
-> Press
-> Press
-> Press
   Fatigued 활성화
-> Annealing Chamber
   Fatigued 해제
   Work Strain 0으로 초기화
   Reclamation 발동 후 Spent
-> Hub
~~~

일시적인 Penalty를 의도적으로 받아들이고 회복에 투자한다.

### 16.4 분산형 카드 생산

~~~text
Hot Planet A:
    Metal 채굴 및 완전 가공
    -> Red 카드 수출

Icy Moon B:
    Crystal 채굴 및 Resonance 묶음
    -> Blue 카드 수출

Ocean Planet C:
    Organic Growth Vat와 수확
    -> Green 카드 수출

Storm Planet D:
    Plasma Amplification과 Grounding
    -> Yellow 카드 수출

Outer Moon E:
    Void Sacrifice와 Echo 가공
    -> 다섯 번째 카드 수출

Assembly Planet:
    Card Warehouse 5개
    -> 항성 연료 제작기
    -> Hub
    -> Star Fuel Missile
~~~

Origin Foundries 또는 Convergence Protocol이 이 배치를 강화한다.

### 16.5 중앙집중형 Line

~~~text
Mining Planet A의 Raw Metal -----\
Crystal Moon B의 Raw Crystal -----\
Organic Planet C의 Raw Organic ----> Central Factory Planet
Plasma Planet D의 Raw Plasma -----/      -> Family 가공 District
Void Moon E의 Raw Void ----------/       -> Fuel Imprinter
                                         -> 5장 조립
                                         -> Star Fuel 발사
~~~

Bulk Raw Route와 Landing Charge가 중앙 공장의 효율을 높인다. 대신 수입 물류와 공용 Bottleneck 부담이 크다.

### 16.6 순차형 다중 천체 Metal Line

~~~text
Ore Planet A:
    Star Iron 채굴
    -> Hot Induction Forge
    -> Export Hub

Cryogenic Route:
    Cryogenic Hold
    -> 도착 시 명시적인 Cold Action

Icy Moon B:
    Tempered 완성
    -> 다른 Process Archetype
    -> Pilgrim Charge 발동
    -> Export Hub

Assembly Planet C:
    Fuel Imprint
    -> Card Warehouse
    -> 항성 연료 제작기
~~~

Deep-Space Tempering과 Pilgrim Circuit이 이 배치를 강화한다. 카드 하나의 Energy는 높지만 지연과 Route 비용도 크다.

### 16.7 지역 Spectrum Line

~~~text
Red 자원
-> 짧고 처리량 높은 Family Line
-> Crimson Imprinter
-> Local Card Warehouse
-> Red 중심 연료 족보
~~~

Crimson Circuit은 최대 Line 길이보다 족보 안정성을 강화한다.

## 17. 매번 같은 Run으로 수렴하는 현상 방지

다음과 같은 독립 제약을 함께 사용한다.

- 자원당 Process Tag Slot 하나
- 자원당 Fuel Imprint Slot 하나
- Macro Doctrine Slot 한두 개
- Local 및 Distributed Fuel Imprint의 상호 경쟁
- 전면 상위호환이 아닌 Facility Sidegrade
- 서로 다른 자원·온도·Biome·Hub 배치
- Immediate, Synergy, Pivot 역할을 가진 Augment Offer
- Route Capacity와 운영 비용
- 목표 족보 차이
- 모든 Package를 얻기 전에 끝나는 제한된 Run 길이

Augment가 결국 전부 획득하는 유한한 Facility 해금 목록이 되어서는 안 된다. Blueprint 접근권은 늘어날 수 있지만 활성 Doctrine, Tag Slot, 환경 적합성, 운영 Budget이 최종 Network를 결정해야 한다.

## 18. 진행 불가능한 Run 방지

- Tag 없이도 기본 가공이 작동한다.
- 유효한 카드 5개는 항상 기본 Star Fuel을 만든다.
- Family Foundation Facility는 확정 제공한다.
- 모든 부정 State에는 확정적인 회복 방법이 있다.
- 일반 운송은 자원을 보존하며 무효화하지 않는다.
- 잘못 붙인 Tag는 Scrub할 수 있다.
- Fuel Assembly Buffer는 도착한 첫 5개를 무작정 먹지 않고 호환 Card Key를 예약한다.
- Manifest는 호환되지 않는 Cargo가 Route나 입력을 채우는 일을 막는다.
- 모든 Augment Offer에는 즉시 사용 가능한 선택지가 있다.
- 접근할 수 없는 Content는 무작위 Offer에서 제거한다.
- Route 전용 보너스는 선택 사항이다.
- Route가 막혀도 지역 Line은 계속 작동할 수 있다.

## 19. Network 관리와 성능 요구사항

물리적 규모가 크므로 Network 관리 도구는 편의 기능이 아니라 Gameplay 성립 조건이다.

필요한 관리 View:

- 천체별 생산 요약
- 이름을 붙인 자동화 Line
- Route 처리량과 예상 도착 시간
- Cargo Manifest와 정지 이유
- Batch의 Current Energy 범위
- 활성 State와 Tag 분포
- Card Key 생산 속도
- 연료 족보 준비 상태
- Source부터 최종 조립까지 Bottleneck 강조

필요한 조작 지원:

- Facility 묶음 Copy/Paste
- 재사용 가능한 Line Blueprint 또는 Template
- 저장 가능한 Hub Manifest
- Route 이름과 색상
- 연료 입력 Slot 5개를 위한 Batch 예약
- 멈춘 Line 또는 오염된 Batch 알림
- 전역 Line Graph에서 지역 Surface Segment로 이동

대규모 Simulation 규칙:

- Tag는 가공 완료, 수입, State 전이, 최종 제작 같은 불연속 Event에 반응한다.
- 모든 자원의 Tag를 매 Frame Tick하지 않는다.
- Organic Growth는 자원별 임의 실시간 Tick이 아니라 Facility 또는 Cargo Batch Event를 사용한다.
- 동질 Stack을 우선한다.
- 실제 활성 규칙만 선택적 물류 이력 Metadata를 할당한다.
- 관찰하지 않는 원격 Facility는 결과가 결정적으로 동일하다면 집계 Simulation을 사용할 수 있다.

## 20. 현재 권장 Baseline

명시적인 승인 전까지 다음을 권장한다.

1. Technology가 필수 기능을 보장한다.
2. Augment는 효율과 배치를 특화한다.
3. 자원은 Process Tag Slot 하나와 Fuel Imprint Slot 하나를 사용한다.
4. 일반 천체 간 운송은 State 중립이며 자원 Instance 전체를 보존한다.
5. 물류 효과는 명시적인 Facility, Hold, Route Module, Tag, Augment에서만 발생한다.
6. Route는 재사용 가능한 속성 기반 Manifest를 사용한다.
7. 지역 완결형, 분산 카드형, 중앙 공장형, 순차 다중 천체형을 모두 지원한다.
8. Macro 물류 Doctrine은 활성 Slot 한두 개를 사용한다.
9. Footprint는 지역 제약이며 주된 전역 확장 방지 수단이 아니다.
10. 천체별 Operational Capacity를 주된 공장 확장 압력으로 사용하고 보급 중인 Service Core로 늘린다.
11. Fuel Assembly Buffer는 의도한 5장 Set을 예약한다.
12. 대규모 성능을 위해 Tag와 State를 Event 기반으로 판정한다.
13. Hub별 Fleet Capacity를 별도의 물류 압력으로 사용하며 초과 출발은 Cargo를 무효화하지 않고 Queue에 넣는다.

## 21. Phase 7 구현 기준: Operational Economy

현재 Project에는 다음 Baseline이 `Resource V2` 전용 Runtime 규칙으로 반영되어 있다. 전역 Ruleset이 `Legacy`이면 기존 처리 속도와 자원 경로를 그대로 사용한다.

- `Common Ore`, `Biomass Feedstock`, `Industrial Supply`는 `Utility` 자원이다. Card 목록, Family State, Spectrum/Grade 족보, 항성 연료 입력에서 분리된다.
- Supply Fabricator는 `Common Ore 1 + Biomass Feedstock 1 -> Industrial Supply 2`를 30초에 수행하며 Operational Load 4와 기본 `Critical` 우선순위를 가진다.
- Service Core는 Input Slot 하나에 Industrial Supply 4개를 비축할 수 있고, 30초마다 1개를 소비한다. Buffer 또는 처리 예약 Inventory에 Supply가 남아 있고 Process가 켜져 있으면 해당 천체에 Capacity 18을 제공한다.
- 천체의 기본 Capacity는 30이다. 두 값은 `USRSimulationSettings`에서 조정할 수 있다.
- Operational Demand에는 `bProcessing` 상태이며 실제 진행 가능한 Facility의 Load만 포함한다. 배치만 되었거나 Idle인 설비, 출력이 막혀 완료 대기 중인 설비는 Capacity를 예약하지 않는다.
- Capacity는 `Critical -> Normal -> Background` 순서로 배정한다. 같은 Tier의 속도는 `min(1, 남은 Capacity / Tier Demand)`로 동일하게 감속한다.
- 초과 수요는 입력을 거부하거나 진행 중인 작업을 취소하지 않는다. Tick은 먼저 시작 가능한 작업을 모두 예약하고 한 번의 Demand Snapshot을 만든 뒤, 각 Facility의 Speed Factor만큼 진행 시간을 더한다.
- 배치된 Facility마다 우선순위를 따로 저장한다. Facility Control UI의 Priority 버튼으로 `Normal -> Background -> Critical`을 순환하며 Body Demand/Capacity, 활성 Core 수, 현재 속도를 함께 확인할 수 있다.
- Supply Fabricator를 `Critical`로 둘 수 있으므로 모든 Service Core가 정지해도 기본 Capacity 30 안에서 복구 Line을 다시 가동할 수 있다.
- 현 단계에서 Augment Capacity 보정은 0이다. Fleet Capacity는 Phase 8에서 별도 Hub 물류 Budget으로 구현했으며 Operational Capacity와 합치지 않는다.

## 22. Phase 8 구현 기준: Fleet Capacity와 Fleet Berth

현재 Project에는 다음 Baseline이 `Resource V2` 전용 Runtime 규칙으로 반영되어 있다. 전역 Ruleset이 `Legacy`이면 기존 Route의 적재량과 출발 동작을 그대로 사용하며 Fleet Queue를 만들지 않는다.

### 22.1 Route Profile

| Profile | 허용 Cargo | 최대 Stack | 출발 Fleet Load |
|---|---|---:|---:|
| Neutral Shuttle | 모든 Cargo. 구 Save와 복구 Line을 위한 호환 Profile | 8 | 2 |
| Card Courier | `Card` | 12 | 2 |
| Bulk Raw Hold | `Utility` 또는 State·Tag·Imprint·가공 이력이 없는 `Card` | 16 | 3 |
| Conditioned Hold | `Card` | 4 | 3 |

- Profile은 Route에 저장되며 Hub UI에서 빈 우주선이 정박 중일 때 현재 Run에서 해금된 항목만 순환 선택한다. Profile을 바꾸면 최대 적재량도 해당 Profile 기준값으로 초기화된다.
- 사용자가 더 작은 Batch를 원하면 최대 적재량을 `1`과 Profile 최대값 사이에서 설정할 수 있다. 어떤 설정도 Profile의 최대값을 넘지 못한다.
- Neutral Shuttle과 Card Courier는 Technology Profile이다. Bulk Raw Hold는 `CentralConvergence`, Conditioned Hold는 `DeepSpaceTempering`, `BioArkFreight`, `GroundedTransit` 중 하나로 해금된다.
- Neutral Shuttle은 `4.0 cargo/load`, Card Courier는 `6.0`, Bulk Raw Hold는 약 `5.3`, Conditioned Hold는 약 `1.3`이다. 따라서 범용 Shuttle은 복구용, Courier는 일반 Card 운송용, Bulk는 중앙집중형 원재료 수입용으로 역할이 겹치지 않는다.
- Conditioned Hold는 작은 Cargo와 높은 Fleet Load라는 선체 계약을 제공한다. 실제 Cold, Growth, Discharge는 Phase 9에서 구현된 별도 Route Module이 소유하며, Profile만 선택해서는 무료 가공이 생기지 않는다.

### 22.2 예약과 Queue

- Hub의 기본 Fleet Capacity는 8이며 `USRSimulationSettings`에서 조정할 수 있다.
- Route는 그 Hub에서 출발해 한쪽 구간을 항해하는 동안에만 출발 Hub의 Fleet Load를 예약한다. 도착해 Unload 단계로 들어가면 예약을 즉시 해제한다.
- 따라서 Capacity는 영구적인 Route 개수 제한이 아니라 동시 출발편과 물류 처리량 제한이다. 비활성 Route, Cargo 대기 Route, Unload 중인 Route, Debug Orbit은 예약하지 않는다.
- 출발하려는 Route가 들어갈 여유가 없으면 `WaitingForFleetCapacity`가 된다. Runtime은 Hub Export Buffer를 먼저 조회만 하며, Capacity와 Queue 우선권을 모두 통과한 뒤에만 Cargo를 실제로 꺼낸다.
- 대기 Route에는 증가하는 Queue Ticket을 부여한다. TArray 순서나 막 도착한 Route가 오래 기다린 Route를 추월할 수 없고, Ticket 순서가 Save/Load 후에도 유지된다.
- Queue 중 Cargo가 사라졌거나 Filter/Profile과 맞지 않게 되면 해당 Route는 Ticket을 반납하고 `WaitingForCargo`로 돌아간다. Cargo가 없는 Route가 Fleet Queue를 막지 않는다.
- 항성 연료 Missile은 최종 발사 수단이므로 일반 Cargo Fleet Capacity에서 제외한다.
- 대규모 Route를 위해 Queue 순번은 Hub별로 묶어 한 번 정렬하고, Capacity Report도 Hub당 Tick Snapshot을 재사용한다. 같은 Tick에 승인된 새 출발 Load만 Snapshot에 즉시 더해 과예약을 막는다.

### 22.3 Fleet Berth

- Fleet Berth는 Technology Facility이며 Input Slot 하나에 Industrial Supply 4개를 비축한다.
- 60초마다 Industrial Supply 1개를 소비하며, Buffer 또는 처리 예약 Inventory에 Supply가 남아 있고 Process가 켜져 있는 동안 Fleet Capacity +8을 제공한다.
- Fleet Berth의 Operational Load는 0이다. 물류 회복 설비 자체가 Operational Capacity 부족 때문에 멈추는 순환 교착을 만들지 않는다.
- 한 천체에 Hub가 여러 개면 각 Fleet Berth는 같은 Face의 격자 거리를 우선해 가장 가까운 Hub 하나에만 결정적으로 배정된다. 같은 거리에서는 Hub Occupant Id 순서를 사용하므로 Berth 하나가 여러 Hub에 중복 적용되지 않는다.

### 22.4 UI, 저장, 검증

- Hub UI는 `Reserved / Total Fleet Capacity`, Queue 수, 공급 중인 Berth 수, 각 Route의 Profile·Phase·Queue 순번을 표시한다.
- Fleet Capacity 필드는 물류 Save Schema Version 3에서 도입되었다. Route Module은 Version 4, 조건부 운송 체류 Snapshot은 Version 5에서 도입되었다. Version 1~2 Route는 Neutral Shuttle과 Queue Ticket 0으로, Version 3은 Module `None`으로, Version 4의 운송 중 Module Cargo는 결정적인 체류 Snapshot으로 이관된다.
- 자동화 검증은 Profile 수치와 Cargo 허용 범위, 기본/확장 Capacity, 예약 해제, Ticket 공정성, Legacy 우회, Fleet Berth의 Supply 생명주기, Save 직렬화를 각각 검사한다.
- PIE 기준 검증은 Fleet 예약과 Berth 확장 Smoke Check를 포함한다.

## 23. Phase 9 구현 기준: Conditioned Transit Route Module

Conditioned Transit은 `Resource V2`에서만 동작한다. 일반 Shuttle/Courier/Bulk 운송과 Module을 장착하지 않은 Conditioned Hold는 자원의 Energy, Family State, Process Tag, 가공 이력을 바꾸지 않고 출발·도착 천체와 Transit Count만 기록한다.

### 23.1 Module과 Augment

| Route Module | 해금 Augment | 허용 Family | 도착 시 명시적 가공 | 기본 합연산 |
|---|---|---|---|---:|
| Cryogenic Hold | Deep-Space Tempering | Metal | `CryogenicTransit`, Cold | +3 |
| Bio-Culture Hold | Bio-Ark Freight | Organic | `BioCultureTransit`, Growth | +0 |
| Grounding Hold | Grounded Transit | Plasma | `GroundingTransit`, Discharge | +1 |

- Hold는 Facility Content가 아니라 Route에 장착하는 Logistics Module이다. Augment Package는 공통 선체를 `Route Profile`, Family별 장비를 `Route Module` Grant로 각각 표시한다.
- Module은 Conditioned Hold Profile이며 우주선이 정박했고 Cargo가 비어 있을 때만 바꿀 수 있다. Hub UI의 `Next Hold` 버튼은 현재 Run에서 해금된 Module과 Neutral만 순환한다.
- Profile을 다른 선체로 바꾸면 Module은 자동으로 `None`이 된다. Conditioned Hold Profile만 고른 상태도 `None`이며 완전히 중립이다.
- Module이 장착되면 Hub Export Buffer의 `Card` 중 해당 Family만 적재한다. 다른 Family는 Buffer에 남으므로 잘못된 Cargo를 소비하거나 변환하지 않는다.

### 23.2 도착 Event 순서와 안전성

도착 한 구간의 판정 순서는 다음과 같다.

1. 입력 자원의 복사본에 출발·도착 천체와 Transit Count 1회를 기록한다.
2. Module이 없으면 그 복사본을 그대로 Unload한다.
3. Module이 있으면 Augment 해금과 Family 호환성을 다시 확인한다.
4. 공용 Resource Processing Kernel로 도착 천체에서 단 한 번의 Process Event를 평가한다.
5. 성공 결과만 Cargo에 반영하고 Unload 단계로 전환한다.

따라서 Preview와 Runtime은 같은 순수 계산 함수를 사용하며 입력 자원을 미리 변경하지 않는다. 도착 Tick에서 Route Phase가 즉시 `Traveling -> Unloading`으로 바뀌므로 Save/Load나 다음 Tick이 같은 구간을 재적용하지 않는다. 귀환편에 실제 Cargo가 있다면 반대쪽 천체 도착도 독립된 한 구간이므로 같은 규칙을 한 번 적용한다.

- Cryogenic Hold는 이전 가공이 Hot인 Metal에 Cold를 적용하므로 기본 +3과 Tempered +5를 함께 받을 수 있다. 이전 이력이 Hot이 아니면 기본 +3만 받는다.
- Bio-Culture Hold는 Matured를 켜고 Depleted를 해제하지만 자체 기본 Energy는 0이다. 이후 일반 Organic 가공이 Matured 보너스를 소비한다.
- Grounding Hold는 기본 +1과 함께 Energized/Overloaded를 해제한다.
- 모두 일반 Process Event이므로 State 활성/회복, Archetype 변경, 수입 후 Energy 변화에 반응하는 유효 Process Tag가 같은 표준 순서로 발동할 수 있다. 별도의 곱연산은 없다.
- 잠긴 Module, 손상된 Save의 잘못된 Profile 조합, 불일치 Cargo는 Transit 이력만 남기고 가공하지 않는다. Cargo 손실이나 임의 Family 변환은 없다.

### 23.3 UI, 저장, 검증

- Hub Route 행은 현재 Hold Module과 정적 계약을 표시한다. 운송 중 Cargo가 있으면 도착 후 예상 Energy와 총 합연산 Delta를 표시한다.
- 자원 상세와 Inventory Card Tooltip은 진단용 Process Count를 노출하지 않으면서 `Last Process`, 마지막 가공 천체, 마지막 도착 천체를 보여준다. 따라서 `CryogenicTransit`, `BioCultureTransit`, `GroundingTransit`이 이동에 숨은 보너스가 아니라 Line의 가공 단계처럼 보인다.
- Route Module 자체는 Save Schema Version 4에서 도입되었고 현재 물류 Save Schema는 조건부 체류를 포함한 Version 5다. Version 1~3 Route는 Module `None`으로 이관되며, Version 3의 Profile·Fleet Queue·Cargo는 그대로 보존한다.
- 자동화 검증은 Module Catalog와 Augment 소유권, 중립 운송 불변성, 세 Family Action, 잠김/불일치 안전성, Preview 순수성, Schema 4 직렬화를 검사한다.
- Phase 9 완료 당시 전체 `StarRovers.ResourceSystem` 자동화 Suite는 48개였고 PIE Baseline은 `Checks=22`, `Failures=0`, `SaveVersion=4`였다.

## 24. Phase 10 구현 기준: Refinement Resistance와 Metal Anneal

Phase 10은 `Resource V2` 일반 가공에만 적용되며 `Legacy` 처리 시간과 기존 자원 경로를 바꾸지 않는다.

### 24.1 공통 Cycle 규칙

~~~text
Refinement = max(0, Current Energy - Seed Energy)
Effective Cycle Seconds = Base Cycle Seconds * (1 + Refinement / 40)
~~~

- Facility Energy 변화량이 0이 아닌 `FamilyProcess`만 대상이다. 채굴, Tag/Fuel Imprint, Tag Scrub, Anneal, 운영 Economy, 최종 연료 합성은 기본 Cycle을 유지한다.
- Seed Energy는 연결된 Resource Data Asset에서 우선 읽고, Reference Content Instance는 ResourceId로 Catalog를 조회한다. Seed를 확정할 수 없는 손상·Custom Instance는 작업을 막지 않고 기본 Cycle로 안전하게 처리한다.
- 진행 중 Facility는 이미 예약한 `ProcessingInventory`의 첫 자원을 사용한다. Idle Preview는 다음 Input Slot 자원을 사용한다. 따라서 현재 Runtime Snapshot과 UI가 같은 시간을 계산한다.
- 별도의 Refinement Counter는 필요하지 않다. Resource Instance의 불변 Seed Snapshot과 Facility의 시작 시점 Cycle Snapshot을 저장하며, 지표면 Facility Runtime은 별도 Save DTO로 Input/Output/Processing Inventory와 진행 상태를 복원한다.
- Facility UI는 기본 Cycle, 저항 배율, 유효 Cycle, Seed보다 증가한 Energy를 표시한다. Resource V2 Process Preview도 같은 공통 계산 결과를 설명한다.

### 24.2 Metal Work Strain과 Annealing Chamber

- Tag/Imprint가 아닌 Metal Family Process가 끝날 때마다 `GeneralProcessesSinceReset`을 Work Strain으로 1 올린다.
- 세 번째 작업은 Forge/Press 등 Archetype이 달라도 즉시 Fatigued `-8`을 받고, Fatigued인 동안 Hot -> Cold가 새 Tempered를 만들지 못한다.
- Archetype 변경과 일반 운송은 Fatigued를 해제하지 않는다.
- `Annealing Chamber`는 Technology에 포함되는 Metal 전용 `Anneal` Action이다. 기본 Cycle 6초, Operational Load 2, Energy 변화 0이며 Tempered·Fatigued를 끄고 Work Strain을 0으로 만든다.
- Anneal은 Refinement Resistance를 받지 않는다. Reclamation처럼 “부정 State 해제”를 기다리는 한 번짜리 Process Tag는 Anneal에서 정상 발동할 수 있다.
- Prototype의 반복 가능한 안정 주기는 `Induction Forge -> Cryo Press -> Annealing Chamber`다. 가공 횟수 자체에는 제한이 없다.

### 24.3 검증 범위

- 순수 Formula와 Reference ResourceId Seed 조회
- 진행 중 예약 입력, Idle 입력, Runtime 복사본에서 동일한 Cycle
- Anneal·Tag·Legacy 제외 경로
- 세 번째 Metal 작업의 즉시 Fatigued, Archetype 변경 후 지속, Anneal 회복
- Catalog, Technology 해금, UI Preview, PIE Baseline Smoke Check
- 전체 `StarRovers.ResourceSystem` 자동화 Suite `51/51` 통과
- PIE Baseline `Checks=23`, `Failures=0`, `SaveVersion=4` 통과

## 25. 미결정 사항

1. Macro Doctrine Slot을 하나로 할지 둘로 할지
2. 구현 Baseline `30 / +18 / 1개당 30초`의 실제 Run Balance 재조정 여부
3. Origin Body를 항상 저장할지 활성 규칙이 있을 때만 저장할지
4. Last Process와 Last Arrival 외의 물류 이력을 전역 Line Graph에서 얼마나 노출할지
5. Void·Crystal용 추가 Route Module을 만들지, 세 Module만 유지할지
6. 우주선 한 대가 운반할 동질 Resource Stack 수
7. 항성 연료 제작기가 선택한 목표 족보만 따를지 Buffer에서 자동으로 최선의 족보를 고를지
8. 구현 Baseline `8 / +8 / Profile별 Load`와 이동 거리·속도를 함께 고려한 실제 Run Balance 재조정 여부
9. 임시 Spectrum 또는 Rank 변경이 천체 간 운송 후에도 유지될지
10. Process Tag, Fuel Imprint, Facility, Augment, Doctrine의 정확한 수치
11. 어떤 행성 환경을 전역 확정으로 제공하고 어떤 환경을 Run Random으로 둘지
12. 결과의 결정성을 유지하면서 원격 자동화를 어떻게 집계할지
13. Refinement Resistance Scale `40`과 Annealing Chamber의 `6초 / Load 2`가 실제 Run 처리량에서 적절한지

## 26. Phase 10 구현 후 발견된 후속 위험

### 26.1 최우선: 대규모 Facility Tick 공정성

현재 한 Facility Network는 Tick당 최대 64개만 처리한다. 처리 후보를 `TMap`의 처음부터 매번 다시 수집하고 Round-robin Cursor가 없으므로, 같은 천체에 65개 이상 등록되면 뒤쪽 Facility가 계속 선택되지 않을 수 있다. 여러 대형 Line을 한 천체에 두는 목표와 직접 충돌한다.

- 다음 구현에서는 안정적인 Facility Id 순서와 저장 가능한 Round-robin Cursor를 둔다.
- Capacity Snapshot은 그 Tick의 후보 집합 전체에 대해 한 번만 계산한다.
- 자동화 테스트는 65개, 128개 Facility가 여러 Tick 안에 모두 전진하는지 검사한다.

### 26.2 높음: Reference Content와 실제 건설 Content의 연결

현재 V2 Facility Catalog와 `Annealing Chamber`는 C++ Reference Preset 및 자동화 테스트에서 사용할 수 있다. 반면 실제 건설 UI는 `USRStructureDataAsset -> USRFacilityDataAsset`으로 연결된 authored `.uasset` 목록을 사용한다. 새 Reference Facility에 대응하는 Structure/Facility Asset이 아직 없으므로 플레이어가 PIE 건설 UI에서 직접 배치하는 End-to-end 경로는 완성되지 않았다. 현재 PIE Baseline의 `RegisteredFacilities=0`도 계약 Smoke Test이지 실제 Line 건설 검증은 아니다.

- V2 Facility별 authored Facility/Structure Asset과 Icon, Port, Footprint를 만든다.
- Augment 해금 결과가 건설 목록의 잠금 상태와 실제로 연결되는지 검증한다.
- PIE에서 채굴기부터 Annealing Chamber, 항성 연료 제작기까지 직접 배치한 Line을 검증한다.

### 26.3 높음: 지표면 Facility Save/Load

`USRFacilityNetworkComponent`의 Facility Map과 Processing 상태는 `Transient`이며 대응 Save DTO가 없다. 따라서 Input/Output Inventory, `ProcessingInventory`, 진행 시간, Recipe와 Priority를 포함한 진행 중 작업이 실제 Save/Load에서 복원되지 않는다. Refinement Resistance는 복원된 입력이 주어지면 결정적으로 다시 계산할 수 있지만, 그 입력과 진행 상태를 보존하는 상위 저장 계층이 먼저 필요하다.

### 26.4 높음: Conditioned Transit의 저항 우회 가능성

Cryogenic/Grounding/Bio-Culture Hold의 도착 가공은 이동 시간과 Fleet Load를 비용으로 사용하며 현재 Refinement Resistance를 받지 않는다. 특히 짧은 Route를 반복하는 Metal Line이 Cryogenic Hold의 `+3`과 Tempered 보너스를 시설 Cycle 증가 없이 반복해 후반 직렬 가공보다 지나치게 유리할 수 있다.

- Route 왕복 시간과 Fleet Load가 충분한 대체 비용인지 실제 처리량 Simulation으로 비교한다.
- 부족하면 Conditioned Arrival에 최소 Turnaround를 두거나 제한된 Resistance 시간 비용을 추가한다.

### 26.5 중간: Seed를 찾을 수 없는 동적 Resource

일반 채굴 자원은 Resource Data Asset을 가지고 Reference Card는 ResourceId Catalog로 Seed를 찾는다. 향후 Asset과 Catalog 양쪽에 없는 동적 합성 Resource를 허용하면 기본 Cycle로 처리되어 Resistance를 우회한다. 그런 Content를 도입하기 전에 Resource Instance에 불변 Seed Snapshot을 저장하거나 ResourceId 기반 Runtime Registry를 마련해야 한다.

### 26.6 중간: 처리 도중 Cycle 설정 변경

유효 Cycle은 Tick마다 다시 계산한다. 현재 `RefinementResistanceEnergyScaleV2`는 고정 Config라 문제가 없지만, 추후 Augment나 Runtime Event가 Scale을 바꾸면 진행 중 작업의 완료 시점도 즉시 바뀐다. Scale을 동적으로 바꿀 계획이라면 처리 시작 시 `ResolvedCycleSeconds`를 Snapshot으로 저장해야 한다.

## 27. Phase 11~16 후속 위험 해소 결과

26장에서 발견한 문제는 각각 독립적인 임시 예외를 추가하지 않고, 결정적 Snapshot과 단일 authored content manifest를 사용하는 방향으로 해소했다.

### 27.1 Phase 11: 대규모 Facility Scheduler

- Facility는 `OccupantId`의 안정적인 사전식 순서로 정렬한다.
- 한 Tick에 비용이 큰 전이를 최대 64개만 처리하되 저장 가능한 Round-robin Cursor에서 다음 Tick을 재개한다.
- Capacity Snapshot은 후보 전체를 기준으로 만들며, 선택되지 않은 활성 Facility도 자기 속도 계수만큼 Clock이 진행한다.
- 65개 이상 Facility가 있어도 뒤쪽 Facility가 영구적으로 굶지 않으며 TMap 내부 순서가 결과를 바꾸지 않는다.

### 27.2 Phase 12~13: Seed와 진행 시간 Snapshot

- Resource Schema Version 3은 생성 시점의 `SeedEnergySnapshot`과 유효 여부를 Instance에 저장한다.
- Data Asset, Reference Catalog, 저장 당시 Snapshot 순서로 Seed를 해결하므로 향후 Definition Balance 변경이 진행 중 자원의 Resistance 기준을 소급 변경하지 않는다.
- Facility는 작업 시작 시 `ResolvedProcessSeconds`를 한 번 확정한다. Config나 Augment가 이후 바뀌어도 진행 중 작업의 완료 시점은 흔들리지 않는다.
- 구 Schema는 현재 정의에서 Seed를 한 번 이관하고 이후 불변 Snapshot을 사용한다.

### 27.3 Phase 14: 지표면 Facility Save/Load

- Facility Network Save DTO는 Structure/Facility Soft Path, 회전, 온도, Process Enable, Priority, 선택 Recipe, 모든 Port Inventory, Processing Inventory, 진행 시간과 Cycle Snapshot을 보존한다.
- Import는 전체 Payload를 먼저 검증한 뒤 한 번에 교체한다. 손상된 일부 Facility 때문에 정상 Runtime을 반쯤 덮어쓰지 않는다.
- Round-robin Cursor도 함께 저장되어 Load 직후 Scheduler 공정성이 유지된다.

### 27.4 Phase 15: Conditioned Transit 체류 비용

조건부 Module은 이제 이동 도착 즉시 무료 가공되지 않는다.

~~~text
Conditioning Seconds = Module Base Seconds
                     * (1 + max(0, Current Energy - Seed Energy) / 40)
~~~

| Module | 기본 체류 |
|---|---:|
| Cryogenic Hold | 6초 |
| Bio-Culture Hold | 8초 |
| Grounding Hold | 4초 |

- Route는 `Travel -> Conditioning -> Apply Once -> Unload` 순서로 진행한다.
- 체류 시간은 구간 진입 시 Snapshot하며 Save Schema Version 5가 진행도와 확정 시간을 보존한다.
- Conditioning 동안 출발 Hub의 Fleet Capacity 예약은 유지된다. 따라서 짧은 Route 왕복도 Facility Resistance를 비용 없이 우회하지 못한다.
- Source/Destination Conditioning 방향은 enum 뒤에 추가하여 기존 직렬화 숫자를 보존한다.

### 27.5 Phase 16: 실제 authored Content와 건설 경로

최종 선택은 “Blueprint 목록 수동 편집”이나 “Transient Runtime Asset”이 아니라 다음 단일 흐름이다.

~~~text
C++ Reference Catalog
  -> SRGenerateResourceV2Content Commandlet
  -> 실제 Resource / Facility / Structure / Deposit Data Asset
  -> Resource V2 PlayerController Build Catalog
  -> Augment Recipe Gate + Facility Runtime
~~~

생성된 실제 Content:

| 종류 | 수량 | 경로 |
|---|---:|---|
| Resource | 8 | `/Game/StarRovers/Automation/V2/Resources` |
| Facility | 21 | `/Game/StarRovers/Automation/V2/Facilities` |
| 건설 Structure | 21 | `/Game/StarRovers/Structure/DataAssets/Artificial/ResourceV2` |
| 채굴 Deposit | 7 | `/Game/StarRovers/Structure/DataAssets/Natural/ResourceV2` |

- 일반 Process는 연결 가능한 Input 1개와 Output 1개를 가진 2x1 Footprint다.
- Supply Fabricator는 Input 2개, Stellar Fuel Fabricator는 경계에 배치된 Input 5개와 Output 1개를 가진다.
- Service Core와 Fleet Berth의 Supply Input도 실제 Port로 연결된다.
- Earth Terrain Profile에는 Card 5종과 Common Ore, Biomass Feedstock의 결정적인 V2 Spawn Rule이 들어간다.
- 자연 구조물 생성기는 활성 Ruleset과 Resource Definition Version을 비교한다. Legacy에서는 V2 Deposit을, Resource V2에서는 Legacy 광맥을 생성하지 않는다. 나무와 바위 같은 비광맥 자연 구조물은 영향을 받지 않는다.
- Resource V2 건설 목록은 Legacy Process/Synthesize Structure를 제거하고 V2 Structure 21종을 한 번씩 병합한다. Miner, Conveyor, Hub는 계속 사용한다.
- Facility는 Technology가 보장하고 Tag/Fuel Imprinter의 구체 Recipe와 Route Module은 Augment가 해금한다.
- 관련 Content 디렉터리는 Packaging Always Cook 대상으로 등록했다.

에셋을 C++ Catalog와 다시 동기화할 때는 Editor가 닫힌 상태에서 다음을 실행한다.

~~~powershell
UnrealEditor-Cmd.exe StarRovers.uproject -run=SRGenerateResourceV2Content -unattended -nop4 -nosplash -nullrhi
~~~

명령은 기존 Asset을 갱신하고 누락된 Asset만 생성하는 반복 실행 가능한 authoring 작업이다.

### 27.6 활성 Baseline과 최종 검증

- Project와 C++ 기본 Ruleset을 `ResourceV2`로 전환했다. `Legacy`는 Save 이관과 회귀 검증용으로 남는다.
- 실제 authored Helios Iron Instance를 `Induction Forge -> Cryo Press`로 처리하고, authored Card 5장을 authored Stellar Fuel Fabricator에 넣어 Full House 연료를 만드는 Vertical Slice를 자동 검증한다.
- 전체 `StarRovers.ResourceSystem` 자동화 Suite는 `61/61` 통과한다.
- Family Build Dock, Operations, Facility Inspector, Hub, Augment, Guidance와 통합 HUD 계약을 포함한 `StarRovers.UI` Suite는 `28/28` 통과한다.
- SolarSystem PIE Baseline은 `Checks=26`, `Failures=0`, `Ruleset=ResourceV2`, `SaveVersion=5`다.
- 같은 PIE에서 `AuthoredV2=8R/21F/21S/7D`, `BuildableV2=21`, `RuntimeDepositsV2=56/7Types`를 확인한다. `RegisteredFacilities=0`은 빈 시작 Map에 플레이어가 아직 설비를 배치하지 않았다는 뜻이며, 건설 Content 부재를 뜻하지 않는다.

화면별 정보 구조, 레이어와 입력 우선순위, 반응형 정책, 수동 PIE 체크리스트는 [UIOverhaulImplementation.md](UIOverhaulImplementation.md)에 정리되어 있다.

### 27.7 남은 것은 기능 결함이 아니라 Content/Balance 작업

- 현재 21개 Structure는 기능 검증을 위해 기존 설비 Mesh 변형을 재사용한다. 고유 Icon과 최종 Family별 외형은 Art Pass에서 교체한다.
- Prototype은 어느 행성에서도 기본 Line을 만들 수 없게 되는 Soft-lock을 막기 위해 Earth Profile 행성마다 V2 Deposit 7종을 제공한다. 실제 Run에서는 행성 Data/Seed가 최소 접근성을 보장한 뒤 Family별 분포를 천체별로 갈라야 한다.
- `30 / +18`, Fleet `8 / +8`, Deposit 8개씩, 체류 `6/8/4초`, Resistance Scale 40은 실제 다중 행성 처리량 Simulation으로 재조정할 수치다.
- 자동화 테스트는 authored Asset과 실제 Runtime Executor를 연결하고 PIE 건설 목록까지 검사한다. 최종 UX 승인은 사용자가 PIE에서 배치·회전·Conveyor 연결·삭제를 직접 수행하는 조작 QA로 남는다.

## 28. Phase 19: 항성 연료 5장 Batch 안전성과 가시성

최종 합성 지점의 입력은 `Empty → Collecting → Ready → Reserved` 상태로 분석한다. 정상 Card 1~4장은 더 이상 Recipe 오류가 아니며, Inspector가 유효 Card 수, 빈 Lane, 현재 족보, 중복 Card Key를 보여 준다. 다섯 장이 완성되면 기존 Fabricator 계산기로 B, C와 최종 Energy를 소비 전에 Preview한다.

Utility, 구 Schema, 잘못된 Energy, 알 수 없는 Fuel Imprint는 직접 투입과 Conveyor 수신 경계에서 Inventory 변경 전에 거부한다. 이전 Save에 이미 들어간 오염은 삭제하지 않고 `BATCH CONTAMINATED`와 정확한 Lane 번호로 알린다.

다섯 입력의 예약은 Port 복사본에서 먼저 검증한 뒤 한 번에 Commit한다. 불완전한 입력은 한 장도 소비하지 않으며, 이미 예약된 Processing Inventory를 두 번째 시작이 덮어쓰지 못한다.

구체 계약과 PIE 체크리스트는 [StellarFuelBatchSafetyImplementation.md](StellarFuelBatchSafetyImplementation.md)에 정리되어 있다.

## 29. Phase 20: 유한 광맥 경제와 Reserve 압력

실제 V2 Deposit의 50,000 강제값을 제거했다. Card 광맥은 120개, Common Ore와 Biomass는 180개이며 Industrial Supply는 자연 광맥을 만들지 않는다. C++ Catalog, 생성 Commandlet, authored Asset 검증이 같은 값을 사용한다.

Balance Harness는 공급 Stage를 정확히 N Batch 뒤 고갈시킬 수 있다. 실제 기본 Full House 824/10초와 최적화 1180/10초를 사용한 결과, 기본 광맥 세트 하나는 26:47에 패배한다. 원격 최적화 Line은 23:00에 생산을 시작하고 120초 운송 뒤 25:00에 도착하면 25:39에 승리한다.

행성 Overview와 Focus Operations는 잔량 비율, 활성/전체 광맥, Card와 원료 잔량을 표시한다. 전역 Telemetry는 만들 수 있는 완전한 5장 Batch 수와 제한 Card ID를 기록하고 자원 고갈을 별도 병목으로 진단한다.

구체 수치, 기각된 96개 안, Simulation과 다음 Save 경계는 [FiniteResourceEconomyBalanceImplementation.md](FiniteResourceEconomyBalanceImplementation.md)에 정리되어 있다.

## 30. Phase 21: Run Save와 유한 광맥 Migration

최상위 `Resource V2 Run Save`는 천체별 구조물·광맥과 Facility Network, 행성 간 물류, Augment 진행, 항성 압력, Cycle 시계, Milestone을 하나의 버전된 Checkpoint로 묶는다. Restore 전에 현재 Run을 백업하며, 하위 System 하나라도 실패하면 전체 상태를 Rollback한다.

구 Structure Save의 50,000 광맥은 채굴 비율을 보존해 현재 120/180으로 이관한다. 암묵적 무한 Resource V2 광맥은 유한화하지만 진짜 Legacy 광맥은 명시적인 무한 상태를 유지한다. 현재 Schema의 비상 광맥처럼 authored 기본량과 다른 유한 총량은 정확히 보존한다.

Telemetry는 저장하지 않고 복원된 권위 상태에서 다시 계산한다. 실제 SolarSystem PIE에서는 저장 뒤 광맥 잔량과 배속을 변조한 다음 Restore하여 구조물, 광맥, 시계와 Reserve Snapshot이 돌아오는지 확인한다.

구체 Schema, Load 순서와 현재 고정-Topology 경계는 [ResourceV2RunSaveMigrationImplementation.md](ResourceV2RunSaveMigrationImplementation.md)에 정리되어 있다.

## 31. Phase 22: 다중 Seed Soak와 결정론적 재생

실제 `BP_SolarSystemGenerator`와 여섯 Planet DA를 사용한 Root Seed 1~512 Soak가 모두 통과했다. 모든 System은 5~7개 행성, 4~6개 고유 환경, 필수 Resource 7종과 보장 완전 Card Front 4~10개를 가진다. 환경별 Resource는 정확히 세 종류이며 모든 Spawn Envelope가 유한 최소·최대치를 가진다.

하위 천체 Randomize Seed는 이제 비결정적 전역 난수가 아니라 Root `FRandomStream`에서 파생된다. 실제 D3D12 PIE에서 Seed A → B → A를 재생성한 결과 A의 행성, 천체 Seed와 Resource Cube-Sphere Cell Signature가 완전히 같고 B는 달랐다.

기본 Front 하나는 25:20에 고갈되고 26:47에 패배한다. 원격 최적화 Front는 23:00 선행 출발, 120초 운송, 25:00 도착으로 25:39에 승리한다. 이 결과를 바탕으로 Card 120 / Utility 180을 현재 최종 기준선으로 유지했다.

구체 분포, CSV Commandlet, 검증 경계는 [ResourceEconomySeedSoakImplementation.md](ResourceEconomySeedSoakImplementation.md)에 정리되어 있다.
