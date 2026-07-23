# 10. BOSS WORKLIST — 보스전 남은 작업 정리

> **이 문서의 목적**: 다른 로컬(다른 PC/다른 세션)에서 보스 파트 작업을 이어받을 때,
> 이 문서만 읽고 바로 시작할 수 있도록 **현황 / 남은 작업 / 구현 가이드**를 정리한다.
> 작업을 끝내면 해당 체크박스를 채우고 커밋에 포함할 것.

> ⭐ **지금 이어서 하는 작업(최우선)**: **백헤드 데칼 머티리얼 제작** →
> 노드 그래프 전 과정을 자립형으로 정리한 [`11_BACKHEAD_DECAL_TODO.md`](11_BACKHEAD_DECAL_TODO.md) 를 그대로 따라갈 것.

**우선순위 (위에서부터)**

| 순위 | 항목 | 상태 |
|---|---|---|
| 1 | 기믹 — 지형파괴 마무리 | 코어 로직 완료, 연출/UI/검증 남음 |
| 2 | 보스 체력 UI + 무력화 게이지(보스 발밑) + 백헤드 데칼 머티리얼 | C++ 준비 완료, 위젯/머티리얼 제작 남음 |
| 3 | 보스 사망 → 클리어 | **코드 구현 완료** — 보스 BP에 사망 몽타주 지정만 남음 |
| 4 | 클리어 연출 — **카메라 연출 + 위젯 방식으로 확정** | **코드 구현 완료** — WBP_RaidClear 제작만 남음 |
| - | 넉백/낙사 시스템 | **구현 완료** (사용법: §5) |

**확정된 결정 사항 (2026-07-20 담당자 확인)**

- 클리어 연출: **레벨 시퀀스 없이 카메라 연출(BossArenaCamera 줌인/슬로모) + 클리어 위젯**
- 보스 사망 모션: **몽타주 에셋 있음 → 사망 몽타주 재생으로 처리**
- 클리어 후 흐름: **그 자리에서 종료** (배너 → 카메라 복귀 `EndEncounter`, 레벨 이동/결과 화면 없음)
- 무력화(스태거) 게이지: 화면 상단 체력바에 붙이지 않고 **보스 메쉬 아래(발밑, 월드 공간)에 표시**
- 백헤드 표시: C++ 컴포넌트(`UBackHeadDecalComponent`)는 완성 → **데칼 머티리얼 제작이 남은 작업** (가이드: §2-3)

---

## 0. 지금 당장 이어서 할 것 (최우선, 2026-07-21 실플레이 검증 중단 지점)

- [ ] **저스트가드 = 노티파이 기반 판정 (AOE 아님, 2026-07-21 방식 확정)**
  판정은 **장판(AOE)이 아니라** 몽타주의 "보스 공격 순간" 프레임 노티파이가 직접 한다.
  성공=무피해, 실패=대상에게 **데미지 GE 직접 적용**(장판으로 데미지 주지 않음).
  → 담당자 의도: 창(G 1회) → 가드 상태 → 개발자가 찍은 공격 타점에 [가드상태+정면 판정].
  - **몽타주 2개 노티 배치**:
    1. `Just Guard Window`(NotifyState, 노랑 구간=가드 가능): `bOnlyCurrentTarget=true` (기믹 대상만),
       **`GuardStateDuration=1.0`** (검정 구간=1초 — 가드 상태 유지시간은 '창'이 정한다)
    2. **`Boss: Just Guard Judge (공격 순간 판정)`** (신규 `AnimNotify_BossJustGuardJudge`): 보스 공격 타점 프레임(빨간 선)에 1개
  - Judge 노티 설정: `bDebugBypassDirection=true`(테스트), `DamageEffect`=플레이어 피격 GE(없으면 데미지 없이
    판정만), `DamageCoefficient`, `bGroggyOnSuccess`=2-4(마지막 판정)만 ✅. (지속시간은 여기 없음 — 창에 있음)
  - 배치: 노란 창이 **[Judge − 1초, Judge]** 를 덮게 (그 안에 G 를 눌러야 성공)
  - **AOE 방식(`UBossAoeJustGuardEffect`, `BP_JustGuard`)은 기믹에선 안 씀** — 만들어둔 BP_JustGuard/SpawnAOE 노티는 제거해도 됨. (AOE 판정 코드는 일반 저스트가드용으로 남겨둠)
  - 화면 디버그: 노랑="창 열림", 시안="입력 기록", 초록/빨강="판정 성공/실패".
    `가드가능 0명` 뜨면 타겟팅(현재 타겟≠가드 대상) 문제. G 는 창 열린 동안만 유효.
  - 설계 참고: [09_JUSTGUARD_PATTERN.md](09_JUSTGUARD_PATTERN.md) (단, 판정 주체는 노티파이로 변경됨)

- [ ] **[임시 디버그 제거]** 확인 끝나면 `BossJustGuardComponent.cpp` 의
  `AddOnScreenDebugMessage` 3곳(창 열림 1001 / 입력 기록 1002 / 판정 결과 1003) 삭제.

- [ ] **레이저가 타워를 못 지움 (Z 정렬)**: S1_4 레이저(`Boss Spawn AOE`, `bAimBySocketForward=true`,
  헤드 소켓)는 X/Y 방향은 맞게 나가나, 판정 Z(발밑 폴백 ~498)와 타워 메시 임팩트 Z(~660)가 안 맞아
  타워가 안 지워짐(캐릭터는 맞아 날아감 → 판정 자체는 살아있음). 레이저 AOE 높이 또는 타워 히트 판정
  Z 범위 조정 필요.

- [ ] **기믹 데칼 높이가 낮음**: 지형 데칼이 바닥보다 낮게 떠서 잘 안 보임 → Z 오프셋/투영 높이 조정.

- [ ] **바닥 파괴 테스트용 디버그 입력 (번호 → DestroySlice)**: 백엔드는 이미 있음
  (`AArenaSliceActor.SliceIndex` + `ABossRaidGameMode::DestroySlice(int32)` — BlueprintCallable).
  게임 중 **번호를 입력해 즉석에서 조각을 파괴하는 디버그 도구는 없음**(코드/콘텐츠/커밋 확인함).
  필요 시 `ALostArkPlayerController` 의 임시 디버그 키(E/G/Q 옆)에 Server RPC 로 `DestroySlice(index)` 를
  호출하는 키를 하나 추가하면 됨. (지금은 만들지 말고 기록만 — 레벨팀이 만든 로컬 BP 있는지 먼저 확인.)
  실제 게임 경로는 기믹 43-3 `AnimNotify_BossGimmickDestroySlice` 로 이미 동작하므로, 몽타주 43 완성되면
  그걸로도 검증 가능.

- [ ] **카운터 성공 그로기도 같은 GE 버그 잠재 (미수정)**: `UBossGroggyEffect` GE 는 적용은 되지만
  `State.Boss.Groggy` 태그를 안 붙임. 이미 **복제 루스 태그+타이머**로 우회한 곳:
  `UBossTerrainGimmickComponent::ApplyGroggy`(무력화 그로기), `UBossJustGuardComponent::MarkJustGuardedResult`
  (저스트가드 성공 그로기). **`UBossCounterComponent::ApplyCounterSuccess` 는 아직 GE 방식 그대로** —
  카운터 그로기 실제 테스트 후 같은 증상이면 동일 방식으로 교체.

- [ ] **연속 저스트가드 성공 누수 수정됨**: `JustGuarded` 태그가 스텝을 넘어 남아 다음 저스트가드 스텝의
  성공 분기를 오발(2-3 성공이 판정 없이 그로기로)하던 문제 → `BossPatternAbility::CleanupStep` 에서
  스텝 이탈 시 `JustGuarded` 를 지우도록 수정 (실패 게이트 `JustGuardFailed` 는 유지).

---

## 1. 기믹 — 지형파괴 (최우선)

### 현황 (구현 완료된 것)

| 클래스 | 역할 |
|---|---|
| `UBossTerrainGimmickComponent` | 기믹 라운드 오케스트레이터. 타워 스폰(4개 후보 위치) → 무력화 페이즈(게이지 리필 + `State.Boss.StaggerPhase` 복제 태그) → 게이지 0 시 그로기 → `RequestDestroyGimmickSlice(Delay)` |
| `ABossGimmickTower` | 감전 Rect 장판 주기 스폰. **레이저 장판(`bCanHitGimmickTargets` 켠 AOE)으로만 파괴** 가능. 서 있는 슬라이스가 파괴되면 함께 소멸. 파괴 연출 훅 `OnTowerDestroyed(bByLaser)` (BP 구현 필요) |
| `AArenaSliceActor` | 피자 조각 바닥 1개. 파괴 시 메시 숨김 + 콜리전 제거 + NavModifier(클릭이동 차단). 연출 훅 `OnSliceDestroyedFX()` (BP 구현 필요) |
| `ABossRaidGameState` | `DestroyedSliceMask` 복제(단일 진실) + `OnArenaSlicesChanged` 방송. `ArenaFloorZ`, `GetSliceIndexAt()` |
| `ABossRaidGameMode::DestroySlice` | 조각 파괴 마킹. 첫 파괴 시 보스에 `State.Boss.WeakPointExposed` (약점포착) |
| `AArenaKillVolume` | 낙사 판정 (서버). `FallDeathEffect` GE 또는 `State.Dead` 태그 폴백 |
| 노티파이 | `AnimNotify_BossGimmickSpawnTower` / `AnimNotify_BossGimmickSpawnAoe` / `AnimNotify_BossGimmickDestroySlice` / `AnimNotifyState_BossFaceGimmickSlice` — 몽타주 프레임이 각 단계를 트리거 |

흐름 제어(몽타주/분기)는 패턴 데이터(`PatternDataAsset`)가 담당. 컴포넌트는 상태+부수효과만.

### 남은 작업

- [ ] **슬라이스 파괴 연출**: `BP_ArenaSlice` 의 `OnSliceDestroyedFX` 구현 — 파편/먼지 나이아가라, 사운드, 카메라 쉐이크. (서버/클라 모두 호출됨 — 코스메틱만 넣을 것)
- [ ] **타워 파괴 연출**: `BP_BossGimmickTower` 의 `OnTowerDestroyed(bByLaser)` 구현 — 레이저 파괴/지형 소멸 각각 다른 연출 가능. `DestroyFXDelay` 안에 끝나는 연출로.
- [ ] **무력화 게이지 UI**: 보스 발밑(월드 공간) 표시로 확정 — 상세는 §2-2. 표시/숨김은 `State.Boss.StaggerPhase` 복제 태그, 값은 `UBossAttributeSet::StaggerGauge / MaxStaggerGauge` 복제 어트리뷰트 바인딩.
- [ ] **풀 플로우 멀티 검증** (리슨서버 + 클라 1): 타워 스폰 → 감전 장판 → 무력화 → (성공: 그로기 / 실패: 저스트가드 구간) → 슬라이스 파괴 → 타워 동반 소멸 → 약점포착 태그 → 파괴 지형 낙사(`ArenaKillVolume`).
- [ ] **낙사 연계 확인**: §5 넉백에서 `bCanCauseFallDeath=true` 패턴으로 파괴 지형 방향으로 밀었을 때 실제 낙사가 되는지 확인.

### 실플레이 검증 중 발견 (2026-07-21, 후순위)

- [ ] **레이저가 타워를 못 지움 (Z 정렬)**: S1_4 레이저를 `Boss Spawn AOE`(`bAimBySocketForward=true`, 헤드 소켓)로 보스 헤드 X/Y 방향 발사로 바꿨으나, 레이저 판정 Z(발밑 폴백 ~498)와 타워 메시 높이(ImpactZ 660 근처)가 안 맞아 타워가 파괴되지 않음. 캐릭터는 맞아 날아감. → 레이저 AOE의 Z/높이(또는 타워 히트 판정 Z 범위)를 맞춰야 함.
- [ ] **기믹 데칼 높이가 낮음**: 데칼 자체가 지형보다 낮게 떠서 잘 안 보임 → 데칼 Z 오프셋/투영 높이 조정.
- [x] **무력화 → 그로기**: 게이지 0 도달 시 `State.Boss.Groggy` 가 안 붙던 문제 해결. 원인 = `UBossGroggyEffect` GE 가 적용은 되나(TargetTags 컴포넌트) 태그를 안 붙임. `UBossTerrainGimmickComponent::ApplyGroggy` 를 **복제 루스 태그 + 타이머**(`EndGroggy`)로 직접 부여하도록 교체. **주의: 카운터 성공 그로기(`UBossCounterComponent`)도 같은 GE 라 동일 버그 잠재 — 아직 미수정.**

---

## 2. 보스 UI (체력바 · 무력화 게이지 · 백헤드 데칼)

### 2-1. 화면 상단 체력바

**현황 (C++ 준비 완료 — WBP Reparent + 그래프만 만들면 됨)**

- `ABossBase::OnBossHealthChanged(NewHealth, MaxHealth)` — 서버/클라 모두 방송되는 델리게이트.
- `ABossBase::TotalHealthBars`(기본 500줄), `GetCurrentHealth()` / `GetMaxHealthValue()` — 위젯 최초 구성용.
- `UBossHealthBarLibrary` (BlueprintPure 3종):
  - `GetHealthFraction` — 퍼센트 텍스트용 (0~1)
  - `GetBarsRemaining` — `xN줄` 표기
  - `GetCurrentBarFill` — 지금 깎이는 '한 줄'의 0~1 채움 (줄 경계마다 1.0 리필, 위젯에서 보간하면 피 닳는 연출)
- **`UBossHUDWidget`** (`Boss/UI/BossHUDWidget.h`, 신규) — WBP_BossHUD 의 C++ 베이스. `BossPlayerStatusWidget` 과 동일한 주입 패턴.
  - `InitForBoss(Boss)` (컨트롤러가 호출) → `OnBossHealthChanged` 바인딩 + 초기값 1회 반영 + `GameState.OnRaidCleared` 구독.
  - `OnBossHealthUpdated(NewHealth, MaxHealth)` — **BlueprintImplementableEvent**. WBP 가 여기서 라이브러리로 막대/줄수/퍼센트 갱신. `Boss->TotalHealthBars` 로 줄 수 참조.
  - `OnRaidCleared` — **BlueprintNativeEvent**. 기본 Collapsed(숨김), WBP 에서 페이드 연출로 오버라이드 가능.
- **생성/배치는 `ALostArkPlayerController::TryCreateBossHUD()`** — 로컬 컨트롤러 BeginPlay 에서 호출. **보스 레벨 판별 = 월드 GameState 가 `ABossRaidGameState` 인지** (튜토/카오스던전은 다른 GameState 라 자동 제외). GameState/보스 복제 지연 시 0.25s 간격으로 최대 10s 재시도. 생성 후 `AddToViewport` + `InitForBoss`.

**남은 작업 (전부 에디터)**

- [ ] **WBP_BossHUD 부모 재지정**: `UBossHUDWidget` 으로 Reparent (`Content/Levels/UI/WBP_BossHUD`).
- [ ] **WBP 그래프**: `OnBossHealthUpdated(NewHealth, MaxHealth)` 이벤트 구현 → `UBossHealthBarLibrary`(`GetHealthFraction`/`GetBarsRemaining`/`GetCurrentBarFill`, 줄수는 `Boss->TotalHealthBars`)로 체력바·`xN`·퍼센트 갱신. 화면 상단 고정 + 보스 이름.
- [ ] **PlayerController BP 에 `BossHUDWidgetClass = WBP_BossHUD` 지정** (UI 카테고리). 보스 레벨이 쓰는 컨트롤러 BP 에 설정하면 됨 — 게이트가 있어 튜토/카오스던전에 같은 PC 를 써도 안 뜬다.
- [ ] (선택) `OnRaidCleared` 오버라이드로 클리어 시 페이드아웃. (기본은 즉시 숨김)
- [ ] (선택) 페이즈 표시: `Boss.Phase.N` 태그 기반.

### 2-2. 무력화(스태거) 게이지 — 보스 메쉬 아래(발밑) 표시로 확정

체력바에 붙이지 않는다. 보스 발밑 근처에 월드 공간으로 띄운다.

**구현 가이드**

- [ ] 보스 BP 에 `UWidgetComponent` 추가 (Space=**Screen** 권장 — 항상 카메라를 보며 크기 일정. 쿼터뷰 고정 카메라라 World 여도 무방하나 Screen 이 간단).
  - 부착 위치: 캡슐 하단 근처 소켓/오프셋 (보스 발밑 앞쪽, 데칼과 겹치지 않게 Z 살짝 위).
- [ ] `WBP_BossStaggerGauge` 제작: 좌→우로 닳는 단순 바 + (선택) "무력화" 라벨.
  - 값 바인딩: `UBossAttributeSet::StaggerGauge / MaxStaggerGauge` (복제 어트리뷰트. ASC 의 `GetGameplayAttributeValueChangeDelegate` 바인딩 또는 틱 폴링).
- [ ] 표시/숨김: `State.Boss.StaggerPhase` **복제 루스 태그**가 있을 때만 Visible (무력화 페이즈에만 노출). `RegisterGameplayTagEvent` 바인딩.
- [ ] 게이지 0 도달(그로기 성공) 시: 짧은 강조 연출(번쩍/흔들림) 후 숨김 — `State.Boss.Groggy` 태그로 트리거 가능.

### 2-3. 백헤드 데칼 — 머티리얼 제작 가이드 (C++ 은 완성)

**현황**: `UBackHeadDecalComponent`(UDecalComponent 파생) 완성 —
보스에 부착되어 회전을 따라가고, `UpdateRadius(캡슐 반경)` 이 데칼 풋프린트와 MID 의 `Radius` 스칼라 파라미터를 자동 갱신한다.
**"Decal Material" 슬롯에 넣을 머티리얼(M_BossBackHead)이 없어서 현재 아무것도 안 보이는 상태.** 이 머티리얼 제작이 남은 작업의 전부다.

- [ ] **M_BossBackHead 머티리얼 제작** (아래 스펙)
- [ ] 보스 BP 데칼 컴포넌트에 머티리얼 지정 + 존 각도를 `ABossBase` 값과 일치시키기
- [ ] (선택) 약점포착 연동 연출

**머티리얼 스펙**

| 항목 | 값 |
|---|---|
| Material Domain | **Deferred Decal** |
| Blend Mode | **Translucent** (DBuffer 필요 시 DBuffer Translucent Color — 프로젝트 세팅 확인) |
| 파라미터 (스칼라) | `Radius` (컴포넌트가 자동 주입), `HeadHalfAngle` (기본 45), `BackHalfAngle` (기본 45), `RingThickness`, `Opacity` |
| 파라미터 (벡터) | `HeadColor` (예: 주황/빨강), `BackColor` (예: 파랑/보라) |

**그래프 구성 (절차적 부채꼴 마스킹)**

1. TexCoord(데칼 UV, 0~1) → `-0.5` 로 중심 원점화 → 극좌표 변환:
   - 각도 = `atan2(Y, X)` (도 단위 변환), 반경 = `length(XY) * 2` (0~1)
2. **전방(+X) 기준 각도**로 헤드/백 존 마스크:
   - 헤드 존: `abs(각도) < HeadHalfAngle` → `HeadColor`
   - 백 존: `abs(각도) > 180 - BackHalfAngle` → `BackColor`
   - 측면: 투명 (판정도 보너스 없음 — `UBossCombatStatics::GetHitZone` 과 동일 규칙)
3. **링 형태 권장**: `반경 ∈ [1-RingThickness, 1]` 구간만 불투명 — 꽉 찬 부채꼴은 바닥을 너무 가린다. 링 + 존 경계선만으로 충분히 읽힌다.
4. 존 경계는 `smoothstep` 으로 1~2° 부드럽게.

**주의사항**

- **각도 일치가 생명**: 머티리얼의 `HeadHalfAngle/BackHalfAngle` 은 `ABossBase::HeadZoneHalfAngle/BackZoneHalfAngle`(판정값, 기본 45°)과 반드시 같게. 어긋나면 "표시는 백인데 판정은 측면"이 된다. 보스 BP OnConstruction 에서 MID 로 넘겨 자동 동기화하는 것도 좋다.
- **UV 전방 축 확인**: 데칼이 Pitch -90 으로 지면 투영되므로 UV 의 +X 가 보스 전방과 일치하는지 PIE 에서 확인. 90° 틀어져 있으면 머티리얼에서 각도에 오프셋 상수 하나만 더하면 된다.
- 데칼 크기/판정은 이미 캡슐 반경 연동(`UpdateBackHeadDecal`) — 머티리얼은 UV 0~1 만 신경 쓰면 된다.
- (선택) 약점포착(`State.Boss.WeakPointExposed`) 중에는 어디서 때려도 보너스이므로 링 전체를 금색으로 바꾸는 연출을 붙이면 판정 규칙(§`BossCombatStatics` 주석)과 표시가 일치한다.

### 2-4. 어그로 표식(몸통 마커) — 지정된 1명 시각화 (✅ C++ 완료, WBP만 남음)

기믹이 랜덤 지정한 **1명의 몸통**에 마커를 띄워 "이 사람이 어그로(레이저 대상 + 나중에 나올 저스트가드 담당)"임을 전원에게 보여준다.
머리 위(전하)·발밑(무력화 게이지)과 겹치지 않게 **몸통 중앙(Z=+30)** 에 표시.

**현황 (C++ 완료 — 빌드 검증됨)**

- 태그: `State.Player.Marked` (복제 루스). **타겟 선정(`SelectTarget`)과 독립** — 이 태그는 아래 API/노티로만 켠다. 그래서 일반 패턴의 타겟 선정으론 마커가 안 뜬다.
- `UBossTargetingComponent::MarkCurrentTarget()` / `ClearMark()` — 현재 타겟에 표식 부여/회수(항상 최대 1명, 대상은 스냅샷 보관). `GetMarkedTarget()` 조회.
- 노티파이 2개: **`Boss Mark Target (표식 켜기)`** / **`Boss Clear Mark (표식 끄기)`** (서버 게이트, `Boss Select Target` 과 동일 패턴).
- 위젯 호스트: `UBossChargeGaugeComponent` 에 몸통 슬롯 추가 — `BodyMarkWidgetClass` / `BodyMarkZOffset(+30)` / `BodyMarkDrawSize`. 표식 태그 감시 → `OnMarkedChanged(bool)` 델리게이트 + `IsMarked()`.
- 안전장치: 보스 사망(`ABossBase::HandleDeath`) 시 `ClearMark()` 자동 호출(몽타주 중단돼도 마커 안 남음).

**남은 작업 (전부 에디터)**

- [ ] **`WBP_BossMark` 제작** 후 부모를 `UBossPlayerStatusWidget` 으로 **Reparent** (전하/게이지 위젯과 동일 베이스).
- [ ] WBP 그래프: `OnStatusInitialized` 에서 `ChargeComponent->OnMarkedChanged` 바인딩 → 표식 시 Visible / 해제 시 Collapsed. Construct 초기값은 `IsMarked()`. (기본 Hidden)
- [ ] 전하 게이지 컴포넌트 디폴트의 `Boss|Charge|UI > BodyMarkWidgetClass` 에 `WBP_BossMark` 지정 (몸통 높이는 `BodyMarkZOffset`).
- [ ] **기믹 몽타주 노티 배치**: `Boss Select Target(랜덤)` → 바로 뒤 `Boss Mark Target` … 레이저·저스트가드 지나 시퀀스 끝에 `Boss Clear Mark`.

> ⚠️ 저스트가드는 판정 순간 `GetCurrentTarget()` 을 쓰고 마커는 켠 시점 스냅샷이므로, **표식~저스트가드 사이에 다른 `SelectTarget` 을 부르면 대상이 어긋난다.** 이 기믹 몽타주 안에서 재선정을 안 하면 항상 일치(현재 방식). 중간 재선정이 필요해지면 저스트가드가 `GetMarkedTarget()` 을 쓰도록 바꿔 묶을 것.

---

## 3. 보스 사망 → 클리어 (✅ 코드 구현 완료)

### 구현된 흐름 (전부 서버 권위)

1. **사망 감지**: `ABossBase::OnHealthChanged` — Health 0 도달 시 `HandleDeath()` (1회 가드 `bDead`).
2. **`ABossBase::HandleDeath()`**:
   - `State.Dead` 복제 루스 태그 (클라 UI/플레이어 파트가 이 태그로 감지)
   - `BossPatternComponent::StopCombat()`(신규 API — 이후 패턴 재개 금지) → `CancelAllAbilities()`
   - 무력화 페이즈 종료, 살아있는 장판 전부 `DestroyAoeNow()`, 기믹 타워 Destroy (잡힌 플레이어는 장판 EndPlay 안전 복구)
   - 이동 정지 + 캡슐 Pawn 채널 Ignore (시체가 길막 안 함, 바닥 지지는 유지)
   - **사망 몽타주 멀티캐스트** 재생 → 종료 시 `bPauseAnims` 로 마지막 포즈 고정 (Idle 복귀 방지)
   - `OnBossDied` 방송(BP 훅) + `BossRaidGameMode::NotifyBossDied()` 통지
3. **클리어 상태 복제**: `ABossRaidGameState::bRaidCleared` (`ReplicatedUsing`) + `OnRaidCleared` 방송 (서버/클라 모두) — 체력바 숨김 등은 여기 구독.

### 남은 작업

- [ ] **보스 BP 에 `DeathMontage` 지정** (Boss|Death 카테고리). 미지정 시 로그 경고 + 포즈만 유지.
- [ ] 사망 FX/사운드: `OnBossDied`(서버) 또는 `State.Dead` 태그(클라)에 BP 바인딩.
- [ ] 체력바 위젯 숨김: `BossRaidGameState.OnRaidCleared` 구독 (§2-1 위젯 만들 때 함께).

> 협업 경계: 플레이어 쪽엔 **GameState 복제 + State.Dead 태그**로만 전달된다 (docs/00 원칙 1 준수).

---

## 4. 클리어 연출 — **카메라 연출 + 위젯 방식, ✅ 코드 구현 완료**

레벨 시퀀스 없음. 클리어 후에는 **그 자리에서 종료** (레벨 이동/결과 화면 없음).

### 구현된 시퀀스 (`ABossRaidGameMode::NotifyBossDied` — 서버 주도)

| 시점 | 동작 |
|---|---|
| t=0 (사망) | 전 플레이어 뷰타겟 → **클리어 카메라** (보스 정면 `ClearCameraDistance`/`Height`에서 내려다보는 고정 샷, 복제 스폰) + **글로벌 슬로모** (`ClearSlomoDilation`, WorldSettings 복제로 전 머신 동시) |
| t=`ClearSlomoDuration`(실초) | 슬로모 복구 |
| t=`ClearBannerDelay` | `GameState.MarkRaidCleared()` → 복제 → **전 머신 클리어 배너** (`OnRaidCleared` 방송 + `ClearWidgetClass` 위젯 자동 생성, `ClearWidgetLifetime` 후 자동 제거. 미지정 시 화면 디버그 텍스트 폴백) |
| t=+`ClearHoldTime` | `EndEncounter()` — 카메라 각자 캐릭터 복귀 + 클리어 카메라/공명 타이머 정리 |

모든 타이밍/거리 값은 **GameMode BP 의 `Raid|Clear` 카테고리**에서 조정.

### 남은 작업

- [ ] **WBP_RaidClear 제작** 후 **GameState BP 의 `ClearWidgetClass` 에 지정**: "클리어" 배너 + 사운드. (위젯 등장 애니메이션은 WBP Construct 에서)
- [ ] 클리어 카메라 구도 튜닝: 실제 보스 크기에 맞춰 `ClearCameraDistance/Height/FocusHeight` 조정 (기본 1300/700/350)
- [ ] (선택) 연출 중 입력 잠금: 필요 시 `GameState.OnRaidCleared` / `IsRaidCleared()` 를 플레이어 파트가 구독해 처리 (직접 참조 금지)
- [ ] (선택) 클리어 타임 표시: GameState 에 조우 시작 시각 기록 추가

---

## 5. 넉백/낙사 시스템 (구현 완료 — 사용법)

패턴별로 "맞으면 어떻게 되는가"를 AOE 쪽에서 설정한다. 코드는 `BossPatternActorBase` 에 있음.

### 설정 위치 (둘 중 하나)

1. **장판 BP 디폴트**: `Aoe|Knockback` 카테고리의 `Knockback` 구조체
2. **스폰 노티파이(권장)**: `Boss Spawn AOE`(및 도형별 서브클래스) 노티파이의 `bOverrideKnockback` + `KnockbackOverride` — 같은 장판 BP 를 몽타주(패턴)마다 다른 리액션으로 재사용

### 필드 설명

| 필드 | 의미 |
|---|---|
| `Reaction` | `None` 데미지만 / `PushBack` **뒤로 밀려 날아감** / `LaunchUp` **그 자리에서 위로 잠깐 솟음** (수평 이동 0 → 낙사 원천 불가) |
| `Direction` | PushBack 방향: `FromCenter` 장판 중심에서 바깥(원형/부채꼴) / `AlongForward` 장판 전방(직선·사각·투사체) / `FromCaster` 보스 중심에서 바깥(Follow 회전 장판) |
| `HorizontalSpeed` | 수평 넉백 속도 cm/s (기본 1200) |
| `VerticalSpeed` | 위로 뜨는 속도 cm/s (기본 420) |
| `bCanCauseFallDeath` | **낙사 유발 여부**. `true` = 그대로 날려서 파괴 지형/아레나 밖이면 추락(KillVolume 처리). `false` = 밀려나는 경로 바닥을 샘플링해 **구멍 직전까지만** 밀리도록 수평 속도 자동 클램프 |

### 적용 경로 (자동)

- 기본 데미지 장판(OnHitEffect 미지정): 데미지 직후 자동 적용
- 저스트가드 **실패** 판정: 데미지와 함께 자동 적용 (장판 BP 에 Knockback 설정 시)
- 잡기(Grab): 기존 던지기(`ThrowHorizontal/VerticalSpeed`) 유지 — 낙사 연계가 의도된 패턴이라 새 시스템 미적용
- 잡힘 상태(MOVE_None) 대상은 넉백 스킵 (부착 찢어짐 방지). 판정/넉백은 전부 서버 권위

### 패턴 예시

- 충격파(원형, 뒤로 크게 밀지만 낙사는 안 되게): `PushBack + FromCenter + bCanCauseFallDeath=false`
- 낙사 유도 장판(파괴 지형 쪽으로 날리기): `PushBack + bCanCauseFallDeath=true`
- 지진/내려찍기(제자리에서 붕 뜨기만): `LaunchUp`
