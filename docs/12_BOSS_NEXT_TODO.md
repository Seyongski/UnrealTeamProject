# 12. BOSS NEXT TODO — 이번 작업 목록 (UI · 사망 · 패턴 진행)

> **목적**: 지금 이어서 할 보스 작업을 담당자 기준으로 정리한다.
> 상위 단일 진실 문서는 [`10_BOSS_WORKLIST.md`](10_BOSS_WORKLIST.md) — 이 문서는 그중
> "지금 당장 할 것"만 추려 현황/남은작업을 붙였다. 끝내면 체크박스를 채우고 커밋할 것.
> (코드 현황은 2026-07-24 기준 실제 소스 확인함.)

---

## 해야 할 것 (우선순위 순)

### 1. 보스 체력 UI — 줄 수 표기 + 어색한 부분 수정

**현황 (C++ 준비 완료 — WBP 작업만 남음)**

- C++ 베이스: `UBossHUDWidget` ([`Boss/UI/BossHUDWidget.h`](../LostArk/Source/LostArk/Boss/UI/BossHUDWidget.h)) — 컨트롤러가 `InitForBoss` 로 주입, 체력 변동 시 `OnBossHealthUpdated(NewHealth, MaxHealth)` (BlueprintImplementableEvent) 방송.
- 줄 수/퍼센트 계산: `UBossHealthBarLibrary` ([`Boss/UI/BossHealthBarLibrary.cpp`](../LostArk/Source/LostArk/Boss/UI/BossHealthBarLibrary.cpp))
  - `GetHealthFraction` (0~1 퍼센트)
  - `GetBarsRemaining` (`xN줄` 표기 — 살아있으면 최소 1줄)
  - `GetCurrentBarFill` (지금 깎이는 한 줄의 0~1 채움)
- 줄 수 기준값: `ABossBase::TotalHealthBars` (기본 **500줄**).

**남은 작업**

- [ ] `WBP_BossHUD` 를 `UBossHUDWidget` 으로 **Reparent** (`Content/Levels/UI/WBP_BossHUD`).
- [ ] 그래프에서 `OnBossHealthUpdated` 구현 → 라이브러리로 막대/`xN줄`/퍼센트 갱신 (줄 수 = `Boss->TotalHealthBars`).
- [ ] **줄 수 표기 방식 확정 + 어색한 부분 수정** — 예: `xN줄` 텍스트 위치/폰트, 막대와 숫자 정렬, 화면 상단 고정, 보스 이름 배치 등.
      → *구체적으로 뭐가 어색한지 담당자 메모 필요 (스샷/항목).*
- [ ] PlayerController BP 에 `BossHUDWidgetClass = WBP_BossHUD` 지정.

---

### 2. 보스 사망 — 피 0 시 전 패턴 중단 + 사망 몽타주

**현황: 코드 구현 완료** — 남은 건 **BP에 몽타주 지정뿐**.

`ABossBase::HandleDeath()` ([`Boss/BossBase.cpp`](../LostArk/Source/LostArk/Boss/BossBase.cpp)) 가 서버 권위·1회 가드로 다음을 처리:

1. 체력 0 감지(`OnHealthChanged`) → `HandleDeath()`
2. `State.Dead` 복제 태그 부여
3. **모든 패턴 중단**: `PatternComponent->StopCombat()` + `ASC->CancelAllAbilities()` (이후 패턴 재개 금지)
4. 무력화 페이즈 종료 + 남은 장판/타워 정리 + 이동 정지
5. **사망 몽타주 멀티캐스트 재생** (`MulticastPlayDeathMontage` → `DeathMontage` 재생, 종료 시 포즈 고정)
6. `OnBossDied` 방송 + `BossRaidGameMode::NotifyBossDied()` (클리어 연출)

**남은 작업**

- [ ] **보스 BP `Boss|Death` 카테고리의 `DeathMontage` 에 사망 몽타주 지정.**
      (미지정 시: `[Boss] DeathMontage 미지정` 로그 경고 + 현재 포즈만 유지)
- [ ] (선택) 사망 FX/사운드: `OnBossDied`(서버) 또는 `State.Dead` 태그(클라)에 BP 바인딩.

> 즉 이 항목은 **코드 추가 불필요, 에디터에서 몽타주 지정만** 하면 동작한다.

---

### 3. 보스 패턴 진행 — 지형파괴(지파) 단계별 선형 진행

**원하는 흐름**

```
[패턴 1개] → 기믹(1지파 파괴) → [패턴 1개] → 기믹(2지파) → [패턴 1개] → 기믹(3지파) → [패턴 1개]
```

총 **패턴 4개 / 기믹 3회**, 각 기믹이 슬라이스(지파)를 1개씩 파괴하며 다음 단계로.

**현재 구조 (그대로는 안 맞음)**

- `UBossPatternComponent` ([`Boss/Pattern/BossPatternComponent.cpp`](../LostArk/Source/LostArk/Boss/Pattern/BossPatternComponent.cpp)) 는
  **체력%(`EnterHealthPercent`) 기반 페이즈** + 각 페이즈 **가중치 랜덤 패턴 무한 반복** 구조.
- 페이즈 진입 시 **`TransitionGimmick` 먼저 실행 → 그 다음 일반 패턴** (즉 *기믹 → 패턴* 순).
- 지파 카운트는 이미 존재: `ABossRaidGameState::DestroyedSliceMask` (복제) + `GameMode::DestroySlice`.

**설계 결정 필요 (구현 전 확정할 것)**

- [ ] **진행 트리거**: 체력%가 아니라 **"패턴 1개 완료" 후 자동으로 다음 단계**로 넘어가게 할지.
- [ ] **순서**: 지금은 *기믹 → 패턴*. 원하는 건 *패턴 → 기믹* — 순서 교체 필요.
- [ ] **"패턴 1개"의 의미**: 단계별 **지정 패턴**인지, 풀에서 **랜덤 1개**인지.
- [ ] **연동 기준**: 단계 진행을 `DestroyedSliceMask`(지파 수)에 물릴지, 별도 카운터로 관리할지.

**구현 방향 (안, 택1)**

- (A) **페이즈 시스템 재활용**: 각 단계를 페이즈로 만들되 전환 조건을 "패턴 N개 완료"로, `TransitionGimmick` 을 지형파괴 기믹으로. (순서 교체 로직 추가)
- (B) **선형 시퀀스 모드 신규**: 패턴 컴포넌트에 `[패턴, 기믹, 패턴, 기믹, ...]` 스텝 리스트를 순서대로 도는 모드 추가.

> 이 항목만 **실제 C++ 로직 변경**이 필요. 위 결정 확정 후 착수.

---

## 시간 남을 시 할 것

- [ ] **패턴 다양화**: 페이즈별 `PatternDataAsset` 추가 (몽타주/장판/넉백 조합).
- [ ] **보스 백헤드 데칼**: `M_BossBackHead` 머티리얼 제작 — 상세 가이드는
      [`11_BACKHEAD_DECAL_TODO.md`](11_BACKHEAD_DECAL_TODO.md). (C++ `UBackHeadDecalComponent` 는 완성)
- [ ] **캐릭터 선택** (별도 파트)
- [ ] **서버 넣어주기** (배포/접속 — 별도 파트)

---

## 참고 (이미 된 것 / 관련 문서)

- 레벨 BGM 자동재생: `BossRaidGameMode.LevelBgm` → GameState 복제 → 각 클라 로컬 재생 (코드 완료, BP에 SoundWave 지정만).
- 보스 사망 → 클리어 연출: [`10_BOSS_WORKLIST.md`](10_BOSS_WORKLIST.md) §3~4 (코드 완료).
- 넉백/낙사 시스템: 같은 문서 §5 (사용법).
