# 10. BOSS WORKLIST — 보스전 남은 작업 정리

> **이 문서의 목적**: 다른 로컬(다른 PC/다른 세션)에서 보스 파트 작업을 이어받을 때,
> 이 문서만 읽고 바로 시작할 수 있도록 **현황 / 남은 작업 / 구현 가이드**를 정리한다.
> 작업을 끝내면 해당 체크박스를 채우고 커밋에 포함할 것.

**우선순위 (위에서부터)**

| 순위 | 항목 | 상태 |
|---|---|---|
| 1 | 기믹 — 지형파괴 마무리 | 코어 로직 완료, 연출/UI/검증 남음 |
| 2 | 보스 체력 UI + 무력화 게이지(보스 발밑) + 백헤드 데칼 머티리얼 | C++ 준비 완료, 위젯/머티리얼 제작 남음 |
| 3 | 보스 사망 → 클리어 | **미구현** (체력 0 처리 자체가 없음) |
| 4 | 클리어 연출 — **카메라 연출 + 위젯 방식으로 확정** | 미구현 |
| - | 넉백/낙사 시스템 | **구현 완료** (사용법: §5) |

**확정된 결정 사항 (2026-07-20 담당자 확인)**

- 클리어 연출: **레벨 시퀀스 없이 카메라 연출(BossArenaCamera 줌인/슬로모) + 클리어 위젯**
- 보스 사망 모션: **몽타주 에셋 있음 → 사망 몽타주 재생으로 처리**
- 클리어 후 흐름: **그 자리에서 종료** (배너 → 카메라 복귀 `EndEncounter`, 레벨 이동/결과 화면 없음)
- 무력화(스태거) 게이지: 화면 상단 체력바에 붙이지 않고 **보스 메쉬 아래(발밑, 월드 공간)에 표시**
- 백헤드 표시: C++ 컴포넌트(`UBackHeadDecalComponent`)는 완성 → **데칼 머티리얼 제작이 남은 작업** (가이드: §2-3)

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

---

## 2. 보스 UI (체력바 · 무력화 게이지 · 백헤드 데칼)

### 2-1. 화면 상단 체력바

**현황 (C++ 준비 완료 — 위젯만 만들면 됨)**

- `ABossBase::OnBossHealthChanged(NewHealth, MaxHealth)` — 서버/클라 모두 방송되는 델리게이트. 위젯이 여기 바인딩.
- `ABossBase::TotalHealthBars`(기본 500줄), `GetCurrentHealth()` / `GetMaxHealthValue()` — 위젯 최초 구성용.
- `UBossHealthBarLibrary` (BlueprintPure 3종):
  - `GetHealthFraction` — 퍼센트 텍스트용 (0~1)
  - `GetBarsRemaining` — `xN줄` 표기
  - `GetCurrentBarFill` — 지금 깎이는 '한 줄'의 0~1 채움 (줄 경계마다 1.0 리필, 위젯에서 보간하면 피 닳는 연출)

**남은 작업**

- [ ] **WBP_BossHealthBar 제작**: 화면 상단 고정. 구성 = 보스 이름 + 체력바(한 줄 채움) + `xN` 줄 수 + 퍼센트. `OnBossHealthChanged` 바인딩, 표시 값은 위 라이브러리 함수로 계산.
- [ ] **표시/숨김 타이밍**: 조우 시작 시 추가, 보스 사망/조우 종료 시 제거. 클라가 조우 상태를 알 방법이 현재 없음 → §3 에서 `ABossRaidGameState` 에 조우 상태 복제 추가할 때 함께 처리 (임시로는 레벨에 보스 존재 시 표시로 시작해도 됨).
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

---

## 3. 보스 사망 → 클리어 (코드 작업 — 미구현)

### 현황

- `UBossAttributeSet` 은 Health 를 0~Max 로 **clamp만** 한다. **0 도달 시 아무 일도 일어나지 않음.**
- `LostArkTags::State_Dead` 태그는 존재 (플레이어 낙사에 사용 중).
- `ABossRaidGameMode::EndEncounter()` 는 카메라 복귀만 한다.
- `ABossBase::OnHealthChanged` 가 이미 체력 변화를 받아 페이즈 전환 판단에 쓰고 있음 → 사망 감지를 여기에 붙이는 게 자연스럽다.

### 남은 작업 (권장 구현 순서)

- [ ] **사망 감지 (서버)**: `ABossBase::OnHealthChanged` 에서 `NewValue <= 0` && 아직 미사망 → `HandleDeath()` 진입 (1회 가드).
- [ ] **사망 처리 `ABossBase::HandleDeath()` (서버)**:
  - `State.Dead` 복제 루스 태그 부여 (`UBossCombatStatics::AddReplicatedLooseTag`)
  - 패턴 정지: `BossPatternComponent` 중단 + 재생 중 몽타주 Stop + 카운터/저스트가드 창 닫기
  - 월드 정리: 살아있는 `ABossPatternActorBase` 전부 `DestroyAoeNow()` (잡힌 플레이어는 `OnEndPlay` 안전 복구가 이미 처리), 기믹 타워 정리, 무력화 페이즈 종료
  - 캡슐 콜리전 off (시체 관통 허용), 타겟팅/전하 공명 타이머 정지
- [ ] **죽는 모션**: **사망 몽타주 에셋 있음(확정)** → `HandleDeath` 에서 멀티캐스트로 몽타주 재생, 마지막 프레임에서 정지(몽타주 끝 섹션 루프 또는 `SetPlayRate(0)`). 시체는 유지.
- [ ] **클리어 상태 복제**: `ABossRaidGameState` 에 `bBossCleared`(또는 조우 상태 enum: 대기/전투/클리어) `ReplicatedUsing` 추가 + 방송 델리게이트 → 클라 UI/연출이 구독. §2 의 체력바 표시/숨김도 이 상태로 처리.
- [ ] **GameMode 체인**: 보스 사망 통지(태그/델리게이트) → `BossRaidGameMode` 가 클리어 상태 세팅 → 클리어 연출(§4) → 연출 종료 후 `EndEncounter()`.

> 협업 경계 주의: 플레이어 쪽 클래스는 직접 참조 금지. 보스 사망을 플레이어/UI 에 알리는 건
> **GameState 복제 + 태그**로만 넘긴다 (docs/00 원칙 1).

---

## 4. 클리어 연출 — **카메라 연출 + 위젯 방식 확정**

레벨 시퀀스 없음. 기존 `ABossArenaCamera` 를 활용한 코드/BP 연출 + 클리어 배너 위젯.
클리어 후에는 **그 자리에서 종료** (레벨 이동/결과 화면 없음).

### 남은 작업 (권장 순서)

- [ ] **연출 시퀀스 구현 (서버 주도, 카메라는 각 클라)**:
  1. 보스 사망(§3 `HandleDeath`) → 사망 몽타주 재생 시작
  2. 전 플레이어 뷰타겟을 보스 쪽으로 블렌드 (기존 `SetViewTargetWithBlend` 패턴 재사용 — 보스 줌인용 임시 카메라 or ArenaCamera 파라미터 조절)
  3. (선택) 글로벌 슬로모: `SetGlobalTimeDilation(0.3)` 1~2초 후 복구 — 서버에서 걸면 복제 환경에서도 안전
  4. 사망 몽타주 종료 시점 → 클라 전원 **클리어 배너 위젯** 표시 (GameState 클리어 복제 플래그에 클라가 반응 — §3)
  5. 배너 수초 유지 → `EndEncounter()` 호출 (카메라 각자 캐릭터 복귀. 기존 구현 재사용)
- [ ] **WBP_RaidClear 제작**: "클리어" 배너 + 사운드. (선택) 클리어 타임 = GameState 에 조우 시작 시각 기록해 계산.
- [ ] **연출 중 입력 잠금**: 클리어 상태 동안 플레이어 입력/스킬 잠금 여부를 플레이어 파트와 협의 — 잠근다면 복제 태그(예: `State.Raid.Cleared`)로 넘긴다 (직접 참조 금지).

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
