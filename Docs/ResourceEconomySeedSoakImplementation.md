# Resource V2 다중 Seed Soak와 최종 기준 Balance

## 결론

현재 기준 수치는 유지한다.

| 항목 | 확정 기준 |
|---|---:|
| Card 광맥 1개의 총량 | 120 |
| Common Ore 광맥 1개의 총량 | 180 |
| Biomass Feedstock 광맥 1개의 총량 | 180 |
| 기본 Miner Cycle | 4초 |
| Stellar Fuel Fabricator Cycle | 10초 |
| 기본 기준 Fuel Batch | 824 Energy |
| 최적화 기준 Fuel Batch | 1,180 Energy |

512개 Root Seed의 행성 포트폴리오와 실제 D3D12 PIE 재생 결과 모두 Soft-lock, 무한 광맥, 접근 불가 광맥 또는 지나치게 짧은 첫 Front를 만들지 않았다. 따라서 이번 단계에서는 Deposit 총량이나 환경별 Spawn 최소·최대치를 다시 조정하지 않았다.

이 값은 앞으로 절대 바꿀 수 없는 상수가 아니라 현재 Reference Content와 25~35분 Run 계약의 검증된 기준선이다. Card, Facility Cycle, 항성 압력 또는 행성 수가 바뀌면 같은 Soak를 다시 통과해야 한다.

## 왜 다중 Seed 검증이 필요한가

단일 PIE는 다음 문제를 놓칠 수 있다.

- 특정 Family Card가 없는 태양계
- 행성 종류는 다양하지만 실질 자원 구성이 같은 태양계
- 한 Family만 최소 광맥 수가 지나치게 적은 Seed
- 바다와 자연 구조물 때문에 Miner를 붙일 수 없는 광맥
- 원격 Line은 충분하지만 운송 도착 전에 Star가 붕괴하는 시간 경계
- 같은 Seed를 다시 실행해도 지형이나 광맥 Cell이 달라지는 비결정성

이번 구현은 빠른 순수 포트폴리오 Soak와 느린 실제 PIE 재생을 분리한다. 전자는 512개 이상을 빠르게 검사하고, 후자는 실제 Cube Sphere Cell과 자연 구조물 배치가 같은 계약을 지키는지 확인한다.

## Root Seed 결정론

기존 Generator는 태양계의 Root Seed를 사용하면서도 `bRandomizeGenerationSeedEachRun`이 켜진 Star, Planet, Moon의 하위 Seed를 별도의 비결정적 난수로 만들었다. 그 결과 같은 Root Seed를 다시 넣어도 지형과 광맥 Cell이 달라질 수 있었다.

현재 규칙은 다음과 같다.

1. Map Instance는 Run 시작마다 새로운 Root Seed를 선택할 수 있다.
2. 한 Root Seed가 정해진 뒤 모든 하위 천체 Seed는 같은 `FRandomStream`에서 파생한다.
3. `GenerateRuntimeSystemForSeed`는 설정된 Editor Seed와 Randomize Flag를 훼손하지 않고 특정 Root Seed로 전체 System을 재생성한다.
4. `GetLastRuntimeGenerationSeed`는 실제 사용한 Root Seed를 노출한다.
5. 자연 구조물도 같은 Root Seed에서 파생된 Stream을 사용한다.

따라서 다른 Run은 계속 달라지지만, 기록한 Root Seed는 행성 환경, 천체별 지형 Seed와 Resource Deposit Cell까지 재현한다.

## 순수 512 Seed 결과

검사 범위는 Root Seed `1~512`다. 실제 `BP_SolarSystemGenerator` CDO와 실제 여섯 Planet Data Asset을 읽었으며 별도의 Test 전용 자원 표를 사용하지 않았다.

| 관측 항목 | 결과 |
|---|---:|
| 통과 Seed | 512 / 512 |
| 행성 수 | 5~7 |
| 한 System의 고유 환경 수 | 4~6 |
| 보장 완전 Card Front 수 | 4~10 |
| 최대 Spawn Envelope 기준 완전 Front 수 | 7~17 |
| 환경별 Resource 종류 | 정확히 3 |
| 요구 Resource Coverage | 7 / 7 |
| 결정론적 재실행 | 512 / 512 동일 |

`완전 Card Front`는 Metal, Crystal, Organic, Plasma, Void의 광맥을 각각 하나씩 묶을 수 있는 횟수다. 한 행성에 다섯 Family가 모여 있다는 뜻이 아니며, 여러 천체의 공급원을 연결할 수 있는 전략적 개척 횟수다.

### 환경 출현

아래 수치는 중복 행성 Slot 수가 아니라 해당 환경이 한 번 이상 포함된 System 수다.

| 환경 | 포함된 System |
|---|---:|
| Temperate | 403 / 512 |
| Arid Desert | 394 / 512 |
| Frozen Ocean | 444 / 512 |
| Badlands | 354 / 512 |
| Lava Ocean | 325 / 512 |
| Toxic Wetland | 437 / 512 |

가장 적은 Lava Ocean도 63% 이상의 System에 나타났다. 최소 4종 환경 선택과 자원 Coverage가 특정 환경을 사실상 제거하지 않는다.

### Resource Source와 광맥 Envelope

`보장`은 DA의 `MinimumGuaranteedCount` 합, `잠재`는 `MaxCount` 합이다. 잠재값은 실제 Spawn 개수를 보장하는 값이 아니라 해당 Seed가 가질 수 있는 상한이다.

| Resource | Source 행성 | 보장 광맥 | 잠재 광맥 |
|---|---:|---:|---:|
| Helios Iron | 1~6 | 4~28 | 7~48 |
| Echo Quartz | 1~5 | 4~24 | 7~41 |
| Verdant Spore | 1~6 | 6~34 | 10~57 |
| Aurora Plasma | 1~4 | 4~22 | 7~37 |
| Null Pearl | 1~5 | 4~28 | 7~47 |
| Common Ore | 2~6 | 10~30 | 16~48 |
| Biomass Feedstock | 1~5 | 5~25 | 8~40 |

모든 Seed가 모든 필수 자원의 Source를 하나 이상 가진다. 가장 희소한 Card도 완전 Front 네 개를 보장하므로 첫 선택 실수나 한 광맥 고갈이 곧 Run Soft-lock으로 이어지지 않는다.

## 고갈 시간의 두 가지 의미

Card 광맥 한 개는 120개를 가진다.

~~~text
Miner 단독 최대 채굴: 120 × 4초 = 480초 = 8분
제작기 기준 동기화 Front: 120 × 10초 = 1,200초 = 20분
~~~

두 값은 모순이 아니다. 출력이 막히지 않은 Miner 하나만 보면 8분에 광맥을 비울 수 있지만, Card 다섯 장을 10초마다 소비하는 기준 Fuel Line에서는 Backpressure 때문에 한 동기화 Front가 20분 동안 공급된다.

따라서 플레이 감각을 판단할 때는 다음을 구분해야 한다.

- Miner 재배치 빈도: 실제 Buffer, Conveyor와 병렬 소비량의 영향
- Fuel Front 수명: 가장 느린 Card와 최종 Fabricator Cycle의 영향
- 전역 Reserve: 여러 천체에 남아 있는 대체 Front 수

전역 보장 Front가 4~10이라는 이유로 한 Run에서 모든 광맥을 다 소모하도록 설계하지 않는다. 이 여유는 탐사 선택, Family별 Line, 행성간 물류와 실패 복구를 위한 전략적 후보군이다.

## Run 시간 경계

### 기본 Front 하나

| 항목 | 값 |
|---|---:|
| 생산 시작 | 05:00 |
| 운송 지연 | 30초 |
| 첫 도착 | 05:30 |
| 최대 Batch | 120 |
| 마지막 도착·첫 공급 고갈 | 25:20 |
| 결과 | 패배 |
| 패배 시각 | 26:47 |

첫 Front는 학습과 복구 시간을 주지만 혼자서는 승리할 수 없다.

### 분산 최적화 Front 선행 출발

| 항목 | 값 |
|---|---:|
| 기존 기본 Front | 05:00 생산, 30초 운송 |
| 원격 최적화 Front 생산 시작 | 23:00 |
| 행성간 운송 지연 | 120초 |
| 원격 첫 Batch 도착 | 25:00 |
| 결과 | 승리 |
| 승리 시각 | 25:39 |

원격 Line을 25:00에 처음 가동하고 120초 운송하면 첫 Batch가 27:00에 도착한다. 이 경우 Star가 26:47에 먼저 붕괴한다. 그래서 Route Delay는 단순 감점이 아니라 플레이어가 약 2분 일찍 생산과 발사를 시작해야 하는 예측 문제다.

Deposit 120을 늘려 이 실패를 숨기지 않은 이유도 여기에 있다. 수송 시간을 미리 읽고 선행 출발하는 것이 다중 천체 자동화의 핵심 판단이어야 한다.

## 실제 D3D12 PIE 재생 결과

`SolarSystem` Map을 실제 렌더링 PIE로 시작한 뒤 전체 System을 세 번 재생성했다.

| Root Seed | 행성 | 고유 환경 | 실제 V2 광맥 |
|---:|---:|---:|---:|
| 104729 | 5 | 4 | 125 |
| 130363 | 6 | 4 | 150 |
| 104729 재실행 | 5 | 4 | 125 |

검증 내용:

- Seed `104729`의 두 Signature가 천체 환경·천체 Seed·Resource ID·Cube Face·Cell X/Y·총량까지 동일함
- Seed `130363`은 다른 Signature를 만듦
- 모든 광맥이 Card 120 또는 Utility 180의 정확한 Catalog 총량으로 시작함
- 모든 광맥 옆에 비어 있고 건설 가능한 Cell 또는 철거 가능한 자연 구조물 Cell이 하나 이상 존재함
- 실제 광맥 개수가 해당 환경 DA의 Minimum~Maximum Spawn Envelope 안에 있음
- 모든 System이 Card 5종과 Utility 2종을 포함함

## 구현 경로

- `FSRPlanetEnvironmentSelector::GetEnabledResourceRuleAvailability`: Profile과 Planet Override를 합친 실제 Resource Spawn 계약 조회
- `FSRResourceEconomySoakModel`: Seed 선택, 환경 다양성, Coverage, 광맥 Envelope와 Run 결과를 계산하는 순수 모델
- `SRRunResourceEconomySoak` Commandlet: 결과 Log와 Seed별 CSV 출력
- `GenerateRuntimeSystemForSeed`: 실제 System 재생 API
- Phase 22 Automation: 순수 512 Seed와 D3D12 PIE A/B/A 재생 Gate

## 실행 방법

~~~powershell
& 'D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'D:\Unreal Projects\StarRovers\StarRovers.uproject' `
  -run=SRRunResourceEconomySoak -FirstSeed=1 -SeedCount=512 `
  '-LocalDataCachePath=D:\Unreal Projects\StarRovers\Saved\DDC' `
  -unattended -nop4 -nosplash -nullrhi -DDC-ForceMemoryCache -log
~~~

기본 CSV 출력:

~~~text
Saved/Balance/ResourceV2SeedSoak.csv
~~~

`-Output=상대경로` 또는 절대경로로 바꿀 수 있다. CSV는 Seed별 행성 수, 환경, Coverage, 보장·잠재 Front, Fuel Batch 수, Resource별 Source/보장/잠재 광맥과 실패 원인을 기록한다.

## 자동 검증

- `StarRovers.ResourceSystem.Phase22.SeedSoak.AuthoredPortfolio`
  - 실제 여섯 Planet DA로 512 Seed 검사
  - 전체 CSV의 Byte 단위 재현성 검사
  - 기본 Front 패배와 120초 원격 운송 승리 경계 검사
- `StarRovers.ResourceSystem.Phase22.SeedSoak.PIE.RuntimeSeedReplay`
  - 실제 D3D12 PIE에서 Root Seed A → B → A 전체 재생성
  - 유한량, 접근성, Spawn Envelope와 Cell Signature 검사

2026-07-28 최종 검증 결과:

- `StarRoversEditor Win64 Development`: 성공
- `SRRunResourceEconomySoak`, Seed 1~512: 성공, CSV 생성, 오류 0
- `StarRovers.ResourceSystem.Phase22.SeedSoak.AuthoredPortfolio`: 1/1 성공
- D3D12 `StarRovers.ResourceSystem.Phase22.SeedSoak.PIE.RuntimeSeedReplay`: 1/1 성공
- 전체 `StarRovers.ResourceSystem`: 84/84 성공
- 전체 `StarRovers.UI`: 63/63 성공
- `StarRovers.SolarSystem.PlanetEnvironment`: 4/4 성공
- `git diff --check`: 성공

## 자동화가 대신하지 않는 것

이번 Soak는 실제 콘텐츠 수치와 생성 논리의 안전망이지만 다음을 완전한 인간 플레이와 동일하다고 주장하지 않는다.

- 플레이어가 Conveyor를 우회 배치하며 소비하는 실제 건설 시간
- Operational Capacity 우선순위 실수와 복구 속도
- Augment Offer를 순간적으로 읽고 선택하는 난이도
- 여러 Route와 Missile이 동시에 경쟁할 때의 체감
- 25~35분 전체 Session의 UI 가독성과 조작 피로도

이 항목은 기존 Run Telemetry와 실제 장기 Playtest로 계속 측정한다. 다만 수치 또는 DA 변경이 Seed Soft-lock이나 결정론 회귀를 만들면 Phase 22가 먼저 차단한다.
