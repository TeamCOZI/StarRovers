# Run Balance Telemetry 및 결정론적 Simulation Harness

## 목적

이 단계는 당시 밸런스 값을 바꾸지 않고 기준선을 만든 단계다. 실제 PIE Run에서 생산·Capacity·물류·항성 상태를 같은 기준으로 관측하고, 특정 공급 가정을 반복 실행해 이후 밸런스 변경 전후를 비교할 수 있게 한다. 이후 적용된 압력 곡선 V2 결과는 `StellarPressureCurveImplementation.md`를 따른다.

Runtime의 권위 있는 게임 상태와 예측 모델은 분리한다.

- `USRRunTelemetrySubsystem`은 실제 World를 읽기만 한다.
- `FSRRunBalanceSimulator`는 World, Frame Rate, 난수에 의존하지 않는 1초 단위 순수 모델이다.
- 항성의 기존 수요 증가식은 `FSRStellarDemandModel` 하나를 Runtime과 Harness가 함께 사용한다.
- Telemetry와 Harness는 Facility, Resource, 물류, 항성 상태를 변경하지 않는다.

## Runtime Telemetry

기본값은 Simulation 시간 5초마다 최대 720개 Snapshot을 메모리에 보관한다. 정상 간격 기준으로 최근 60분이며, 오래된 Snapshot부터 제거된다. 일시정지 중에는 Simulation 시간이 증가하지 않으므로 주기 Sample도 쌓이지 않는다. 같은 Simulation 시각에 강제로 다시 측정하면 기존 마지막 Snapshot을 갱신한다.

저장하는 주요 정보는 다음과 같다.

- Run: 경과 Simulation 시간, Cycle, 일시정지, 단계, 승패, 누적 전달량, 목표 진행도
- 항성: 진화 단계, 저장 연료, 현재 소비량, 최근 30초 유입량, 순수지
- 천체·설비: 천체 수, 건설 가능 천체 수, Network와 설비 수, 처리 중·효율 저하 설비 수
- Capacity: 전체 수요, 전체 공급, 활성 Service Core 수
- 물류: 활성·차단·Fleet 대기 Route 수, 비행 중 항성 연료 Missile 수와 연료량
- 생산: 생산된 Card 수, 항성 연료 수, 생산 Energy 합계
- 자원: 전역 유한 광맥 총량·잔량, 완성 가능한 5장 Batch 수, 제한 Card ID, Reserve 압력
- 이정표: 첫 Card 생산, 첫 항성 연료 생산·전달, 각 Run 단계 완료, 최종 완료 시각

요약 모델은 최소 항성 연료, 최대 소비·유입, 시간 가중 평균 유입, Capacity 최대 사용률, 최대 효율 저하 설비 수, 최대 물류 대기 수를 계산한다. 가장 우선적인 병목은 다음 순서로 하나를 선택한다.

1. Primary Star 없음
2. 항성 붕괴
3. 최종 안정화 단계의 처리량 부족
4. 완성 Batch 부재 또는 Critical/Depleted 자원 압력
5. Operational Capacity 초과 또는 효율 저하
6. 항성 연료 미생산
7. 생산했지만 전달되지 않음
8. 항성 연료 순수지 적자

이 결과는 원인 확정기가 아니라 조사 우선순위다. 예를 들어 `Operational Capacity`가 선택되어도 배치 주기, 입력 고갈, 배치 경로 문제가 함께 존재할 수 있다.

### 설정

`Project Settings > Star Rovers Simulation` 또는 `DefaultGame.ini`에서 조정한다.

- `bEnableRunTelemetryV2=True`
- `RunTelemetrySampleIntervalSecondsV2=5.0`
- `RunTelemetryMaxSamplesV2=720`
- `bLogRunTelemetryOnCompletionV2=True`

Telemetry는 SaveGame에 영구 저장하지 않는다. 권위 상태가 아니라 복원된 World의 진단 Cache이므로 Run Load 직후 Session을 비우고 현재 Snapshot을 다시 만든다. 한 PIE/Game World의 최근 관측 창만 유지한다.

장시간 Seed 비교용 내보내기는 Phase 22의 `SRRunResourceEconomySoak` Commandlet으로 분리했다. 실제 Planet DA를 사용하는 Seed별 CSV는 `Saved/Balance/ResourceV2SeedSoak.csv`에 기록되며, 환경 다양성·Resource Coverage·보장/잠재 광맥과 Run 결과를 같은 입력으로 반복 비교할 수 있다. 세부 계약은 [ResourceEconomySeedSoakImplementation.md](ResourceEconomySeedSoakImplementation.md)를 따른다.

## 결정론적 Balance Harness

`FSRRunBalanceScenario`는 다음 공급 조건을 입력으로 받는다.

- 시뮬레이션 길이와 출력 Sample 간격
- 항성 시작 저장량·단계별 초기량·초기 소비량·Cycle 길이
- Run 계약 및 이어서 예측할 진행 상태
- 공급 Stage별 시작·종료 시각, 초당 생산량, 배치 간격, 운송 지연
- Capacity 또는 의도한 가동률을 나타내는 `OperationalSpeedFactor` (`0.0~1.0`)
- 공급원이 정확히 몇 Batch 뒤 고갈되는지 나타내는 `MaximumDeliveryCount` (`0`은 무제한)
- Legacy 증가 수요, 고정 수요 또는 항성 압력 V2 곡선

Harness는 결과로 승패, 단계 전환·목표 완료 시각, 총·평균 전달량, 공급 Batch 수·첫 고갈 시각, 최소 저장 연료, 최대 소비량과 제한된 Timeline을 반환한다. 동일 입력은 항상 동일 결과를 만든다. 비정상 값은 유한 범위로 정규화하며 한 번의 시뮬레이션은 최대 24시간으로 제한한다.

이 모델은 전체 Facility Graph를 복제하지 않는다. `SupplyStage`는 실제 Line 또는 설계 중인 Line의 집계 처리량을 뜻한다. 따라서 Recipe, Family State, Tag가 실제로 그 처리량을 만들 수 있는지는 Runtime Telemetry와 ResourceSystem 테스트로 따로 검증해야 한다. 이 경계를 유지해야 게임 로직과 다른 두 번째 자동화 Simulator가 생기지 않는다.

## PIE에서 사용하는 명령

- `sr.Balance.Telemetry.Report`: 현재 Snapshot을 추가하고 누적 요약을 Log에 출력
- `sr.Balance.Telemetry.Reset`: 게임 상태는 건드리지 않고 현재 Telemetry Session만 초기화
- `sr.Balance.ProjectCurrent`: 현재 최근 유입량이 계속 유지된다고 가정해 Run을 앞으로 예측

`ProjectCurrent`는 미래 건설, Augment, Route 증설을 추정하지 않는다. 지금의 30초 관측 유입량을 평평하게 유지하는 “아무것도 바꾸지 않으면” 예측이다.

## 2단계 당시 Legacy 기준선

실제 프로젝트 Map을 새 PIE World로 초기화한 통합 테스트에서, 최근 유입량 `0/s`를 그대로 유지하면 다음 결과가 나온다.

- 결과: 패배
- 완료 시각: 361초
- 전달 연료: 0
- 최대 항성 소비량: 3550.3/s

이는 압력 곡선 개편 전의 비교 기준이다. 3단계 V2 적용 후 동일한 실제 PIE 예측은 공급 0/s에서 618초 패배, 최대 수요 90/s로 변경되었다.

## 자동 검증

- `StarRoversEditor Win64 Development`: 성공
- `StarRovers.UI`: 58/58 성공
- `StarRovers.ResourceSystem`: 62/62 성공
- 실제 프로젝트 Map의 `StarRovers.UI.Integration.PIE.ProjectHUDContract`: 성공
- Harness 검증: Runtime 수요식 일치, 반복 결정성, 승리, 무공급 붕괴, 운송 지연·배치·Capacity, 관측 유입 이어서 예측, 경계값 정규화
- Telemetry 검증: 요약·병목 우선순위, terminal 이정표, Console 명령 등록, PIE World Snapshot

## 다음 단계에서의 사용 원칙

항성 압력 곡선을 바꿀 때는 목표 플레이타임만 맞추지 않는다. 최소한 다음 세 시나리오를 같은 Harness로 비교한다.

1. 공급이 전혀 없는 시작 실패선
2. 첫 행성의 기본 Line만 운영하는 회복선
3. 중반 다중 Line과 후반 행성 간 물류가 추가되는 승리선

각 곡선은 첫 유효 행동까지의 여유, 첫 연료 도착 시점, Capacity 확장 시점, 최종 처리량 유지 시간을 함께 만족해야 한다. 실제 수치 변경 후에는 Harness 결과와 PIE Telemetry가 같은 방향으로 움직이는지도 확인한다.
