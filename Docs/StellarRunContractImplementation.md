# 항성 Run 계약 구현

## 목적

Resource V2의 자동화망이 단순히 항성 붕괴를 늦추는 무한 생존으로 끝나지 않도록, 한 Run의 명시적인 승리 계약을 정의한다. 목표 플레이타임은 25~35분이며 기본 Balance 기준은 30분이다.

항성 연료의 소비와 항성 진화는 생존 압력이고, Run 계약은 승리 진행도와 최종 결과를 판정한다. 두 규칙은 분리되어 있으므로 이후 항성 소비 곡선을 바꾸더라도 승리 조건을 독립적으로 조정할 수 있다.

## 세 단계

| 단계 | 기본 목표 | 플레이어가 증명하는 것 |
|---|---:|---|
| 비상 점화 | 누적 전달 5,000 | 첫 항성 연료 Line과 발사 물류를 완성했다. |
| 지속 공급 | 누적 전달 25,000 | 여러 Line을 운영해 생산 규모를 확장했다. |
| 최종 안정화 | 누적 전달 100,000, 최근 유입 100/s를 30초 유지 | 순간적인 비축 투입이 아니라 지속 가능한 최종 Network를 만들었다. |

승리는 `누적 전달량`, `최근 30초 실제 도착량을 기준으로 한 유입률`, `연속 유지 시간`을 모두 만족해야 한다. 유입률이 한 Tick이라도 기준 아래로 내려가면 유지 시간은 0으로 돌아간다. 30초 윈도우의 경계에 도달한 표본은 제외하므로 한 번의 대량 투입만으로 지속 공급을 통과할 수 없다.

패배는 기존과 동일하게 주 항성이 초신성에 도달했을 때 발생한다. 승리와 패배는 모두 terminal 상태이며, 한 번 확정된 결과·완료 시각·진행 수치는 이후 입력으로 바뀌지 않는다.

## Runtime 권위

- `ASRStar`가 누적 전달량, Simulation 시간, 최근 30초 도착 표본을 소유한다.
- 일시정지 중에는 Simulation 시간과 유지 시간이 증가하지 않는다.
- 최종 결과가 확정되면 Simulation을 정지하고 `OnStellarRunCompleted`를 한 번만 방송한다.
- 승리 후의 추가 연료 투입과 주기별 소비 증가를 거부해 결과 화면 뒤에서 상태가 변하지 않게 한다.
- 기존 `OnStellarSupernovaGameOver`는 Blueprint 호환성을 위해 유지하지만, 플레이어 결과 화면은 승패 공통 Delegate를 사용한다.

계약 판정은 World와 무관한 순수 모델인 `FSRStellarRunContractModel`에 있다. Runtime, UI Fixture, 자동화 테스트가 같은 규칙을 사용한다.

## HUD와 결과 화면

상단 항성 Rail은 다음 정보를 항상 표시한다.

- `별`: 현재 생존시간 또는 안정화 완료
- `목표`: 점화 누적량, 공급 누적량, 최종 요구 유입률, 유지 시간 중 현재 필요한 한 값
- `유입`, `소비`, `수지`, `도착`: 실제 운영 상태

최종 목표량은 채웠지만 유입률이 부족하면 `목표` Badge가 경고색으로 변한다. 유입률을 만족하면 Badge는 유지 시간으로 바뀌며, 승리하면 `목표 완료`와 `별 안정화`로 고정된다.

기존 `USRGameOverWidget`은 Asset과 Controller 호환성을 위해 이름을 유지하되 승패 공통 Run 결과 화면으로 동작한다. 승리 화면은 누적 전달량, 최종 유입률, 완료 시간을 표시하고 패배 화면은 저장 연료와 초신성 상태를 표시한다.

## 설정값

`Project Settings > Star Rovers Simulation > Run Contract > Stellar Stabilization`에서 다음 값을 조정한다.

- `bFiniteStellarVictoryEnabledV2`
- `StellarEmergencyDeliveryTargetV2`
- `StellarSustainedSupplyDeliveryTargetV2`
- `StellarVictoryDeliveryTargetV2`
- `StellarVictoryRequiredIncomePerSecondV2`
- `StellarVictoryRequiredSustainSecondsV2`
- `TargetRunDurationSecondsV2`

잘못된 음수는 0으로 보정하고 세 누적 목표는 항상 단조 증가하도록 정규화한다. `TargetRunDurationSecondsV2`는 Telemetry와 Balance 목표이며 승리에 인위적인 최소 시간을 추가하지 않는다.

## 현재 단계의 Balance 경계

5,000 / 25,000 / 100,000과 100/s / 30초는 유한 Run 계약을 검증하기 위한 첫 기준값이다. 기존 항성 소비가 사용하는 주기별 지수 증가 곡선은 아직 유지한다. 다음 경제·압력 단계에서 실제 광맥 산출량, 가공 Cycle, 항성 연료 족보, 행성 간 수송시간을 함께 Simulation한 뒤 목표량과 소비 곡선을 동시에 조정해야 한다.

따라서 현재 구현은 “언제 승리하는가”를 완성하지만 “평균적인 Seed에서 정확히 30분에 도달하는가”까지 확정한 최종 Balance는 아니다.

## 자동 검증 계약

- 세 단계의 경계값과 진행 전환
- 최종 유입률 이탈 시 연속 유지 시간 Reset
- 승리·패배 terminal 불변성
- 잘못된 설정값 정규화
- 25~35분 목표 설정과 Debug 해금 분리
- HUD의 누적량 → 요구 유입률 → 유지 시간 → 완료 표현
- 승리·패배 결과 문구와 핵심 통계
- 실제 Project HUD의 `StellarObjectiveBadge` Widget 계약

## 1단계 검증 결과

- `StarRoversEditor Win64 Development`: 성공
- `StarRovers.UI`: 50/50 성공
- `StarRovers.ResourceSystem`: 62/62 성공
- `StarRovers.UI.Integration.PIE.ProjectHUDContract`: 실제 프로젝트 Map에서 성공
