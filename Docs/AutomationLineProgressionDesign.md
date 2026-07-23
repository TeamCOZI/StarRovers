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
| 핵심 가공 A | Pulse Processor | 시작 시 제공 |
| 핵심 가공 B | Compression Mill | 시작 시 제공 |
| Tag 제거 | Tag Scrubber | Tag가 열릴 때 확정 제공 |
| 지표면 운송 | Conveyor, Router, Filter | 시작 또는 확정 Technology |
| 우주 연결 | Hub | 확정 진행 |
| 최종 조립 | 항성 연료 제작기 | 확정 진행 |
| 항성 전달 | Star Fuel Missile Launcher 또는 Hub 발사 Mode | 확정 진행 |

Metal 또는 Crystal이 등장하기 전 최소 두 개의 Process Archetype을 확보해야 한다.

### 9.3 Family Foundation Facility

해당 Family를 처음 사용할 수 있게 될 때 필수 제어 Facility도 함께 확정 해금한다.

| Family | Foundation Facility | 목적 |
|---|---|---|
| Metal | Induction Forge, Cryo Press | Hot -> Cold와 Archetype 변화 |
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
-> Hot Pulse Processor
-> Cold Compression Mill
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
-> Hot Pulse Processor
-> Cold Compression Mill
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
-> Cutter
   Fatigued 해제
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

## 21. 미결정 사항

1. Macro Doctrine Slot을 하나로 할지 둘로 할지
2. Prototype 이후 기본 Operational Capacity, Service Core 증가량, Industrial Supply 비율
3. Origin Body를 항상 저장할지 활성 규칙이 있을 때만 저장할지
4. 어떤 물류 이력 필드를 플레이어 UI에 보여줄지
5. Route Module을 Hub Facility, 우주선 설정, Technology 중 어디에 둘지 또는 혼합할지
6. 우주선 한 대가 운반할 동질 Resource Stack 수
7. 항성 연료 제작기가 선택한 목표 족보만 따를지 Buffer에서 자동으로 최선의 족보를 고를지
8. 정확한 Fleet Capacity, Route Load, 이동 속도, 거리, Cargo Capacity Scaling
9. 임시 Spectrum 또는 Rank 변경이 천체 간 운송 후에도 유지될지
10. Process Tag, Fuel Imprint, Facility, Augment, Doctrine의 정확한 수치
11. 어떤 행성 환경을 전역 확정으로 제공하고 어떤 환경을 Run Random으로 둘지
12. 결과의 결정성을 유지하면서 원격 자동화를 어떻게 집계할지
