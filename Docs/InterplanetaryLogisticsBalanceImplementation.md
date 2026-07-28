# 천체간 물류 Profile 차별화 구현

## 목적

이 단계는 천체간 Route가 단순히 같은 수치의 선체 이름만 바꾸는 선택이 되지 않도록 다음 세 계약을 일치시킨다.

- 운송 선체마다 허용 Cargo와 Fleet 효율이 실제로 다르다.
- 전략 증강이 설명에만 등장하지 않고 Route Profile을 실제로 해금한다.
- Hub와 Augment UI가 선체 Profile과 장착 Module을 구분해 보여 준다.

## Route Profile 계약

| Profile | 해금 | 허용 Cargo | Cargo | Fleet Load | Cargo / Load | 용도 |
|---|---|---|---:|---:|---:|---|
| Neutral Shuttle | Technology | 모든 Cargo | 8 | 2 | 4.0 | 호환, 복구, Legacy 이관 |
| Card Courier | Technology | Card | 12 | 2 | 6.0 | 일반 Card 간선 운송 |
| Bulk Raw Hold | Central Convergence | Utility 또는 가공 전 Card | 16 | 3 | 5.3 | 중앙집중형 공장의 원재료 수입 |
| Conditioned Hold | Conditioned Transit Engine 하나 이상 | Card | 4 | 3 | 1.3 | 이동 중 Family Action을 포함하는 순차형 Line |

Card Courier를 기존 `8 / 2`에서 `12 / 2`로 조정했다. 기존 수치에서는 Neutral Shuttle과 효율이 같으면서 Cargo 제약만 더 커서 선택할 이유가 없었다. 현재 수치에서는 범용성은 Shuttle, Card 처리량은 Courier가 담당한다.

## 가공 전 Card 판정

Bulk Raw Hold가 Card를 받으려면 다음 조건을 모두 만족해야 한다.

- Resource Class가 Card다.
- `ProcessingMemory.ProcessCount == 0`이다.
- `ProcessingMemory.EnergyChangeCount == 0`이다.
- 활성 Family State가 없다.
- Process Tag Slot이 비어 있다.
- Fuel Imprint Slot이 비어 있다.

Transit 횟수와 Origin 정보는 Raw 판정에서 제외한다. 따라서 산지에서 중앙 공장까지 여러 Hub를 경유해도 중간 가공을 하지 않았다면 계속 원재료로 취급한다. 반대로 Energy가 변하지 않은 공정이라도 Process Count가 증가했다면 완성·중간 Card로 보고 Bulk 적재를 거부한다.

## Augment 해금 연결

| Package | Route Profile Grant | Route Module Grant |
|---|---|---|
| Central Convergence | Bulk Raw Hold | 없음 |
| Deep-Space Tempering | Conditioned Hold | Cryogenic Hold |
| Bio-Ark Freight | Conditioned Hold | Bio-Culture Hold |
| Grounded Transit | Conditioned Hold | Grounding Hold |

Neutral Shuttle과 Card Courier는 Technology Profile이므로 처음부터 사용할 수 있다. Bulk Raw Hold와 Conditioned Hold는 `SetHubRouteProfile`에서도 해금 여부를 검사하므로 UI를 우회해 직접 호출해도 잠긴 Profile을 선택할 수 없다. Legacy Ruleset에서는 기존 Route 호환을 위해 이 잠금 검사를 적용하지 않는다.

Conditioned Transit Package 세 개가 같은 Conditioned Hold를 중복 Grant하는 것은 의도된 예외다. 세 Package가 모두 같은 `ConditionedTransit` Strategy에 속하고 각자 정확히 하나의 유효한 Family Module을 함께 제공할 때만 Catalog 검증을 통과한다. 서로 다른 Strategy가 같은 잠긴 Profile을 나눠 갖는 구성은 거부한다.

## UI 계약

- Hub의 Profile 버튼은 잠긴 Profile을 건너뛰고 현재 Run에서 사용할 수 있는 Profile만 순환한다.
- 잠긴 Profile을 직접 선택하려 하면 필요한 Augment Package를 먼저 선택하라는 상태 메시지를 표시한다.
- Route 카드와 Profile 버튼 Tooltip은 허용 Cargo, 최대 Cargo, Fleet Load, `cargo/load`를 함께 표시한다.
- Augment 카드는 `ROUTE PROFILE`과 `ROUTE MODULE`을 별도 신규 해금으로 센다.
- Conditioned Hold Profile은 작은 특수 선체의 비용을, Cryogenic/Bio-Culture/Grounding Module은 실제 이동 중 가공 효과를 각각 설명한다.

## Catalog 안전 규칙

- 알 수 없는 Profile ID를 Grant할 수 없다.
- Technology Profile을 Augment가 다시 Grant할 수 없다.
- 잠긴 모든 Profile에는 최소 하나의 Augment 해금 경로가 있어야 한다.
- Profile Package는 Hub 두 개 이상을 요구해야 한다.
- Bulk Raw Hold는 전체 물류 형태를 바꾸는 Macro Doctrine만 Grant할 수 있다.
- Conditioned Hold는 유효한 Conditioned Module 하나와 함께 Grant해야 한다.
- 같은 Profile의 중복 Grant는 같은 Strategy 안에서만 허용한다.

## 검증 항목

- `StarRovers.ResourceSystem.Phase18.Logistics.ProfileDifferentiation`
- `StarRovers.ResourceSystem.Phase18.Logistics.AugmentUnlockPaths`
- 전체 `StarRovers.ResourceSystem`
- 전체 `StarRovers.UI`
- D3D12 PIE Augment Choice 화면

최종 검증 결과:

- `StarRoversEditor Win64 Development`: 전체 및 증분 빌드 성공
- `StarRovers.ResourceSystem.Phase18`: 2/2 성공
- `StarRovers.ResourceSystem`: 69/69 성공
- `StarRovers.UI`: 62/62 성공
- D3D12 PIE `StarRovers.UI.AugmentChoice.PIE.NativeImpactPreview`: 1/1 성공

D3D12 PIE 케이스는 일반 Tag 카드뿐 아니라 Deep-Space Tempering 카드도 생성하고, 세 번째 카드에 Route Profile과 Route Module의 두 규칙 행이 모두 만들어지는지 검사한다.
