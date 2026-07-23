# 자원 시스템 설계

## 1. 문서 범위

이 문서는 Star Rovers 자원 시스템의 현재 설계 방향을 기록한다.

- 구현 명세가 아닌 기획 문서다.
- 이 문서에서는 자원 규칙을 다룬다.
- 확정된 방향과 권고 초안을 구분하여, 아직 승인하지 않은 제안이 확정 규칙처럼 보이지 않게 한다.
- Tag, Facility, Augment, 진행 구조와 천체 간 Line 규칙은 [AutomationLineProgressionDesign.md](AutomationLineProgressionDesign.md)에서 이어진다.
- 실제 수치를 적용한 5장 자원 예시와 세 가지 완성형 자동화망은 [AutomationLineReferenceRun.md](AutomationLineReferenceRun.md)에 정리되어 있다.

## 2. 현재 확정된 방향

현재까지 확정된 내용은 다음과 같다.

1. 연료용 자원은 `Family`, 활성 `Family State`, Energy, `Spectrum`, `Grade` 또는 이에 해당하는 Rank 값을 가진다.
2. Process Count, Energy Change Count 같은 내부 이력은 존재할 수 있지만 자원의 대표 수치로 노출하지 않는다.
3. 각 Family는 고유한 긍정·부정 Family State를 가진다.
4. Family 규칙은 한 종류의 유리한 Facility를 무작정 반복하는 대신 자동화 Line의 형태를 바꾸게 해야 한다.
5. 항성 연료를 만들기 전까지의 모든 일반 가공은 Energy 합연산만 사용한다.
6. 곱연산은 마지막 항성 연료 제작기에서 정확히 한 번만 발생한다.
7. 자원에는 가공 횟수 제한이 없다. 여러 Facility를 거쳤다는 이유만으로 가공이 거부되지 않는다.
8. 항성 연료 제작기는 자원 5개를 소비하고 Spectrum과 Grade 조합을 Poker와 유사한 족보로 판정한다.

## 3. 자원 속성의 역할

처음 생성될 때의 Energy와 가공된 개별 자원이 현재 보유한 Energy는 서로 다른 개념으로 다루는 편이 명확하다.

| 계층 | 속성 | 역할 |
|---|---|---|
| 정의 데이터 | Name | 자원 종의 정체성 |
| 정의 데이터 | Family | 고유 가공 규칙과 사용 가능한 Family State를 결정 |
| 정의 데이터 | Seed Energy | 새로 채굴되거나 생성된 자원의 시작 Energy |
| 정의 데이터 | Native Spectrum | 항성 연료 제작기에서 문양처럼 사용하는 값 |
| 정의 데이터 | Native Rank | 항성 연료 제작기에서 숫자처럼 사용하는 값. 현재 작업 명칭은 `Grade` |
| Runtime | Current Energy | 해당 자원 Instance가 현재 보유한 Energy. Seed Energy에서 시작하여 가공으로 변함 |
| Runtime | Active Family States | 자원에 독립적으로 켜지고 꺼지는 현재 상태 |
| Runtime | Tags | Run 또는 자동화 Line에서 획득한 선택적 효과 |
| 내부 이력 | Processing History | 직전 온도, 직전 Process Archetype, 연속 행동 횟수처럼 다음 Family State를 판정하는 데 필요한 최소 정보 |

가공 과정에서 값이 바뀌기 때문에 `Base Energy`라는 이름은 모호하다. 다음 용어를 권장한다.

- `Seed Energy`: 자원 정의에 들어 있는 불변 시작값
- `Current Energy`: 자원 Instance가 운반하는 가변 현재값

플레이어용 자원 카드에서는 Current Energy를 강조하고, Seed Energy는 상세 정보에서만 보여줄 수 있다.

## 4. Energy 계산

### 4.1 항성 연료 제작 전 일반 가공

일반 가공에는 Catalyst나 `C` 항이 없다.

```text
Energy 변화량 = Facility 변화량
              + Family State 변화량
              + Process Tag 변화량

출력 Current Energy = 입력 Current Energy + Energy 변화량
```

항성 연료 제작 전에 존재하는 일반 다중 입력 합성도 기본적으로 합연산만 사용한다.

```text
출력 Current Energy = 모든 입력 Current Energy의 합
                    + Facility 변화량
                    + Family State 변화량
                    + Process Tag 변화량
```

Current Energy가 음수가 될 수 있는지는 아직 미정이다. 별도의 의미 있는 음수 Energy 규칙을 도입하지 않는 한 0에서 Clamp하는 방식을 권장한다.

### 4.2 최종 항성 연료 제작

항성 연료 제작기는 곱연산이 발생하는 유일한 장소다.

```text
B = 입력 자원 5개의 Current Energy 합
  + 족보 B 보너스
  + 최종 합성 Tag B 보너스

C = 1
  + 족보 C 보너스
  + 최종 합성 Tag C 보너스

항성 연료 Energy = 연료 기본값 A + B * C
```

향후 특정 항성 연료 Recipe가 별도의 기본값을 제공하지 않는 한 `연료 기본값 A`는 0으로 둔다.

예시:

```text
카드: R2, B2, R4, G4, Y4
족보: Full House
족보 보너스: B +30, C +3

입력 Current Energy: 30, 40, 55, 25, 70
입력 Energy 합: 220

B = 220 + 30 = 250
C = 1 + 3 = 4
항성 연료 Energy = 0 + 250 * 4 = 1,000
```

자동화 Line에서 이미 Current Energy를 늘린 Tag의 효과는 입력 Energy 합에 포함되어 있다. 같은 Tag가 최종 `B` 또는 `C`도 바꾼다면 별도의 최종 합성 효과를 명시해야 한다. 일반 가공 효과가 최종 합성에서 암묵적으로 한 번 더 적용되어서는 안 된다.

## 5. 가공 횟수 제한이 없는 구조

Remaining Process Limit은 없으며, 가공 이력 때문에 자원이 더 이상 가공되지 않는 일도 없다.

- Process Count와 Energy Change Count는 내부 이력 또는 진단 값으로 남길 수 있다.
- 이 값들은 가공 가능 여부를 막는 Hard Gate로 사용하지 않는다.
- 부정 Family State는 효율을 낮추거나 합연산 손실을 만들지만 자원을 영구적으로 망가뜨리지 않는다.
- 모든 부정 State에는 명확한 회복 행동이 있어야 한다.
- 잘못 가공된 자원도 교정 Line으로 보내거나 낮은 결과를 감수하고 항성 연료 제작기에 투입할 수 있어야 한다.

Hard Limit 대신 다음 압력이 Line 길이를 제어한다.

- 행성 전체 면적이 아니라 가치 있는 광맥, 온도대, Biome, Hub, Route 종점에 대한 제한된 접근성
- Facility 건설비와 가동 기회비용
- 가공 시간, 처리량, Bottleneck
- 확장 가능한 Facility 운영 비용 또는 천체별 Operational Capacity
- Hub 처리량, 화물 용량, 천체 간 이동 시간
- 각 Family의 긍정·부정 State 주기
- 서로 호환되는 자원 Line 5개를 동시에 운영해야 한다는 요구
- 항성 연료 Cycle의 시간 제한

512 x 512 크기 Face 6개로 이루어진 Cube Sphere에는 설치 불가 지형을 제외하기 전 1,572,864개의 Cell이 있다. 절반 정도를 사용할 수 없더라도 전체 면적만으로 Line 길이를 제어하기에는 너무 넓다. 희귀 광맥, 온도대, 해안선, Hub 주변의 국소 면적은 여전히 중요하지만, 별도의 확장형 운영 압력도 필요하다.

어떤 Family도 하나의 Facility 묶음이 동시에 자기 상태를 자동 초기화하고, 영구적으로 긍정 상태를 유지하며, 의미 있는 운영·시간·처리량·운송 비용까지 없는 구조가 되어서는 안 된다. 그렇지 않으면 넓은 지표면 전체에 같은 Module을 반복하는 것이 항상 최적해가 된다.

## 6. Family와 Spectrum

### 6.1 분리를 권장하는 이유

다음과 같이 역할을 분리하는 방식을 권장한다.

- `Family`: 자원을 어떤 방식으로 가공해야 하는가
- `Spectrum`: 최종 단계에서 어떤 자원과 조합해야 하는가
- `Rank`: 어떤 숫자 족보에 기여하는가

이 구조에서는 같은 Metal Family 안에도 서로 다른 Spectrum의 자원이 존재할 수 있다. 따라서 최종 족보 목표와 자동화 Line의 가공 문법이 서로 독립된 두 계획 축이 된다.

Family 구성원 모두가 언제나 같은 Spectrum을 가진다면 Spectrum은 독립 속성이 아니라 파생 데이터다. 이 방식을 택한다면 Spectrum 저장 필드를 제거하고 Family가 족보의 문양 역할까지 직접 맡는 편이 낫다.

두 필드를 모두 유지하면서 영구적인 1:1 대응을 강제하는 방식은 권장하지 않는다. 같은 정보를 중복 저장할 뿐 아니라, 같은 Spectrum 족보를 만들 때 한 Family만 반복하게 되어 하나의 연료 Batch에 합류하는 자동화 Line의 다양성이 줄어든다.

## 7. Family State 초안

다음은 1차 권고안이다. 정확한 Energy 수치는 아직 확정하지 않는다.

### 7.1 Metal

Line 정체성:

> Hot과 Cold 가공을 번갈아 사용하되, 피로를 피할 수 있도록 Process Archetype도 충분히 바꾼다.

#### Tempered — 긍정

- 직전 유효 가공이 Hot이고 현재 가공이 Cold일 때 활성화된다.
- 이 순서를 완성한 Cold 가공에 Energy 합연산 보너스를 준다.
- 다시 보너스를 받으려면 새로운 Hot -> Cold 순서를 만들어야 한다.
- 일반 가공 또는 Cold 반복은 준비된 순서를 끊는다.

#### Fatigued — 부정

- 같은 Process Archetype을 세 번째 연속 사용할 때 활성화된다.
- 세 번째 가공부터 합연산 Penalty를 적용한다.
- 다른 Process Archetype으로 가공하면 해제된다.
- `Process Archetype`은 Forge, Press, Separator 같은 Gameplay 분류를 뜻한다. 배치된 Actor Instance나 Basic/Advanced Tier를 뜻하지 않는다.

Tempered와 Fatigued는 조건이 맞으면 동시에 활성화될 수 있다. 올바른 온도 순서를 지켰다고 같은 가공 방식의 반복까지 면제되는 것은 아니다.

### 7.2 Crystal

Line 정체성:

> 같은 Process Archetype을 짧고 의도적인 묶음으로 반복한 뒤, 결정이 깨지기 전에 다른 방식으로 전환한다.

#### Resonant — 긍정

- 같은 Process Archetype을 두 번째 연속 사용할 때 활성화된다.
- 두 번째와 세 번째 반복에 Energy 합연산 보너스를 준다.
- 동일 반복이 계속되는 동안 활성 상태를 유지한다.
- Process Archetype이 바뀌면 해제된다.

#### Fractured — 부정

- 같은 Process Archetype을 네 번째 연속 사용할 때 활성화된다.
- Penalty가 Resonant 보너스보다 커서 반복을 계속할수록 순손실이 발생한다.
- Resonant와 동시에 활성화될 수 있다.
- Process Archetype을 바꾸거나 향후 전용 복원 작업을 사용하면 해제된다.

예시 주기:

```text
A -> A -> A -> B -> B -> B -> C
```

### 7.3 Organic

Line 정체성:

> 계속 가공하지 않고, 명시적인 성장 구간과 생산 가공을 번갈아 배치한다.

#### Matured — 긍정

- 성장 가능한 Buffer 또는 배양 Facility에서 명시적인 Growth Cycle을 한 번 완료하면 활성화된다.
- 다음 일반 가공에 Energy 합연산 보너스를 준다.
- 그 가공에서 소비된다.

#### Depleted — 부정

- 새로운 Growth Cycle 없이 일반 가공을 두 번 수행하면 활성화된다.
- 합연산 Energy 결과를 줄이며 필요하다면 처리량도 낮출 수 있다.
- Growth Cycle을 완료하면 해제된다.

Growth Cycle은 적절한 Facility가 소유한 Simulation Event다. 실제 시간을 임의로 기다리거나, 게임을 Pause하거나, Conveyor를 느리게 만드는 행동은 성장으로 인정하지 않는다.

### 7.4 Plasma

Line 정체성:

> 짧은 증폭 Burst를 만든 뒤 자원이 과부하되기 전에 방전한다.

#### Energized — 긍정

- Amplification 작업으로 Energy가 증가하면 활성화된다.
- 다음 연속 Amplification 작업에 합연산 보너스를 준다.
- 자원이 방전되거나 증폭 순서를 벗어나면 해제된다.

#### Overloaded — 부정

- Discharge 없이 세 번째 연속 Amplification을 수행할 때 활성화된다.
- Energized 보너스보다 큰 합연산 Penalty를 적용한다.
- Discharge 또는 Grounding 작업을 완료할 때까지 유지된다.

예시 주기:

```text
Amplify -> Amplify -> Discharge -> Amplify -> Amplify -> Discharge
```

Crystal과 달리 반복 Facility가 같은 Process Archetype일 필요는 없다. 핵심은 증폭 Burst와 초기화의 교대다.

### 7.5 Void

Line 정체성:

> Energy를 의도적으로 희생한 뒤 더 강한 합연산 반향으로 회수한다.

#### Echoing — 긍정

- Facility가 명시적인 Void Sacrifice 작업을 수행하면 활성화된다.
- 다음 유효 Energy 증가 작업이 희생량에 비례하거나 Family에 정해진, 상한이 있는 합연산 보너스를 받는다.
- 그 증가 작업에서 소비된다.

#### Collapsed — 부정

- 중간에 Void Sacrifice 없이 Energy 증가 작업을 두 번 수행하면 활성화된다.
- 이후 합연산 Energy 증가량을 낮춘다.
- Void Sacrifice 작업을 완료하면 해제된다.

다른 부정 State나 Tag 때문에 우연히 발생한 Energy 손실은 Void Sacrifice로 인정하지 않는다. Penalty를 무료 Echoing 보너스로 바꿀 수 없도록 Facility 작업 자체가 해당 의미를 명시해야 한다.

## 8. Family State 공통 규칙

1. Family State는 Tag가 아니라 Family 고유 규칙이다.
2. 가능한 State 목록은 Family에 한 번 정의되며, 해당 Family의 모든 자원이 공유한다.
3. 활성 State는 서로 독립적인 Runtime Flag다. Trigger 조건이 허용하면 긍정·부정 State가 동시에 켜질 수 있다.
4. State 전이는 결정적이어야 하며 플레이어가 통제할 수 있는 가공 Context로 발생해야 한다. 숨겨진 확률을 사용하지 않는다.
5. 현재 작업에서 Trigger가 충족되면 그 작업부터 효과가 적용된다. 예를 들어 Metal의 세 번째 동일 가공은 즉시 Fatigued Penalty를 받는다.
6. 회복 조건도 현재 작업부터 적용한다. Fatigued 상태로 도착했더라도 다른 Metal Process Archetype을 사용한 현재 작업에는 이전 Fatigued Penalty를 적용하지 않는다.
7. 최종 연료 단계 이전의 Family State 효과는 합연산이다. 기본적으로 최종 `C`를 바꾸지 않는다.
8. 일상적인 부정 State는 주로 현재 작업의 이득을 낮춰야 한다. Line 전체에서 쌓은 Energy의 큰 비율을 한 번에 삭제해서는 안 된다.
9. 항성 연료 제작기는 일반 Family State 전이를 발생시키지 않는다. 이전 State 효과는 이미 Current Energy에 반영되어 있다.
10. 모든 Facility Preview는 배치를 확정하기 전에 State 활성화·해제와 최종 Energy 변화량을 보여줘야 한다.
11. 취소되거나 막힌 작업은 Processing History와 Family State를 바꾸지 않는다.
12. 각 Family는 서로 다른 한 문장의 Line 건설 규칙으로 설명할 수 있어야 한다. 두 Family의 설명 문장이 거의 같다면 핵심 Trigger 중 하나를 바꿔야 한다.

초기 Balance Prototype에서는 모든 긍정 State에 같은 임시 합연산 보너스, 모든 부정 State에 그보다 큰 같은 임시 Penalty를 사용할 수 있다. Family별 정확한 수치 조정보다 각 Routing Pattern 자체가 재미있는지를 먼저 검증할 수 있다.

## 9. Spectrum과 Rank 족보 제약

실물 카드 덱과 달리 자동화에서는 같은 자원을 무한히 복제할 수 있다. 중복 규칙이 없으면 한 광맥에서 나온 같은 자원 5개만으로 구성 고민 없이 최고 족보를 만들 수 있다.

권장 카드 식별자:

```text
Card Key = Spectrum + Rank
```

권장 중복 처리:

- 같은 Card Key의 자원이 여러 개여도 모든 Current Energy는 최종 `B`에 더한다.
- 족보 Pattern을 셀 때는 같은 Card Key를 하나만 인정한다.
- 따라서 Pair는 R2와 B2처럼 Rank가 같고 Spectrum이 달라야 한다.
- Spectrum이 4개라면 일반 Rank 묶음은 최대 Four of a Kind까지 셀 수 있다.
- 같은 고 Energy 카드를 여러 장 보내는 것은 여전히 유효한 기본 Energy 전략이지만 좋은 족보를 포기하는 대가가 있다.

### 9.1 Rank 개수 권고

Rank가 정확히 5개이고 입력도 5장이라면:

- 완전한 연속 숫자는 1-2-3-4-5 하나뿐이다.
- 같은 Spectrum의 서로 다른 카드 5장은 반드시 모든 Rank를 포함한다.
- 따라서 일반 Flush가 자동으로 유일한 Straight Flush가 된다.

Poker에 가까운 구조를 유지하려면 Spectrum 4개와 Rank 7개를 권장한다. 이 경우 1-5, 2-6, 3-7의 세 Sequence가 생기며 같은 Spectrum이지만 연속되지 않은 족보도 가능하다.

Rank 1-5를 유지한다면 표준 Poker를 그대로 복제하지 말고 의도적으로 Custom 족보를 만들어야 한다. 예를 들어 같은 Spectrum 3장 또는 4장, Spectrum 4종 모두 포함, 유일한 1-2-3-4-5 Sequence 등을 사용할 수 있다.

Rank가 Seed Energy를 강하게 결정해서는 안 된다. 그렇지 않으면 높은 Rank가 족보 구성 요소와 원시 Energy 양쪽에서 모두 우월해진다. 숫자가 주로 조합을 위한 값이라면, 큰 숫자가 언제나 더 좋다는 인상을 주는 `Grade`보다 `Rank`라는 이름이 명확하다.

일반 가공은 Native Spectrum이나 Native Rank를 영구적으로 바꾸지 않는다. 향후 임시 Override를 도입한다면 명확하게 표시하고, 범위를 좁게 제한하며, 정의된 Event에서 소비해야 한다. 영구 변경은 의미 있는 비용을 가진 희귀한 Run 보상으로만 제공하는 편이 좋다.

## 10. 천체 간 자원 연속성

천체 간 Route는 자동화 Line의 일부이지 상태 초기화 경계가 아니다.

- 일반 Hub 운송은 Current Energy, Family State, Tag, Spectrum, Rank와 필요한 Processing History를 보존한다.
- 일반 운송은 Energy를 더하거나 빼지 않으며 가공 횟수로 세지 않는다.
- 운송만으로 Fatigued, Fractured, Depleted, Overloaded, Collapsed 등의 부정 State가 무료로 해제되지 않는다.
- 일반 화물칸에서 보낸 시간은 Organic Growth Cycle로 세지 않는다.
- 명시적인 Hub, Cargo Hold, Route Module, Augment 규칙이 선언한 경우에만 운송 중 State나 Tag가 바뀐다.
- 한 천체에서 시작한 가공 순서를 다른 천체에서 이어갈 수 있다.
- 최종 연료 조립은 여러 천체에서 완성된 카드 자원 5개를 모을 수 있다.

선택적 물류 전략에는 Origin Body, Last Processed Body, Last Transit Source와 Destination, 방문 천체 Mask 같은 작은 내부 Metadata가 필요할 수 있다. 이 값은 새로운 대표 자원 속성이 아니다. 결정적인 Tag 또는 Augment가 실제로 필요로 할 때만 존재하며 Route와 Facility Preview에서 확인할 수 있어야 한다.

모든 자원에 전체 이동 이력을 저장해서는 안 된다. 상세 이력은 Stack을 지나치게 분할하고 많은 천체와 자동화 Line에서 확장성이 나쁘므로, 활성 규칙을 판정하는 최소 State만 유지한다.

## 11. 합성과 특수 자원 규칙

- 단일 자원 가공은 해당 자원의 정체성과 Family를 유지한다.
- 일반 다중 입력 Recipe는 출력 자원과 출력 Family를 명시한다. 첫 입력이나 다수결로 Family를 추론하지 않는다.
- 새로 합성된 자원은 출력 Family의 State가 모두 꺼진 상태로 시작하며 해당 Family의 Processing History도 초기화한다.
- 입력 State 이력은 다른 Family로 이전되지 않는다.
- Star Fuel은 최종 Energy만 가지며 Family, Spectrum, Rank, 재사용 가능한 가공 Loop가 없는 종결 자원이다.
- Waste는 연료가 아닌 `None` 또는 `Waste` 분류에 속하며, 향후 명시적인 규칙을 추가하지 않는 한 항성 연료 제작기의 유효 카드가 아니다.
- Industrial Supply 같은 기반 시설용 물자는 카드가 아닌 `Utility` 분류에 속한다. 생산·운송·소비할 수 있지만 Family State나 연료 족보에 참여하지 않으며 항성 연료 제작기 입력으로 사용할 수 없다.
- 완성된 Star Fuel은 다시 가공하거나 항성 연료 제작기에 재투입할 수 없다.

## 12. 숨겨진 이력과 자원 Stack

숨겨진 이력은 기본 자원 카드에서 생략할 수 있지만 결과 예측에서는 생략할 수 없다.

- Facility Preview는 다음 작업의 State와 Energy 결과를 보여줘야 한다.
- 다음 Facility에서 서로 다른 결과를 낼 이력을 가진 자원은 동등한 것으로 취급할 수 없다.
- Stack 동등성은 Current Energy, 활성 Family State, Tag, Spectrum 또는 Rank Override, 필요한 Processing History를 고려해야 한다.
- 천체 간 화물도 같은 동등성 규칙을 유지한다. Resource ID가 같다는 이유만으로 기계적으로 다른 Instance를 합치지 않는다.
- Inventory가 시각적으로 같은 자원을 지나치게 많은 Stack으로 분할하지 않도록 숨은 Counter는 Family 규칙에 필요한 최소치만 유지한다.
- 가공 제한 규칙을 제거했으므로 어떤 숨은 값도 예고 없이 자원을 가공 불가 상태로 만들어서는 안 된다.

Preview 예시:

```text
입력: Star Iron, Current Energy 12, 활성 State 없음
작업: Hot 가공 다음에 Reference Cryo Press 수행
결과: Facility +3, Tempered +5, Current Energy 20
```

## 13. 자원 정의 예시

다음 예시는 속성 구조를 검증하기 위한 것이다. 이름과 수치는 최종 Balance 값이 아니다.

| Name | Family | Seed Energy | Spectrum | Rank | 사용 가능한 Family State |
|---|---|---:|---|---:|---|
| Star Iron | Metal | 5 | Red | 1 | Tempered, Fatigued |
| Echo Quartz | Crystal | 4 | Blue | 2 | Resonant, Fractured |
| Verdant Spore | Organic | 3 | Green | 4 | Matured, Depleted |
| Aurora Plasma | Plasma | 6 | Yellow | 4 | Energized, Overloaded |
| Null Pearl | Void | 2 | Red | 4 | Echoing, Collapsed |

권장 분리 모델에서는 같은 Family 안에서도 Spectrum과 Rank가 다른 자원이 존재할 수 있다. Rank가 높다고 Seed Energy가 높다고 가정하지 않는다.

## 14. 현재 권장 Baseline

다음은 명시적인 승인 전까지의 권장 기준이다.

1. Metal, Crystal, Organic, Plasma, Void의 다섯 Family를 사용한다.
2. 각 Family는 긍정 State 하나와 부정 State 하나로 시작한다.
3. Family와 Spectrum을 서로 독립적으로 유지한다.
4. Seed Energy와 Current Energy를 분리한다.
5. 최종 연료 제작 전 모든 자동화에서 Energy 합연산만 사용한다.
6. `B * C`는 항성 연료 제작기에서 정확히 한 번만 적용한다.
7. Remaining Process Limit과 가공 이력을 이용한 가공 거부 규칙을 제거한다.
8. Poker와 유사한 족보가 목표라면 Spectrum 4개와 가능하면 Rank 7개를 사용한다.
9. 동일한 Spectrum + Rank Card Key는 족보 계산에서 하나만 세되, 모든 입력의 Current Energy는 유지한다.
10. Family State는 Line 건설에, Spectrum과 Rank는 최종 조합에 집중시킨다.
11. Star Fuel은 재가공할 수 없는 종결 결과로 취급한다.
12. 일반 천체 간 운송에서 자원 상태를 보존한다.
13. Route 전용 변화는 수동적인 이동 부가 효과가 아니라 명시적인 가공 Event로 다룬다.
14. 전체 지표면 면적을 Line 길이의 주된 제약으로 사용하지 않는다.

## 15. 미결정 사항

다음 선택은 아직 확정되지 않았다.

1. 최종 명칭을 `Grade`, `Rank`, 또는 세계관 전용 용어 중 무엇으로 할지
2. Custom 족보와 Rank 1-5를 유지할지, Poker에 가까운 족보를 위해 1-7로 확장할지
3. Family와 Spectrum을 독립적으로 유지할지, Family가 Spectrum을 직접 대체할지
4. Current Energy를 0에서 Clamp할지
5. 각 Family State의 정확한 활성 보상과 Penalty
6. 정확한 족보 목록과 각 족보의 `B`, `C` 보너스
7. 어떤 Tag 효과를 일반 가공에 적용하고 어떤 효과를 최종 연료 합성에만 적용할지
8. Prototype 이후 Operational Capacity, Service Core, Industrial Supply의 정확한 수치
9. 승인된 Tag와 Augment가 필요로 하는 최소 물류 이력 필드
