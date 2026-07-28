# Star Rovers UI 개편 구현 기준

## 1. 문서 목적

이 문서는 Resource V2, Family 중심 건설, Operational Capacity, 다천체 물류, Augment 선택을 하나의 플레이 흐름으로 보여주는 Runtime UI의 최종 구현 계약이다.

UI는 Gameplay 상태를 소유하지 않는다. 각 Widget은 Runtime Source of Truth를 조회해 표시하거나, PlayerController와 Subsystem에 사용자의 요청을 전달한다.

## 2. 정보 구조

플레이어가 보는 정보는 다음 세 단계로 나뉜다.

1. 전역 감시: 어떤 천체나 물류망에 문제가 있는지 발견한다.
2. 지역 진단: 선택한 천체 또는 설비에서 원인과 다음 행동을 확인한다.
3. 실행과 결정: Build Dock, Facility Inspector, Hub, Augment UI에서 실제 선택을 수행한다.

| 화면 | 주 역할 | 핵심 정보 |
|---|---|---|
| Celestial Overview | 전역 천체 감시와 이동 | 천체 계층, `Load / Capacity`, 상태 색상 |
| Focus Info / Operations | 선택 천체 진단 | Capacity 출처, Priority별 Demand와 Speed, Facility·Hub·Fleet 현황 |
| Player Guidance | 현재 가장 중요한 문제 하나 | 문제, 이유, 다음 행동 |
| Build Dock | Family 중심 건설 | Family 탭, 역할, 해금 상태, 처리 시간과 Load, 배치 Preview |
| Facility Inspector | 지역 가공 진단과 제어 | Input → Process → Output, Energy·State 변화, Recipe, Priority |
| Hub UI | 다천체 물류와 연료 출하 | Fleet Load, Queue, Route Profile, Cargo, Conditioned Module, Missile |
| Augment Choice | Run 분기 결정 | 즉시 이득, 적합성, Line 영향, 위험, 실제 해금 내용 |
| Game Over | 최종 모달 | 패배 원인과 최종 항성 상태 |

## 3. 대표 플레이 흐름

```text
Celestial Overview
  -> 병목 천체 선택
  -> Focus Info / Operations에서 원인 확인
  -> Build Dock에서 Family와 역할 선택
  -> 지표면 배치 Preview 확인 후 건설
  -> Facility Inspector에서 Recipe / Priority / State 전이 확인
  -> Hub에서 Route / Fleet / Fuel 출하 구성
  -> Player Guidance로 다음 전역 병목 확인
```

Augment가 발생하면 이 흐름 위에 전면 결정 모달이 열린다.

```text
현재 자동화 Context Snapshot
  -> Augment Package 후보 비교
  -> 하나 선택
  -> Build Catalog / Recipe / Route Module 갱신
  -> 게임 Viewport로 입력 Focus 복귀
  -> Guidance가 변경 결과를 일시 알림
```

## 4. 레이어와 모달 계약

기본 레이어는 아래에서 위 순서다.

1. Focus Info
2. Celestial Overview
3. Time Control과 Player Guidance
4. Build Dock
5. Hub Shortcut
6. Facility Inspector
7. Augment Choice
8. Game Over

`WidgetLayerOrder`는 일반 Gameplay 패널의 상대 순서를 조정할 수 있다. 다만 다음 두 규칙은 Blueprint에 예전 배열이 저장되어 있어도 강제로 유지된다.

- Augment Choice는 모든 일반 Gameplay 패널보다 위에 있다.
- Game Over는 모든 UI보다 위에 있다.

Augment가 보이는 동안 Build Dock 단축키, 월드 선택, 복사·삭제·회전 같은 Gameplay 입력은 처리하지 않는다. 선택을 완료하면 게임 Viewport로 Focus를 돌려준다. Game Over는 UMG Mouse Event뿐 아니라 Enhanced Input에서 들어오는 월드 클릭 경로도 차단한다.

## 5. 해상도와 가독성 계약

고정 디자인 크기가 필요한 결정·진단 화면은 `FSRUILayoutPolicy`와 `ScaleBox`를 사용한다.

- 안전 여백: 논리 Viewport 기준 24
- 큰 화면: 디자인 크기 이상으로 불필요하게 확대하지 않는다.
- 작은 화면: 종횡비를 유지한 채 전체 패널을 안전 영역 안으로 축소한다.
- 적용 화면: Augment Choice, Facility Inspector, Player Guidance, Game Over 본문
- 65% 미만 축소는 자동 계산에서 `BelowReadableScale`로 분류한다. 이는 극소 해상도 QA 경고 기준이지 Gameplay 실패 조건이 아니다.
- Player Guidance는 별도의 `Top Center Command Lane`을 사용한다. 왼쪽 Overview 372, 오른쪽 Focus/Operations 408을 먼저 예약하고 상단 생존·주기 Rail 아래에 배치한다.
- 1920×1080에서는 Guidance의 820 디자인 폭과 전체 설명을 유지한다. 1280×720에서는 500 폭 Compact 형태로 전환해 분류·목표·Resource Glyph·Action은 남기고 보조 설명만 숨긴다. 숨긴 설명은 Tooltip에서 계속 읽을 수 있다.
- 640×360처럼 양쪽 지휘 패널을 동시에 보존할 수 없는 크기는 `BelowReadableScale`로 명시해, 조용한 겹침이나 임의 잘림을 정상 레이아웃으로 취급하지 않는다.

자동 검증 기준 Viewport는 1920×1080, 1280×720, 640×360의 논리 크기다. 실제 시각 QA는 최소 1920×1080과 1280×720에서 수행한다.

## 6. 대규모 목록 계약

Build Dock은 한 페이지에 설비 카드 5개를 보여준다.

- 현재 첫 연료 건설 목표와 정확히 일치하는 선택 가능한 카드 하나만 `NEXT n/9`로 강조한다.
- 상세 패널은 `입력 > 처리 > 출력` 정적 계약과 `TARGET / SIZE / CAP` 실제 배치 지표를 함께 보여 준다.
- 일반 가공은 Energy 합연산만 표시하며 최종 `B x C`는 항성 연료 제작기에만 표시한다.
- 설비 수와 관계없이 페이지 점은 최대 7개만 만든다.
- 점은 현재 페이지 주변을 따라 움직이는 Window다.
- `PAGE 현재 / 전체`를 항상 함께 표시한다.
- 다음과 이전 페이지 이동을 모두 제공한다.
- 60개 설비, 12페이지 PIE 시나리오와 1,000개 항목 상당의 200페이지 계산을 자동 검증한다.

Facility Inspector의 Inventory와 Hub Route 목록은 ScrollBox를 사용한다. 항목 수가 늘어날 때 전체 Panel 크기를 늘리지 않는다.

## 7. Blueprint 호환 규칙

Runtime Source of Truth와 핵심 정보 계층은 C++ Native Widget Tree에 있다. 기존 Widget Blueprint가 개편 전 고정 크기 Root를 보존하고 있으면 다음 화면은 실행 시 완성된 반응형 Native Tree로 자동 승격한다.

- Augment Choice
- Family Build Dock
- Facility Inspector
- Player Guidance

따라서 기존 Blueprint를 일괄 수동 저장하지 않아도 Runtime 계약이 동일하다. 이후 Blueprint에서 외형을 다시 제작할 때는 Native Widget 이름과 데이터 흐름을 유지해야 한다.

## 8. 주요 구현 위치

| 책임 | 구현 |
|---|---|
| 공용 Theme과 상태 색상 | `Public/UI/SRUITheme.h`, `Private/UI/SRUITheme.cpp` |
| 재사용 Card / Badge / Metric | `Public/UI/SRUIComponents.h`, `Private/UI/SRUIComponents.cpp` |
| Viewport와 페이지 정책 | `Public/UI/SRUILayoutPolicy.h`, `Private/UI/SRUILayoutPolicy.cpp` |
| Guidance 반응형 Command Lane | `Public/UI/SRPlayerGuidanceWidget.h`, `Private/UI/SRPlayerGuidanceWidget.cpp` |
| HUD 생성·갱신·Augment 연결 | `Private/Camera/SRPlayerControllerUI.cpp` |
| 레이어 정규화 | `Private/Camera/SRPlayerControllerWidgetLayers.cpp` |
| Pointer 차단과 수동 라우팅 | `Private/Camera/SRPlayerControllerPointerUIRouter.cpp` |
| Family Build Dock | `Public/UI/SRStructureSelectionWidget.h`, `Private/UI/SRStructureSelectionWidget.cpp` |
| 천체 Operations | `Public/UI/SRCelestialBodyOperationsSummary.h`, `Private/UI/SRCelestialBodyOperationsSummary.cpp` |
| Facility Inspector | `Public/UI/SRFacilityControlWidget.h`, `Private/UI/SRFacilityControlWidget.cpp` |
| Hub Route 표현 | `Public/UI/SRHubRoutePresentation.h`, `Private/UI/SRHubRoutePresentation.cpp` |
| Augment 비교 | `Public/UI/SRAugmentChoicePresentation.h`, `Private/UI/SRAugmentChoicePresentation.cpp` |
| 적응형 Guidance | `Public/UI/SRPlayerGuidancePresentation.h`, `Private/UI/SRPlayerGuidancePresentation.cpp` |
| 전략 병목과 Route Overlay | `Public/UI/SRStrategicOverlayPresentation.h`, `Private/UI/SRStrategicOverlayPresentation.cpp` |

## 9. 자동 검증

Editor Target 빌드:

```powershell
& 'D:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat' `
  StarRoversEditor Win64 Development `
  'D:\Unreal Projects\StarRovers\StarRovers.uproject' -WaitMutex
```

UI 전체:

```powershell
& 'D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'D:\Unreal Projects\StarRovers\StarRovers.uproject' `
  -unattended -nop4 -nosplash -nullrhi -DDC-ForceMemoryCache `
  '-ExecCmds=Automation RunTests StarRovers.UI' `
  '-TestExit=Automation Test Queue Empty' -log
```

Resource System 회귀:

```powershell
& 'D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'D:\Unreal Projects\StarRovers\StarRovers.uproject' `
  -unattended -nop4 -nosplash -nullrhi -DDC-ForceMemoryCache `
  '-ExecCmds=Automation RunTests StarRovers.ResourceSystem' `
  '-TestExit=Automation Test Queue Empty' -log
```

2026-07-26 최종 기준:

- `StarRoversEditor Win64 Development`: 성공
- `StarRovers.UI`: 43/43
- `StarRovers.ResourceSystem`: 62/62
- NullRHI 통합 PIE: 실제 Project PlayerController의 HUD, Native Tree 승격, Family Build Catalog, Augment 모달, Guidance Command Lane, 전략 Overlay, Game Over 차단 통과
- D3D12 렌더링 PIE `ProjectHUDContract`: 1/1, 실제 Cached Geometry로 Guidance의 상단 Rail·좌우 패널 회피 확인

## 10. 수동 PIE 승인 체크리스트

자동 테스트는 구조와 상태 계약을 검증하지만 최종 가독성과 조작감은 사람이 확인한다.

### 10.1 1920×1080

- Overview에서 모든 천체의 Load Badge를 한눈에 읽을 수 있는가?
- Operations Card가 Capacity 출처와 가장 큰 병목을 설명하는가?
- Build Dock의 Family, 역할, 해금 상태가 색상만 보지 않아도 구분되는가?
- Facility Inspector의 Input → Process → Output 순서가 즉시 이해되는가?
- Hub Route가 많을 때 Scroll과 선택 대상이 명확한가?
- Augment 카드에서 실제 해금, Line 영향, 위험을 선택 전에 비교할 수 있는가?

### 10.2 1280×720

- Augment와 Facility Inspector의 가장자리나 버튼이 잘리지 않는가?
- Guidance가 Time Control, Overview, Focus Info의 핵심 정보를 가리지 않는가?
- Build Dock 페이지 수와 현재 페이지를 읽을 수 있는가?
- Game Over 본문 전체가 안전 영역 안에 있는가?

### 10.3 입력

- Mouse로 UI를 누를 때 뒤의 천체나 지표면 Cell이 선택되지 않는가?
- Augment가 열릴 때 첫 카드에 Focus가 가는가?
- 방향키, D-Pad, Shoulder로 Augment 카드를 이동하고 확인할 수 있는가?
- Augment 선택 후 Camera와 Build 입력이 즉시 복귀하는가?
- Game Over에서 월드 입력이 발생하지 않는가?

## 11. 완료 범위와 후속 작업

이번 개편에서 정보 구조, Runtime 연결, Run 지휘 레이어, 전략 Overlay, 반응형 Command Lane, 입력 차단, 대규모 목록, 자동화 검증은 완료됐다.

다음 항목은 기능 결함이 아니라 별도 Content 또는 QA 작업이다.

- 최종 Family별 Icon과 Facility 고유 외형
- 전체 한국어 Localization과 Font 자산 확정
- 색각 이상 Palette와 Screen Reader 수준의 Accessibility Pass
- 실제 Gamepad 기기별 Navigation 감도와 Focus 강조 연출
- 다중 행성 장기 플레이에서 정보 밀도와 경고 빈도 조정
