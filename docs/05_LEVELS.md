# 05. 레벨 설계 — 튜토리얼 → 카오스 던전 → 레이드

레벨 담당(main-5)이 주도하되, 스테이지 흐름은 **공용 인터페이스/이벤트로만**
캐릭터·몹·보스 담당과 연결한다(서로 구체 클래스 모름).

---

## 스테이지 흐름 아키텍처

```
UStageSubsystem (GameInstanceSubsystem)  <- 전역 스테이지 진행 관리
   │  브로드캐스트: OnStageBegin / OnObjectiveUpdated / OnStageClear
   ▼
AStageManager (레벨에 배치)   <- 현재 레벨의 스폰/목표/클리어 조건
   ├─ ASpawner (Factory)      <- DataTable 기반 몬스터 스폰
   ├─ 목표(Objective) 규칙
   └─ 포탈/다음 레벨 전환
```
- 몬스터/보스/캐릭터는 `IStageActor`(`OnStageBegin/OnStageEnd`)만 구현하면 스테이지에 반응.
- 스폰은 **DataTable(무엇을/어디서/몇 마리)** 로 주도 → 몹 담당과 레벨 담당 분리.

---

## 1) 튜토리얼 (Tutorial)
- 목적: 이동/공격/스킬/GAS 기본 학습, 직업 선택.
- 구성: 좁은 선형 맵, 스크립트성 안내(UI 담당과 연동), 약한 더미 적 1~2종.
- 클리어 조건: 지정 행동 수행 → 포탈로 카오스 던전 이동.
- 네트워크: 싱글/멀티 공통 흐름으로 짜되 서버 권위 유지.

## 2) 카오스 던전 (Chaos Dungeon)
- 목적: 물량 전투. 일반 몬스터(main-4) 웨이브.
- 구성: 여러 방 + 웨이브 스폰(Spawner + DataTable), 처치 수/시간 목표.
- 몬스터: `Enemies/Common` — 근접/원거리/광역 등 역할 태그(`Enemy.Role.*`)로 구분.
- 클리어 조건: 전 웨이브 처치 or 목표 달성 → 레이드 입장 포탈.
- 확장 여지: 랜덤 방 조합/난이도 스케일(데이터로).

## 3) 레이드 (Raid)
- 목적: 보스전(main-3). 협동 필수.
- 구성: 아레나 맵 + 레이드 보스 + 기믹.
- 보스: `State 패턴 + Behavior Tree/State Tree`로 **페이즈** 전환.
  - `Boss.Phase.1/2/Enrage` 태그로 페이즈 표현, 페이즈별 패턴 GA 세트 교체.
  - 예: 광역 장판, 소환, 버서크(엔레이지 타이머).
- 클리어 조건: 보스 처치 → 결과/보상 UI.

---

## 레벨 ↔ 타 담당 계약(인터페이스)
| 이벤트 | 발신 | 수신 | 방식 |
|--------|------|------|------|
| 스테이지 시작 | StageManager | 캐릭터/몹/보스 | `IStageActor::OnStageBegin` |
| 목표 갱신 | StageManager | UI | GameplayMessage/델리게이트 |
| 몬스터 사망 집계 | 몹/보스 | StageManager | 사망 태그/이벤트 구독 |
| 스테이지 클리어 | StageManager | 전원/UI | `OnStageClear` 브로드캐스트 |

- 레벨 담당은 보스/몹의 **내부 구현을 몰라도** 위 계약만으로 스테이지를 구성한다.

---

## 맵/에셋 규칙
- `.umap`은 바이너리 → **동시 편집 금지**([01_GIT_WORKFLOW](01_GIT_WORKFLOW.md)의 LFS 잠금).
- 대형 맵은 **World Partition / Level Streaming / Sublevel**로 분할해 편집 충돌을 줄인다.
- 로직은 레벨 블루프린트에 몰아넣지 말고 `AStageManager`/Subsystem에 둔다(재사용·테스트·병합 용이).
