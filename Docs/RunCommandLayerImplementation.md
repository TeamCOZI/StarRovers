# Run 지휘 레이어 구현 계획

## 목표

플레이어가 긴 설명을 읽지 않고도 Run 시작 후 3초 안에 다음 세 가지를 판단할 수 있게 한다.

1. 별이 현재 안전한가.
2. 지금 해결해야 할 병목은 무엇인가.
3. 어느 천체·자원·설비를 눌러야 하는가.

이 문서는 Resource V2의 규칙을 다시 설계하는 문서가 아니라, 이미 구현된 규칙을 HUD·월드·선택 화면에서 일관되게 읽히도록 만드는 구현 추적 문서다.

## 구현 단계와 가중치

| 단계 | 범위 | 가중치 | 상태 |
|---|---|---:|---|
| 0 | 기존 UI·Simulation·PIE 기준선 점검 | 5% | 완료 |
| 1 | 플레이/Debug 해금 분리와 Run 시작 일시정지 | 8% | 완료 |
| 2 | 별 생존 Rail: 잔여시간·실측 유입·소비·수지·도착 예측 | 15% | 완료 |
| 3 | 첫 연료 Milestone과 직접 Action | 15% | 완료 |
| 4 | 시작 System Scan과 추천 천체·광맥 | 12% | 완료 |
| 5 | 공통 Resource Glyph와 핵심 화면 적용 | 12% | 완료 |
| 6 | Build Dock 추천 설비·변환 Diagram·배치 Preview | 10% | 완료 |
| 7 | Augment 조건→효과 카드와 Run 적합도 | 8% | 완료 |
| 8 | 천체·Route 전략 Overlay와 월드 병목 표현 | 8% | 완료 |
| 9 | 전체 회귀·PIE 가독성 검증·문서 정리 | 7% | 완료 |

현재 전체 진행률: **100%**

## 단계 1 결과

- `bPauseSimulationOnRunStart`를 Simulation Settings에 추가했다.
- 프로젝트 기본 Run은 정지된 계획 모드로 시작한다.
- Play 버튼을 누르기 전에는 주기, 시설 처리, 물류, 항성 연료 감소가 진행되지 않는다.
- 프로젝트 기본 설정에서 모든 설비 및 Resource V2 Augment Package의 Debug 해금을 비활성화했다.
- Debug 해금 옵션 자체는 Developer Settings에 남아 있어 전용 검증 시 명시적으로 사용할 수 있다.
- 설정 계약과 PIE의 실제 시작 정지 상태를 자동화 테스트로 검증한다.

검증 결과:

- `StarRoversEditor Win64 Development`: 성공
- `StarRovers.UI.RunCommand`: 1/1 성공
- `StarRovers.UI`: 29/29 성공
- `StarRovers.ResourceSystem`: 61/61 성공

## 단계 2 결과

- 상단 중앙에 항상 보이는 항성 생존 Rail을 추가했다.
- Rail은 긴 문장 대신 `별`, `목표`, `유입`, `소비`, `수지`, `도착`의 여섯 상태 칩으로 현재 Run을 요약한다.
- `유입`은 설비의 이론 생산량이 아니라 항성에 실제 도착한 연료의 최근 30초 평균이다. Simulation이 정지하면 관측 시간도 함께 멈춘다.
- `수지`는 `실측 유입/초 - 현재 소비/초`로 계산하며, 흑자·적자·안정 상태를 텍스트와 색상으로 함께 구분한다.
- 현재 적자가 지속될 때의 기본 잔여시간과, 고갈 전에 도착할 수 있는 비행 중 연료 수송을 순서대로 반영한 `확보 생존시간`을 계산한다.
- 다음 도착 연료량과 ETA를 별도 칩으로 보여 주어 이미 확보한 물류와 아직 실현되지 않은 생산을 혼동하지 않게 했다.
- 다음 항성 주기의 예상 소비량과 현재 실측 유입 대비 부족량을 Tooltip에 표시한다.
- 모든 상세 계산 근거는 Rail Tooltip으로 내리고, 기본 HUD에는 순간 판단에 필요한 숫자만 남겼다.

예측 범위:

- 확보 생존시간은 현재 소비·최근 30초 실측 유입·현재 비행 중인 연료 수송을 기준으로 한 운영 예측이다.
- 미래에 새로 발사될 수송, 아직 처리되지 않은 자원, 다음 주기에 발생할 소비 증가까지 확정값처럼 합산하지 않는다.
- 다음 주기 소비 증가는 별도 경고로 제공해 현재 생존시간과 장기 위험을 구분한다.

검증 결과:

- `StarRoversEditor Win64 Development`: 성공
- `StarRovers.UI`: 32/32 성공
- `StarRovers.ResourceSystem`: 61/61 성공
- PIE 통합 검증: 실제 프로젝트 HUD의 여섯 Rail 칩 생성, 새 Run 일시정지, 주 항성 연결, 실제 연료 도착의 30초 관측치 반영 성공

## 단계 3 결과

- 첫 항성 연료 완성 과정을 `채굴기 배치 → 첫 Card 채굴 → Family 가공기 배치 → 첫 Card 가공 → 항성 연료 제작기 배치 → 첫 연료 제작 → Hub 배치 → 첫 연료 발사 → 항성 도착`의 아홉 Milestone으로 정의했다.
- Milestone은 현재 구조물 개수만 보는 체크리스트가 아니다. 설비가 실제 자원을 생산할 때 발생하는 이벤트와 월드의 권위 상태 재탐색을 함께 사용해 빠르게 이동한 자원, 늦게 생성된 UI, 저장 데이터 복원도 추적한다.
- 한 번 증명된 단계는 자원 소비나 설비 철거 뒤에도 되돌아가지 않는다. 후행 결과를 먼저 관측했다면 이미 지나간 선행 단계도 완료된 것으로 복원한다.
- Guidance에는 항상 `첫 연료 n/9` 형식의 짧은 목표 하나와 직접 행동 버튼 하나만 노출한다.
- 건설 단계의 버튼은 첫 Card를 얻을 수 있는 천체로 이동하고, 조립 모드를 열고, 현재 단계에 맞는 설비를 Build Dock에서 선택한다.
- 생산 대기 단계에서 Simulation이 정지되어 있으면 즉시 재생하고, 이미 가동 중이면 해당 설비의 Inspector로 이동한다. 발사 후 도착 대기 단계에서는 주 항성으로 이동한다.
- 항성 위험 같은 긴급 Guidance와 Augment·Game Over 같은 Modal은 첫 연료 안내보다 우선하며, 플레이를 방해하는 중복 행동을 만들지 않는다.
- 행성과 위성이 표면 건설 공간을 보유하면서도 런타임 데이터의 `bCanConstruct`를 지정하지 않아 모든 시작 천체가 건설 불가로 판정되던 기존 결함을 발견했다. 행성·위성은 명시적으로 건설 가능, 항성은 건설 불가로 고쳤다.

단계 3 당시 경계:

- 단계 3의 시작 천체 선택은 건설 가능한 Card 산출 천체 중 결정적인 첫 후보를 사용했다.
- 이 임시 선택은 단계 4의 System Scan으로 교체했다.

검증 결과:

- `StarRoversEditor Win64 Development`: 성공
- `StarRovers.UI.RunCommand.FirstFuelMilestone`: 4/4 성공
- `StarRovers.UI`: 36/36 성공
- `StarRovers.ResourceSystem`: 61/61 성공
- PIE 통합 검증: 첫 목표 표시, 행동 버튼 입력, 추천 천체 Focus, 조립 모드 전환, 채굴 설비 선택까지 실제 프로젝트 HUD에서 성공

## 단계 4 결과

- 비동기 태양계 생성이 끝나고 실제 건설 가능한 표면이 준비된 뒤에만 최초 System Scan을 확정한다. 첫 완전한 결과를 Run 동안 고정하므로 시설을 지을 때마다 추천이 흔들리지 않는다.
- 후보는 추상적인 천체가 아니라 `천체 + 실제 Card 광맥` 쌍이다. Family 가공 설비를 사용할 수 있고 광맥 주변에 건설 가능한 셀이 있는 후보만 시작 추천으로 채택한다.
- 추천 점수는 `Base Energy 35 + 주 항성 근접도 25 + Operational Capacity 여유 20 + Family 가공 준비 10 + 인접 건설 공간 10`, 총 100점이다.
- Spectrum과 Grade는 좋고 나쁜 값을 가진 단순 품질 수치가 아니라 최종 5장 조합을 구성하는 전략 정보이므로 추천 점수에 넣지 않았다. 대신 추천 배지와 상세 Tooltip에 그대로 노출한다.
- 동점은 Base Energy, 거리, Capacity 여유, 안정 식별자 순으로 결정해 같은 Seed의 Run이 컨테이너 순서 때문에 다른 추천을 내지 않게 했다.
- 서로 다른 `천체 + 자원` 기준의 상위 후보를 최대 3개까지 Snapshot에 보존한다. 이번 단계의 기본 HUD는 순간 판단을 위해 1순위만 표시하고, 나머지 후보 비교 UI는 이후 전략 Overlay 단계에서 재사용할 수 있다.
- 첫 Guidance는 `SYSTEM SCAN` 한 줄에서 추천 천체와 자원을 명명하고, Family·Spectrum/Grade·Energy·Capacity 여유·거리대·총점을 짧게 보여 준다. 세부 점수 근거는 Tooltip으로 내렸다.
- Overview 목록과 월드 Nameplate에서 추천 천체를 금색 별표와 `Spectrum + Grade` 배지로 같은 방식으로 강조한다.
- `채굴기 배치` Action은 추천 천체만 여는 데서 끝나지 않는다. 정확한 광맥으로 표면을 회전·중앙 정렬하고 그 자연 구조물을 선택한 상태에서 조립 모드와 채굴 설비 선택을 함께 유지한다.

단계 4 당시 경계:

- 단계 4는 추천 계산과 첫 행동 경로를 완성했지만 Resource 정보 표현은 아직 화면별 텍스트에 의존한다.
- 다음 단계에서는 Family·State·Spectrum·Grade·Energy를 같은 모양으로 읽는 공통 Resource Glyph를 만들고 Guidance, Build Dock, Inspector, 물류 UI에 적용한다.

검증 결과:

- `StarRoversEditor Win64 Development`: 성공
- `StarRovers.UI.RunCommand.SystemScan.ScoringAndOrdering`: 성공
- `StarRovers.UI`: 37/37 성공
- `StarRovers.ResourceSystem`: 61/61 성공
- PIE 통합 검증: 스캔 완료, 유효 후보 산출, 추천 문구, Overview 추천 판정, 정확한 광맥 선택, 조립 모드 유지, 채굴 설비 선택까지 실제 프로젝트 HUD에서 성공

## 단계 5 결과

- 화면마다 따로 조합하던 자원 문자열을 순수 Presentation Builder와 Native `Resource Glyph` Widget 하나로 통합했다.
- Family는 고유 도형과 고정 약어를 함께 쓴다: `■ MTL`, `◆ CRY`, `● ORG`, `▲ PLS`, `✦ VOI`. 공용 설비는 `◇ SHR`로 구분한다.
- Spectrum과 Grade는 한 토큰으로 묶는다: Red `◆ Rn`, Green `▲ Gn`, Blue `● Bn`, Yellow `■ Yn`. 색상을 보지 못해도 도형·문자·숫자로 같은 정보를 읽을 수 있다.
- Energy는 카드 표면에서 `E 55`, `E 1.2K`처럼 축약하되 Tooltip에는 정확한 소수 값을 유지한다.
- 이로운 Family State는 `+TMP`, 해로운 State는 `-FAT`처럼 부호와 약어를 함께 표시한다. 긍정·위험 색상은 보조 정보이며 부호가 의미의 권위다.
- Process Tag는 `TAG`, Fuel Imprint는 `SEAL`, Stack은 `xN` 접두어로 서로 다른 수명과 역할을 혼동하지 않게 했다.
- 일반 카드에는 의사결정에 필요한 Family·Spectrum/Grade·Energy·State·Tag·Imprint만 노출한다. 내부 처리용 `Process Count`, `Energy Change Count`, Processing Memory 진단치는 Tooltip에도 노출하지 않는다.
- System Scan Guidance는 장문의 Family·Spectrum·Grade·Energy 문장을 Glyph로 교체하고, 남은 한 줄은 Capacity·항성 거리·추천 점수에만 사용한다.
- Build Dock의 설비 카드와 상세 패널에는 해당 설비가 다루는 Family Glyph를 표시한다. 공용 물류·기반 설비도 `SHR`로 명시해 Family 전용 설비와 구분한다.
- Facility Inspector의 입력 슬롯, 출력 슬롯, 입력/출력 Preview는 모두 같은 상세 Glyph를 사용한다. 따라서 실제 자원과 처리 예상 결과를 같은 문법으로 비교할 수 있다.
- Hub Route는 적재 중인 실제 Cargo를 상세 Glyph로 표시하고, 기존 텍스트 줄은 Filter와 Stack Limit 같은 운송 정책만 담당한다.
- Overview의 최초 추천 배지도 공통 Spectrum/Grade 토큰 생성기를 사용하므로 Guidance와 월드 목록의 기호가 어긋나지 않는다.
- Family 및 Spectrum Accent를 공통 UI Theme 설정으로 이동해 이후 전체 HUD에서 같은 팔레트를 재사용할 수 있게 했다.

현재 경계:

- Resource Glyph는 자원의 정체성과 현재 상태를 일관되게 보여 주지만, 어떤 설비가 다음 최선인지 또는 처리 전후에 무엇이 변하는지는 판단하지 않는다.
- 다음 단계에서는 Build Dock에 추천 이유, 입력→설비→출력 변환 Diagram, 실제 배치 Preview를 결합해 Glyph를 행동 선택으로 연결한다.

검증 결과:

- `StarRoversEditor Win64 Development`: 성공
- `StarRovers.UI.ResourceGlyph.Presentation`: 성공
- `StarRovers.UI`: 38/38 성공
- `StarRovers.ResourceSystem`: 61/61 성공
- PIE 통합 검증: Native Glyph의 Family·Spectrum/Grade·Energy·State·Tag·Imprint 토큰 생성, Guidance Glyph Host, Build Dock Family Glyph, Inspector 및 프로젝트 HUD 회귀 성공

## 단계 6 결과

- Build Dock의 `NEXT`는 모든 고효율 설비에 붙는 점수 배지가 아니다. 현재 첫 연료 Milestone이 실제로 요구하는 건설 역할에 대해 선택 가능한 설비 하나만 결정적으로 추천한다.
- 추천 후보 선택을 `FSRStructureBuildDockModel` 하나로 통합했다. Guidance의 직접 Action과 Build Dock 카드가 같은 함수로 정확히 같은 설비를 선택하므로, 안내 버튼을 누른 뒤 다른 카드가 추천되는 불일치가 없다.
- 추천은 건설 행동이 필요한 `채굴기`, 해당 첫 Card의 `Family 가공기`, `항성 연료 제작기`, `Hub` 단계에서만 나타난다. 생산을 기다리거나 발사·도착을 확인하는 단계에는 불필요한 건설 추천을 만들지 않는다.
- 추천 카드에는 `NEXT n/9`와 한 줄 목적만 표시한다. 선택 상태는 추천보다 우선하며, 해금되지 않은 설비는 추천 후보가 될 수 없다.
- Build Catalog가 Facility V2의 Operation, Process Role, Archetype, Family Action, Energy Delta, Tag/Imprint 기본 Payload, Synthesis Role을 UI용 불변 Snapshot으로 보관한다. 상세 패널은 매 Tick Data Asset을 다시 읽지 않는다.
- 상세 패널에 `입력 > 처리 > 출력` Diagram을 추가했다. 채굴, 운송, Family 가공, Tag 부착·제거, Fuel Imprint, 5장 항성 연료 합성, Industrial Supply, Service Core, Fleet Berth, Hub의 서로 다른 계약을 같은 세 노드 문법으로 읽을 수 있다.
- 일반 가공의 Energy 변화는 `ENERGY +N/-N` 합연산으로만 표시한다. `B x C`는 항성 연료 제작기 Diagram에만 표시해 중간 가공에서 곱연산이 일어난다는 오해를 막았다.
- Family State 결과는 입력 Card의 현재 State와 숨은 가공 이력에 따라 달라지므로 확정 출력처럼 표시하지 않는다. Diagram 표면에는 설비 자체의 고정 Action만 표시하고, 실제 State·Tag 결과는 입력과 이력에 의존한다는 경계를 Tooltip에 둔다.
- 배치 영역을 장문 두 줄에서 `TARGET`, `SIZE`, `CAP` 세 지표로 바꿨다. 선택 전에는 선택 필요 여부·기본 Footprint·추가 Load를, 선택 후에는 실제 커서 지점의 설치 가능/교체/지형·점유 차단, 회전된 Footprint, `현재 > 예상 / 총 Capacity`를 보여 준다.
- 배치 지표는 별도 근사 규칙을 계산하지 않는다. 월드 Ghost와 실제 건설 판정이 공유하는 `FSRAssemblyStructurePlacementPreviewEvaluator` 결과를 그대로 Presentation으로 변환한다.
- 기존 Blueprint 기반 Build Dock에 새 Named Widget이 없으면 Native Tree로 안전하게 승격되며, 5장 Paging·Family 탭·잠금 표시·클릭 입력 계약은 유지된다.

현재 경계:

- Diagram은 설비를 놓기 전의 정적 계약을 보여 준다. 실제 입력 Card를 넣었을 때의 정확한 Energy·State·Tag 전이는 Facility Inspector의 런타임 Preview가 계속 담당한다.
- 이번 추천은 첫 항성 연료 완성이라는 명시적 Run 목표만 다룬다. 첫 연료 이후의 병목·행성 간 물류 최적 설비 추천은 단계 8의 전략 Overlay에서 실제 Network 상태를 근거로 확장한다.

검증 결과:

- `StarRoversEditor Win64 Development`: 성공
- `StarRovers.UI`: 38/38 성공
- `StarRovers.ResourceSystem`: 61/61 성공
- PIE 통합 검증: 새 추천 Row, 입력·처리·출력 노드, Target·Footprint·Capacity 지표, 기존 Blueprint 자동 승격, Guidance와 Build Dock의 정확한 추천 설비 일치 성공

## 단계 7 결과

- Augment 선택 화면에서 긴 Description 문단을 제거하고 `Offer Role`, `Rarity`, `READY n/n`, `Strategy` 배지를 첫 시선 계층으로 올렸다. 모든 설명은 카드 Tooltip에 남아 있지만 기본 화면에서는 선택 판단을 방해하지 않는다.
- `READY n/n`은 임의 효율 점수가 아니다. 해당 Package에 작성된 Family, Spectrum, Grade, 선행 Package, Technology Facility, Hub 수, Doctrine Slot 조건 그룹 중 현재 충족한 수를 뜻한다.
- `FSRAugmentPackageEligibilityReportV2`를 Simulation의 단일 판정 결과로 추가했다. Offer 필터, 실제 선택 가능 여부, UI의 준비 근거가 같은 결과를 사용하므로 UI만 `READY`라고 표시하는 불일치가 없다.
- 모든 Package가 `WHEN > RESULT` 행을 제공한다. 예를 들어 State Resonator는 `POSITIVE STATE ACTIVATES > OVERTONE | E +5`, Deep-Space Tempering은 `METAL IN CONDITIONED HOLD > COLD | E +3`으로 읽힌다.
- Process Tag는 `TAG RECIPE`, Fuel Imprint는 `FUEL RECIPE`, 물류 효과는 `ROUTE MODULE`로 명시했다. 따라서 Augment 선택 즉시 자원에 Tag가 붙거나 Energy가 수동 증가하는 것이 아니라, 해당 설비·Route에서 사용할 Recipe 또는 Module이 해방된다는 경계를 카드에서 바로 확인할 수 있다.
- Full-House, Prismatic, 세 Macro Doctrine은 최종 Batch 조건과 `FINAL B` 또는 `FINAL C` 효과를 표시한다. 중간 가공에서는 곱연산이 없고 최종 항성 연료 제작기에서만 평가된다는 상세 설명은 Tooltip에 유지한다.
- Line 예시는 장문 Impact 설명 대신 `LINE SHAPE` 한 줄로 축약했다. 주의점도 `TRIGGERED RECIPE, NOT PASSIVE`, `FINAL FABRICATOR ONLY`, `REQUIRES DOCK DWELL`, `USES THE ONE DOCTRINE SLOT` 중 하나로 먼저 보이고 전체 예외는 Tooltip에서 확인한다.
- Package가 표시된 뒤 Run Context가 바뀌면 카드를 `CONTEXT CHANGED` 또는 `ALREADY OWNED`로 비활성화한다. 키보드·게임패드 좌우 탐색은 선택 불가능한 카드를 건너뛰며, Simulation은 선택 시 다시 같은 권위 판정을 수행한다.
- Header는 Core Facility가 무작위 Augment에 잠기지 않는다는 점과 이번 선택이 하나의 Line Package를 고르는 결정임을 짧은 배지로 구분한다.

현재 경계:

- 현재 Offer 생성기는 선택 가능한 Package만 제시하므로 정상 선택 시 `READY`가 기본이다. `n/n`은 Package 간 우열이 아니라 필요한 Run 기반이 무엇인지 보여 주는 증거이며, 근거 없는 “효율 83점” 같은 상대 점수는 만들지 않았다.
- 현재 적합 근거는 Package의 정적 선행 조건까지 다룬다. 실제 가동 중 Network의 병목, 천체별 생산 여유, Route 정체에 따른 동적 추천은 단계 8의 전략 Overlay가 담당한다.
- Fuel Imprint의 정확한 수치는 항성 연료 제작기 Asset에서 조정 가능하므로 카드 표면은 `FINAL B/C` 역할을 약속하고, 작성 값과 실제 Batch 결과는 Fabricator Preview가 권위 있게 표시한다.

검증 결과:

- `StarRoversEditor Win64 Development`: 성공
- `StarRovers.UI`: 39/39 성공
- `StarRovers.ResourceSystem`: 62/62 성공
- PIE 통합 검증: 실제 프로젝트 Augment Modal의 Fit·Strategy 배지, 조건 카드, 효과 카드, Line Shape, Watch, 기본 Focus 및 좌우 탐색 생성 성공

## 단계 8 결과

- 천체별 Operational Capacity와 Hub·Route·Fleet 상태를 읽기 전용 `FSRStrategicOverlayPresentation`으로 통합했다. 목록, 월드 Nameplate, Route 선, 상단 요약과 Focus Action이 모두 같은 Snapshot을 사용한다.
- 병목 우선순위는 `막힌 Route > Operational Capacity 초과 > 실제 Throttle > Fleet 출발 Queue > Fleet 포화 > Capacity 포화 > Capacity 80% 위험`으로 고정했다. 표시 순서만 정하며 시설, 우선순위, Route 또는 Simulation 상태를 자동 변경하지 않는다.
- Overview 상단에 `NETWORK NOMINAL` 또는 `BOTTLENECK | 천체 | 상태` 배지를 추가했다. 문제가 있으면 가장 긴급한 천체 하나만 `FOCUS` 버튼으로 제시하므로 플레이어가 여러 경고를 읽고 목적지를 다시 찾을 필요가 없다.
- 천체 목록의 오른쪽 열은 최초 System Scan, 전략 경고, 현재 Load를 `SCAN ... | ! ROUTE 1 | L 24/30`처럼 짧은 토큰으로 결합한다. 계산 근거와 시설·물류 상세는 Tooltip에 남겼다.
- 월드 Nameplate는 전략 Overlay가 켜진 동안 천체명 옆에 `! ROUTE`, `! LOAD`, `! SLOW`, `Q`, `CAP` 문제 토큰을 붙이고 같은 위험 팔레트로 Leader Line을 강조한다.
- 실제 Hub Route의 출발·도착 천체를 월드 화면에 방향 선으로 투영한다. 비활성 Route는 흐린 회색, 정상 이동은 긍정색, Conditioning은 정보색, Fleet Queue는 경고색, Blocked는 가장 굵은 위험색으로 구분한다.
- 같은 두 천체 사이에 Route가 겹치더라도 정상 선을 먼저 그리고 Warning·Danger 선을 마지막에 그려 실제 병목이 가려지지 않게 했다.
- `NAMES`와 `ROUTES` 토글을 분리했다. 천체 라벨을 끄고 Route Topology만 보거나, Route 선을 끄고 목록의 병목 요약만 사용하는 것이 가능하다.
- 기존 Blueprint Overview에 새 Named Widget이 없으면 전체 Native Tree로 안전하게 승격한다. 따라서 기존 프로젝트 Asset을 수동으로 다시 작성하지 않아도 전략 Header와 토글이 PIE에 생성된다.

현재 경계:

- Overlay는 현재 확정된 Load, Throttle, Fleet Queue, Route Phase만 표시한다. 아직 발생하지 않은 생산량 증가나 미래 Route 정체를 추정 점수로 경고하지 않는다.
- `WaitingForCargo`는 고장이 아니라 정상 대기이므로 병목으로 표시하지 않는다. 비활성 Route도 구성된 Topology로는 보이지만 위험으로 집계하지 않는다.
- 하나의 천체에는 가장 높은 우선순위 문제 하나만 표면에 표시한다. 나머지 Capacity·시설·Hub·Fleet 근거는 Tooltip에 유지해 경고 토큰의 중복을 막는다.
- Route 선은 지휘용 연결도이며 실제 우주선의 비행 궤도나 도착 시간 예측선이 아니다. 정확한 Cargo, Phase, ETA 조작은 기존 Hub Route 화면이 계속 담당한다.

검증 결과:

- `StarRoversEditor Win64 Development`: 성공
- `StarRovers.UI.StrategicOverlay`: 3/3 성공
- `StarRovers.UI`: 42/42 성공
- `StarRovers.ResourceSystem`: 62/62 성공
- PIE 통합 검증: 기존 Blueprint 자동 승격, 전략 상태·상세·Focus·Route 토글 생성, 기본 Route Overlay 활성화 및 기존 Project HUD 계약 회귀 성공

## 단계 9 결과

- 실제 HUD 배치를 다시 대조한 결과, 기존 Player Guidance의 고정 폭 820이 1280×720에서 왼쪽 Overview와 오른쪽 Focus/Operations 영역을 침범할 수 있었다. 이를 단순 축소가 아니라 세 HUD 영역을 분리하는 반응형 `Top Center Command Lane`으로 교체했다.
- Command Lane은 왼쪽 372, 오른쪽 408의 지휘 패널 영역을 먼저 예약하고, 상단 생존·주기 Rail의 높이와 12 여백 아래에서 시작한다. 1920×1080에서는 기존 820 폭을 유지하고, 1280×720에서는 500 폭 Compact 형태로 바뀐다.
- Compact Guidance는 `분류`, `현재 목표`, `핵심 자원 Glyph`, `직접 Action`을 유지하고 보조 설명 한 줄만 숨긴다. 전체 설명은 카드와 Action Tooltip에 남으므로 작은 화면에서도 정보가 소실되지 않는다.
- Celestial Overview의 왼쪽 안전 여백을 공통 24로 맞추고, 월드 Nameplate의 좌측 회피 영역을 실제 Overview 폭과 간격에 맞춰 380으로 조정했다.
- `FSRUILayoutPolicy`에 1920×1080, 1280×720, 640×360 Command Lane 계약을 추가했다. 1280×720은 양쪽 패널을 보존하며, 640×360은 의도적으로 `BelowReadableScale` 경계로 분류한다.
- NullRHI PIE에서는 Viewport 좌표가 0인 엔진 제약을 1280×720 검증 기준값으로 대체한다. 별도의 D3D12 PIE에서는 실제 Cached Geometry를 읽어 Guidance가 상단 Rail과 양쪽 패널을 침범하지 않는지 검증했다.

현재 경계:

- 640×360은 레이아웃 수학의 안전 실패 경계를 검증하기 위한 크기이며 지원 가독성 해상도가 아니다. 실제 승인 기준은 최소 1280×720이다.
- 자동화 테스트는 경계와 Widget 계약을 보장하지만, 최종 Font 크기, 한국어 장문 Localization, 다중 병목이 누적된 장기 Run의 시선 밀도는 수동 PIE와 Content QA에서 계속 조정한다.

최종 검증 결과:

- `StarRoversEditor Win64 Development`: 성공
- `StarRovers.UI`: 43/43 성공
- `StarRovers.ResourceSystem`: 62/62 성공
- D3D12 렌더링 PIE `StarRovers.UI.Integration.PIE.ProjectHUDContract`: 1/1 성공, 실제 Viewport Geometry 사용 확인

## 구현 원칙

- Foundation 기능은 항상 접근 가능해야 하며, 무작위 증강이 생존 필수 기능을 잠그면 안 된다.
- Debug 편의 기능은 출시 기본값과 분리한다.
- 같은 의미는 컨베이어, Inspector, Build Dock, Fabricator, Augment에서 같은 Glyph로 표현한다.
- 화면 기본 계층에는 결과와 다음 행동만 표시하고, 계산 근거와 서술은 Tooltip과 Codex로 내린다.
- 색상만으로 Family, 위험, 상태를 구분하지 않는다.
- Guidance는 구조물 개수가 아니라 실제 자원 흐름 Milestone을 추적한다.

## 검증 기준

- 새 PIE Run은 플레이어 입력 전 일시정지 상태다.
- 정지 중 항성 연료, 가공, 물류, 주기 진행이 멈춘다.
- Play/Fast Forward 입력은 기존 방식대로 명시적으로 Simulation을 시작한다.
- 기본 Build Dock은 Debug 전체 해금 때문에 모든 조건부 설비를 노출하지 않는다.
- 각 단계는 `StarRovers.UI` 자동화 테스트와 SolarSystem PIE 회귀 검증을 통과해야 한다.
