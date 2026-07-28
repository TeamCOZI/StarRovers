# Resource V2 Run Save와 Migration 구현

## 목표

이 단계는 유한 광맥 도입 뒤에도 Run의 의사결정 상태가 저장 전후로 달라지지 않게 만드는 단계다. 단순히 Inventory만 직렬화하지 않고, 자동화 Line을 다시 움직이는 데 필요한 권위 상태를 하나의 Checkpoint로 묶는다.

핵심 계약은 다음과 같다.

- 저장은 태양계 생성이 완료된 PIE/Game World에서만 허용한다.
- 현재 World 상태를 먼저 백업한 뒤 복원을 시도한다.
- 한 하위 System이라도 검증 또는 Commit에 실패하면 전체 Run 복원을 실패시키고 백업 상태로 Rollback한다.
- 화면용 Cache와 Telemetry는 저장하지 않고, 복원된 권위 상태에서 다시 계산한다.
- 같은 환경 이름의 행성이 여러 개 나올 수 있으므로 표시 이름을 영속 ID로 사용하지 않는다.

## 저장 경계

| 소유 System | 저장하는 권위 상태 | 복원 방식 |
|---|---|---|
| Structure Manager | 배치 구조물, Occupant ID, Data Asset, Cell Footprint, 회전, 자연 구조물 여부, 다음 ID Sequence | Grid 점유·HISM·Facility 등록을 실제 배치 경로로 재구축 |
| Resource Deposit | Resource ID, 유한/Legacy 무한 구분, 총량, 잔량 | 구조물과 Resource 정의가 일치한 뒤 정확한 잔량 적용 |
| Facility Network | Port Inventory, 처리 중 Inventory, Recipe 진행도와 Runtime 상태 | 구조물이 먼저 등록된 뒤 기존 Save Adapter로 복원 |
| Space Logistics | Hub Route, Fleet Queue, 운송 중 Cargo, Conditioning 진행도, 항성 연료 Missile | Hub Endpoint를 다시 찾은 뒤 Schema 5 상태 복원 |
| Augment | 해금 구조물, 선택 Package, 직전 Offer 기억, Macro Doctrine, Pity, 보류 중 선택지와 Pause 소유권 | Catalog ID를 검증하고 카드 Presentation 재구축 |
| Star | 비축 연료, 수요 Cycle 경계, 초 단위 Accumulator, 최근 전달 Window, 누적 전달량, 승패 진행 | 현재 V2 곡선을 권위 설정으로 사용하고 Runtime만 복원 |
| Time Control | Cycle Index, Cycle 내부 초, 선택 배속, Pause | 모든 경제 상태 복원 뒤 마지막에 적용 |
| Run Milestone | 최초 행동의 단조 증가 Fact, 첫 Family, 비상 재탐사 사용 여부·대상 | 파생 Scan과 행동 Target은 World에서 다시 계산 |
| Run Telemetry | 저장하지 않음 | Load 완료 뒤 Session Reset 후 즉시 Snapshot 1회 생성 |

최상위 DTO는 `FSRResourceV2RunSaveData` Schema 2다. Unreal `USaveGame`에는 중첩 Struct를 직접 흩어 넣지 않고, 이름과 Object Reference를 문자열로 직렬화한 Binary Payload와 CRC32를 저장한다. Payload 한 Byte라도 바뀌면 Import 전에 거부한다.

## 광맥 Migration 규칙

Structure Save Schema 2부터 `Finite`와 `LegacyInfinite`를 명시적으로 구분한다. `MAX_int32`는 Runtime Sentinel로만 남고, 새 Save의 의미를 추론하는 유일한 근거로 사용하지 않는다.

### Schema 1의 50,000 Placeholder

구 Save의 Resource V2 광맥 총량이 50,000 이상이면 현재 authored 총량으로 바꾸되, 채굴된 비율은 보존한다.

~~~text
새 잔량 = round((구 잔량 / 구 총량) * 현재 authored 총량)
~~~

예시는 다음과 같다.

| 구 Save | 현재 Catalog | 복원 결과 |
|---:|---:|---:|
| 50,000 / 50,000 | Card 120 | 120 / 120 |
| 25,000 / 50,000 | Card 120 | 60 / 120 |
| 0 / 50,000 | Utility 180 | 0 / 180 |

이미 고갈된 광맥을 다시 채우지 않으며, 과거 플레이에서 소비한 비율도 잃지 않는다.

### Schema 1의 암묵적 무한 광맥

- Resource V2 광맥이면 현재 Catalog의 120 또는 180으로 한 번만 유한화한다.
- 진짜 Legacy Resource이면 `LegacyInfinite`와 `MAX_int32`를 유지한다.
- 현재 Schema 2의 비상 재탐사 광맥 `25/x` 같은 작은 특수 총량은 Catalog 값으로 정규화하지 않고 정확히 보존한다.
- `잔량 < 0`, `잔량 > 총량`, 누락·중복 Occupant, Structure/Resource ID 불일치는 전체 Import 실패다.

## 원자적 복원 순서

~~~text
현재 Run 백업 Capture
  -> Parent/Child Schema 및 천체 Topology 사전 검증
  -> Simulation Pause
  -> Augment 진행 상태
  -> 구조물·광맥
  -> Facility Network
  -> Hub Endpoint Refresh
  -> Star
  -> Space Logistics
  -> Milestone
  -> Time Control과 원래 Pause/배속
  -> 파생 Scan·Telemetry 재구축
~~~

구조물 Import 자체도 Body 단위 Backup을 가진다. 배치 중 하나가 실패하면 해당 Body를 먼저 되돌리고, 최상위 Coordinator도 전체 Run Checkpoint로 다시 되돌린다. 실패 보고에는 전체 Rollback 성공 여부와 Migration된 광맥 수가 포함된다.

## 천체 식별과 현재 경계

`Bad Lands`처럼 같은 표시 이름을 가진 행성이 둘 이상 생성될 수 있다. 따라서 최상위 저장은 표시 이름 대신 생성된 Actor의 고유 `FName`을 Body ID로 사용한다. 현재 Load는 저장 당시와 같은 결정적 태양계 생성 Topology가 먼저 존재해야 하며, 다음 조건에서는 의도적으로 거부한다.

- 태양계가 아직 생성 중임
- 저장된 천체 수와 현재 천체 수가 다름
- 저장 Body ID 또는 Primary Star ID를 현재 World에서 찾지 못함
- 필요한 Surface Grid, Structure Manager, Facility Network가 사라짐

Generator C++와 Blueprint Class 기본값은 고정 Seed지만, 현재 `SolarSystem` Map Instance는 Roguelike Run 다양성을 위해 `bRandomizeGenerationSeedEachRun=True`다. Phase 22부터 실제 Runtime Root Seed를 `GetLastRuntimeGenerationSeed`로 확인할 수 있고, `GenerateRuntimeSystemForSeed`로 같은 행성·지형·Resource Cell을 재생할 수 있다. 모든 하위 천체 Seed도 Root Stream에서 파생되며 D3D12 PIE의 Seed A → B → A 검증을 통과했다.

다만 현재 Save DTO는 아직 Root Seed를 Payload에 기록하거나 Load 전에 Generator를 Bootstrap하지 않는다. 따라서 Phase 21이 보장하는 범위는 이미 생성된 같은 World 안의 원자적 Checkpoint와 Restore이며, 서로 다른 실행 간 Slot Load는 여전히 의도적으로 고정-Topology를 요구한다. 다음 Save 단계에서는 Root Seed 저장 → Load 전 System 재생 → Body ID 매핑 → 현재 원자적 Restore 순서로 연결해야 한다. 현재 코드는 잘못된 천체에 Line을 붙이는 추측 복원 대신 Body Topology 불일치로 명시적으로 실패한다.

## 호출 경로

`USRResourceV2RunSaveSubsystem`은 Blueprint에서도 다음 API를 제공한다.

- `CaptureRunState` / `RestoreRunState`: 메모리 Checkpoint와 테스트용
- `SaveRunToSlot` / `LoadRunFromSlot`: Unreal SaveGame Slot용

아직 전용 Save Slot 선택 UI나 Auto-save 주기는 추가하지 않았다. UI가 붙더라도 복원 정책은 이 Coordinator 하나만 사용해야 한다.

## 자동 검증

Phase 21은 다음 경계를 자동화한다.

- 50,000의 50% 채굴 상태가 120의 50%인 60으로 이관됨
- 구 Resource V2 무한 광맥은 유한화되고 진짜 Legacy 무한 광맥은 유지됨
- 비상 광맥 `25/7`은 정확히 보존됨
- Payload CRC 손상이 Import 전에 거부됨
- Augment의 직전 Offer, 보류 카드, Pity와 Pause가 왕복됨
- Cycle Index, 부분 진행도, 배속과 Pause가 왕복됨
- 실제 D3D12 SolarSystem PIE에서 Run Capture 후 광맥 잔량과 배속을 변조하고 Restore하여 원래 상태로 되돌아옴
- 같은 PIE에서 구조물·광맥 수와 복원 직후 Telemetry Snapshot이 유효함

2026-07-28 검증 결과:

- `StarRoversEditor Win64 Development`: 성공
- `StarRovers.ResourceSystem.Phase21`: 4/4 성공
- `StarRovers.ResourceSystem`: 82/82 성공
- `StarRovers.UI`: 63/63 성공
- D3D12 `StarRovers.ResourceSystem.PIE.SolarSystemBaseline`: 성공
- 최근 렌더링 PIE의 실제 Payload: 약 1.63 MB, `Bodies=9`, `Checks=28`, `Failures=0`

Payload 크기는 생성 천체와 자연 구조물 수, 배치 Line에 따라 달라진다. 약 1.63 MB는 상한이 아니라 현재 빈 시작 태양계 한 Seed의 관측값이다.
