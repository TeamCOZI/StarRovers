# Tag·Augment Offer 정비 구현 기준

## 목적

이번 단계는 Tag와 Augment가 단순한 수치 상승 선택으로 수렴하거나, 같은 선택지가 연속해서 등장하거나, 선택해도 실제 Line에서 사용할 수 없는 상황을 막는다.

핵심 원칙은 다음과 같다.

- Process Tag는 자동화 Line의 특정 행동을 한 번 보상한다.
- Augment는 Energy를 즉시 지급하지 않고 Recipe, Fuel Imprint 또는 Route Module을 해방한다.
- 한 Offer의 세 카드는 가능한 동안 서로 다른 Line 전략을 제안한다.
- 직전 Offer에서 선택하지 않은 카드는 다른 후보가 있는 동안 바로 반복하지 않는다.
- 생존과 첫 항성 연료 제작에 필요한 Technology Facility는 무작위 Offer에 잠그지 않는다.

## Process Tag 작성 계약

현재 `FSRProcessTagDefinitionV2`는 Energy 합연산만 표현할 수 있다. 최종 `C`, Spectrum, Grade, Family State 직접 변경 필드가 없으므로 Process Tag가 최종 곱연산이나 Poker 조합을 우회할 수 없다.

Catalog 검증은 다음 조건을 강제한다.

| 항목 | 허용 규칙 | 막는 문제 |
|---|---|---|
| 수명 | `TriggerCount == 1` | 영구 버프와 무한 재발동 |
| 보상 | `0 < EnergyDelta <= 8` | 무효 Tag, 벌칙 Tag, 압도적인 고정 수치 |
| 계산 | 유한한 합연산만 허용 | NaN, 무한대, 일반 가공 중 곱연산 |
| Trigger | 구현된 명시 Trigger만 허용 | 발동할 수 없는 숨은 조건 |
| 차별화 | Catalog에서 Trigger 중복 금지 | 같은 조건에서 수치만 높은 상위호환 Tag |
| 식별 | ID와 표시 이름 필수 | UI와 Save에서 식별 불가능한 Recipe |

현재 기준값은 다음과 같다.

| Tag | 플레이어가 만드는 조건 | Energy |
|---|---|---:|
| Overtone | 긍정 Family State 활성화 | +5 |
| Reclamation | 부정 Family State 제거 | +7 |
| Crosslink | 직전과 다른 Process Archetype 사용 | +4 |
| Landing Charge | 수입 후 첫 Energy 변경 가공 | +5 |
| Pilgrim Charge | Origin 밖에서 첫 유효 가공 | +6 |

상한 8은 Prototype 안전선이다. 이 값을 높이기 전에 Family의 긍정 State 보너스, 평균 Facility Energy, Tag Imprinter의 시간·Capacity 비용을 함께 비교해야 한다.

## 피해야 하는 Tag 효과

다음 효과는 Process Tag에 추가하지 않는다.

- 일반 가공에서 `C`를 올리는 효과
- Spectrum 또는 Grade를 영구 변경하는 효과
- 부정 Family State를 조건 없이 제거하는 효과
- Tag 적용 설비에서 즉시 발동하는 자기 완결 효과
- 같은 Trigger를 사용하면서 Energy만 더 높은 효과
- Trigger 후 다시 스스로 Primed가 되는 효과
- 모든 Card나 Batch에 적용되는 무조건 최종 배율

최종 `B`와 `C`에 관여해야 하는 효과는 Fuel Imprint로 분리하고 항성 연료 제작기에서 한 번만 평가한다.

## Package Catalog 안전 계약

모든 Package는 최소 하나의 실제 신규 해금을 가져야 한다. 이미 Technology 또는 선택한 Package로 접근 가능한 내용만 다시 주는 Package는 Offer와 실제 선택 양쪽에서 거부된다.

또한 Catalog 시작 시 다음을 검증한다.

- 모든 선행 Package와 Facility ID가 존재하며 자기 참조와 순환 참조가 없다.
- Process Tag Package는 Tag Imprinter 접근을 요구한다.
- Fuel Imprint Package는 Fuel Imprinter와 항성 연료 제작기 접근을 요구한다.
- Conditioned Transit Module은 해당 Family와 Hub 2개 이상을 요구한다.
- 모든 Tag, Fuel Imprint, Route Module에는 정확한 Technology 또는 Augment 해금 경로가 있다.
- 같은 Grant를 두 Package가 중복 소유하지 않는다.
- Capstone은 같은 Strategy의 선행 Package를 확장한다.
- Macro Doctrine은 공용 Doctrine Slot, 다천체 Network, Topology Imprint를 모두 요구한다.
- 서로 다른 Macro Doctrine은 서로 다른 Line Strategy로 남아 비교 가능해야 한다.

이 검증은 PIE의 Resource System Validation에도 포함된다.

## Run 접근성의 기준

`RegisteredStructureDataAssets`는 Build Catalog 전체이므로 Resource 접근성의 근거로 사용하지 않는다. 그렇게 하면 이번 태양계에 생성되지 않은 Organic, Plasma 등의 DA도 보유한 것처럼 판단되어 발동할 수 없는 Package가 나타난다.

Offer Context의 Family, Spectrum, Grade는 다음 조건을 모두 만족하는 실제 Runtime 광맥에서만 수집한다.

- 현재 Celestial Body Registry에 등록된 천체
- 건설 가능한 천체
- 실제 Surface에 배치되어 Resource Deposit Instance를 가진 구조물
- 잔량이 남아 채굴 가능한 광맥
- 현재 Resource V2 Definition을 사용하는 Card 자원

프로젝트에 존재하는 5개 Reference Resource를 빈 Context에 자동 삽입하던 Migration Fallback은 제거했다. 따라서 생성이 끝나지 않았거나 유효 Card 광맥이 없는 상황에서는 호환 Family를 요구하는 Tag·Doctrine Package가 나오지 않는다. SolarSystem PIE는 실제 광맥에서 직접 계산한 Family, Spectrum, Grade 집합과 Augment Subsystem의 Context가 정확히 같은지 검사한다.

## Offer 생성 순서

Offer는 현재 Run에서 즉시 선택 가능한 Package만 후보로 사용한다. 기본 세 슬롯은 다음 우선순위로 채운다.

1. **Immediate**: Macro Doctrine과 Capstone이 아닌, 지금 바로 Line에 넣을 수 있는 안전한 구성 요소
2. **Capstone 또는 Synergy**: 이미 선택한 Strategy의 완성 보상이나 연계 Package
3. **Pivot**: 아직 이번 화면에 나오지 않은 다른 Strategy의 Line 형태

후보가 충분하면 한 Offer 안에서 같은 `StrategyId`를 두 번 사용하지 않는다. 후보가 적어 세 장을 채울 수 없는 경우에만 같은 Strategy의 다른 Package를 허용한다. 따라서 선택지 수를 억지로 줄이지 않으면서 정상적인 중·후반 Offer의 의미를 분리한다.

Family별 Conditioned Transit Package 세 개는 모두 `ConditionedTransit` Strategy를 사용한다.

- Deep-Space Tempering: Metal / Cryogenic Hold
- Bio-Ark Freight: Organic / Bio-Culture Hold
- Grounded Transit: Plasma / Grounding Hold

이 셋은 호환 Family와 실제 Route 행동은 다르지만 같은 “운송 구간을 가공 단계로 사용한다”는 결정이다. 따라서 후보가 충분한 Offer에서는 최대 한 장만 차지한다.

반면 세 Macro Doctrine은 분산형, 중앙 집중형, 순차형이라는 서로 다른 전체 Network 구조이므로 각각 별도 Strategy로 유지한다. 같은 Offer에서 둘 이상 나타나는 것은 중복이 아니라 배치 방식의 직접 비교다.

## 연속 Offer 다양성

Subsystem은 직전에 제시한 Package ID를 한 세대 기억한다.

- 같은 역할과 조건을 만족하는 새 Package가 있으면 새 Package를 우선한다.
- 새 후보가 부족하면 최근 Package도 다시 허용하여 세 장 Offer를 유지한다.
- 플레이어가 선택한 Package는 선택 완료 목록 때문에 자연스럽게 후보에서 제거된다.
- 같은 Strategy의 다음 단계인 Synergy와 Capstone은 진행을 막지 않도록 역할 우선순위를 유지한다.

이 기억은 현재 Run의 선택 다양성을 위한 상태이며, Save/Migration 단계에서 선택 Package, Macro Doctrine, Pity, 보류 중인 선택지와 함께 영속화했다. Load 직후에도 직전 Offer 회피와 선택 중 Pause 계약이 유지된다. 세부 내용은 [ResourceV2RunSaveMigrationImplementation.md](ResourceV2RunSaveMigrationImplementation.md)를 참고한다.

## 예시

### 첫 지역 Line 단계

Hub가 하나뿐인 초반에는 다천체 Package가 후보에서 제외된다. 예시는 다음처럼 구성된다.

```text
IMMEDIATE  State Resonator   -> Overtone으로 긍정 State 주기 강화
PIVOT      Full-House Matrix -> Grade 2+4 최종 조합 준비
PIVOT      Recovery Dividend -> 의도적인 부정 State 회복 Line
```

사용할 수 없는 운송 Module이나 4-Spectrum Capstone은 이 시점에 나오지 않는다.

### Full-House 선택 후 다천체 단계

Grade 2·4, 네 Spectrum, Hub Network가 준비되고 Full-House Matrix를 이미 선택했다면 다음 구조가 가능하다.

```text
IMMEDIATE  Deep-Space Tempering -> Metal 운송 구간을 Cold 공정으로 사용
CAPSTONE   Prismatic Focus      -> 네 Spectrum Batch의 최종 C +1
PIVOT      Central Convergence  -> 중앙 공장 Doctrine으로 전환
```

세 선택은 각각 Route 가공, 현재 조합 완성, 전체 물류 Topology 변경을 의미한다. 같은 수치를 조금씩 더 주는 세 카드가 아니다.

## 검증 기준

- `StarRovers.ResourceSystem.Phase17.Tag.AuthoringPolicy`
- `StarRovers.ResourceSystem.Phase17.Augment.OfferDiversityAndDeterminism`
- 전체 `StarRovers.ResourceSystem` 회귀
- 전체 `StarRovers.UI` 회귀
- 렌더링 PIE에서 Augment 카드 생성, Recipe 표시, 선택 가능 상태 확인

최종 실행 결과:

- `StarRoversEditor Win64 Development`: 성공
- `StarRovers.ResourceSystem.Phase17`: 2/2 성공
- `StarRovers.ResourceSystem`: 67/67 성공
- `StarRovers.UI`: 62/62 성공
- D3D12 PIE `StarRovers.UI.AugmentChoice.PIE.NativeImpactPreview`: 1/1 성공
