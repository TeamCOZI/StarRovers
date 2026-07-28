# 항성 연료 수요·압력·회복 곡선 V2

## 목표

기존 항성 수요는 매 Cycle 약 2배씩 증가했다. 실제 새 PIE에서 공급이 없으면 361초에 패배했고, 마지막 수요는 3550.3/s였다. 이 곡선은 플레이어가 첫 Line을 이해하기 전에 Run을 끝내며, 실제 항성 연료 제작기 처리량인 약 82~118/s와도 비교할 수 없는 값이었다.

V2는 다음 경험을 목표로 한다.

1. Run 시작 정지 상태에서 계획한 뒤 약 10분의 무공급 실패선을 제공한다.
2. 기본 항성 연료 Line 한 개는 초·중반 생존을 회복하지만 혼자서는 승리하지 못한다.
3. 최적화 또는 Line 증설로 100/s를 넘으면 25~35분 목표 창에서 승리한다.
4. 초반에 연료를 무한히 비축해 후반 압력을 삭제할 수 없다.
5. Red Giant는 알아볼 수 있는 마지막 회복 기회이며 반복 진화로 무료 연료를 얻을 수 없다.

## 수요 곡선

기본 Cycle은 60초다.

| Cycle | Simulation 시간 | 수요 |
|---:|---:|---:|
| 0~2 | 00:00~03:00 | 50/s |
| 3 | 03:00~04:00 | 55/s |
| 4~11 | 04:00~12:00 | Cycle마다 +5/s |
| 12 이후 | 12:00 이후 | 100/s 고정 |

곡선은 `Grace`, `Expansion`, `Plateau` 세 단계로 노출된다. 수요는 절대 Cycle Index로 계산하므로 Save 복원이나 현재 상태 예측이 중간 시점에서 다시 50/s로 시작하지 않는다. 현재 Cycle의 남은 시간도 Harness에 전달되어 다음 수요 변경 시점이 한 Cycle씩 밀리는 오류를 막는다.

최종 상한 100/s는 Run 계약의 최종 요구 유입량 100/s와 같다. 따라서 HUD가 승리 가능한 처리량과 생존 가능한 처리량을 서로 다른 숫자로 안내하지 않는다.

## 생존 비축량과 압력

- 각 진화 단계의 생존 비축량 상한은 20,000이다.
- 새 Run은 Main Sequence 비축량 20,000에서 시작한다.
- `Pressure Ratio = 1 - Stored Reserve / Reserve Capacity`다.
- 연료 전달은 비축량을 회복하지만 20,000을 넘겨 저장하지 않는다.
- 비축량이 가득 찬 뒤 전달된 연료도 최근 유입량과 누적 Run 안정화 목표에는 전부 기여한다.
- Main Sequence 비축량이 0이 되면 Red Giant로 진화하며 설정된 비율만큼 비상 비축량을 한 번 받는다. 기본 비율은 100%, 즉 20,000이다.
- Red Giant에서 공급 흑자를 만들면 비축량과 압력은 다시 회복된다.
- Red Giant는 Main Sequence로 되돌아가지 않는다. 다시 비축량이 0이 되면 Supernova 패배다.

진화 단계를 되돌리지 않는 이유는 `Main Sequence 고갈 → 무료 비상 비축량 → 공급으로 잠깐 복귀 → 다시 고갈`을 반복하는 악용을 막기 위해서다. 정상적인 완전 회복은 최종 Run 계약 승리로 표현한다.

`LastFuelReserveGain`과 `LastFuelReserveOverflow`를 분리해 저장한다. 여기서 Overflow는 버려진 Energy가 아니라 생존 비축량 대신 누적 안정화에만 기여한 양이다. 항성 Focus 상세 정보는 이를 `Reserve Gain`과 `Stabilization`으로 구분해 보여 준다.

## 기준 Line Simulation

Harness의 공급 Stage는 전체 Facility Graph를 복제하지 않고 완성된 Line의 집계 처리량을 나타낸다.

### 무공급

- 공급: 0/s
- 패배: 618초, 10:18
- 패배 시 최대 수요: 90/s
- Main Sequence와 Red Giant 비축량을 모두 소모

### 기본 Line

- 기본 Full House 연료: 824 Energy
- 제작 주기: 10초
- 집계 처리량: 82.4/s
- 생산 시작: 05:00
- 첫 도착 지연: 30초
- 30분 시점: 생존, Red Giant, Run 진행 중
- 누적 전달량: 121,952
- 승리하지 못하는 이유: 누적 100,000은 넘지만 최종 요구 유입량 100/s에 미달

기본 Line은 실패를 즉시 복구하고 학습 시간을 제공하지만 최종 정답이 되지 않는다.

### 확장 Line

- 05:00부터 기본 82.4/s
- 25:00에 추가 35.6/s
- 최종 집계 처리량: 118/s
- 승리: 1559초, 25:59
- 승리 시 누적 전달량: 103,488

최적화된 기준 Batch 1180 Energy / 10초와 같은 처리량이며 목표 25~35분 안에서 완료한다.

## Runtime 권위와 표시

- `ASRStar`는 초 단위 소비, 비축량 상한, 진화 단계와 전달량을 소유한다.
- `FSRStellarDemandModel`은 Runtime, HUD, Telemetry, Harness가 공유하는 순수 계산 모델이다.
- HUD의 다음 Cycle 소비는 더 이상 별도 2배 공식을 사용하지 않는다.
- Telemetry는 Cycle 진행·다음 Cycle까지 남은 시간, 수요 단계, 압력 비율과 비축량 초과 안정화 기여량을 기록한다.
- Focus 상세 정보는 V2에서 Legacy의 `Next Multiplier` 중심 문자열 대신 Reserve, Pressure, Demand Phase, 현재·다음 수요를 표시한다.
- `sr.Balance.ProjectCurrent`는 현재 Cycle의 남은 시간과 절대 Cycle Index를 보존한다.

## 설정

`Project Settings > Star Rovers Simulation > Run Contract > Stellar Pressure`에서 조정한다.

- `bUseStellarPressureCurveV2=True`
- `StellarFuelReserveCapacityV2=20000`
- `StellarInitialDemandPerSecondV2=50`
- `StellarDemandGraceCycleCountV2=2`
- `StellarDemandIncreasePerCycleV2=5`
- `StellarMaximumDemandPerSecondV2=100`
- `StellarRedGiantEmergencyReserveRatioV2=1.0`

V2가 켜져 있으면 전역 Run 압력 설정이 Star Data Asset의 기존 초기 비축량·초기 감소량을 대체한다. Star Data Asset 필드는 Legacy 회귀와 향후 별 유형별 변형을 위해 제거하지 않는다. Save Schema 2는 구 Save의 Cycle 위치를 현재 V2 수요 곡선에 이관하고, 현재 authored 곡선을 권위 설정으로 유지한다. 세부 내용은 [ResourceV2RunSaveMigrationImplementation.md](ResourceV2RunSaveMigrationImplementation.md)를 참고한다.

## 검증 범위와 남은 경계

자동화 테스트는 수요 단계, 상한, 비축량 회복·상한, Red Giant 비상 비축량, 중간 Cycle 이어서 예측과 세 기준 Line을 검증한다. 실제 Project Map PIE는 시작 비축량 20,000, 초기 수요 50/s, 정지 상태, 무공급 618초 예측을 검증한다.

유한 광맥 단계에서 실제 Catalog의 Card 120개, 제작기 10초, 기본 824와 최적화 1180 Energy를 같은 Harness에 연결했다. 기본 광맥 세트 하나는 26:47에 패배한다. 원격 최적화 Line은 23:00에 생산을 시작하고 120초 운송 뒤 25:00에 첫 Batch가 도착하면 25:39에 승리한다.

Phase 22의 실제 여섯 환경 512 Seed Soak와 D3D12 Seed 재생 검증을 통과했으므로 이 값은 현재 Reference Content의 확정 기준선이다. 실제 25~35분 인간 Playtest에서의 건설 시간, Capacity 실수와 UI 피로도는 계속 Telemetry로 관찰하지만, Seed Coverage와 광맥 수명 때문에 수치를 임시값으로 남겨 두지는 않는다. 세부 근거는 [ResourceEconomySeedSoakImplementation.md](ResourceEconomySeedSoakImplementation.md)를 따른다.
