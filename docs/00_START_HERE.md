# 00. START HERE — 프로젝트 하네스(작업 규칙)

> 이 문서는 팀 전체가 반드시 지켜야 하는 **가드레일(하네스)** 이다.
> 작업 전 이 페이지만이라도 훑고, 상세는 링크된 문서를 본다.

---

## 🎯 우리가 지키는 5대 원칙

### 1. 참조(결합도)를 최소화한다 — 최우선
- 다른 팀원의 **구체 클래스(Concrete Class)를 직접 참조하지 않는다.**
- 협업 경계는 **인터페이스(UInterface) / GameplayTag / DataAsset / 델리게이트**로만 넘는다.
- 이유: 먼저 끝낸 사람이 다른 파트를 도우려면, 각자 작업물이 서로에게 컴파일/실행 의존을 주면 안 된다.
- 상세 → [02_ARCHITECTURE](02_ARCHITECTURE.md)

### 2. 컴포넌트 기반으로 조립한다
- 기능은 `ActorComponent`로 분리하고 캐릭터/몬스터는 컴포넌트를 **조립**해서 만든다.
- "상속으로 기능 추가" 대신 "컴포넌트로 기능 추가"를 기본으로.

### 3. GAS로 모든 전투 로직을 만든다 (필수)
- 스탯 = `AttributeSet`, 스킬 = `GameplayAbility`, 효과/버프/데미지 = `GameplayEffect`, 상태/조건 = `GameplayTag`.
- 캐릭터 클래스 코드에 데미지 계산·쿨다운을 하드코딩하지 않는다.
- 상세 → [03_GAS_GUIDE](03_GAS_GUIDE.md)

### 4. 처음부터 데디케이트 서버를 가정하고 짠다
- 지금 멀티를 구현하지 않아도, **모든 게임플레이 상태는 서버 권위(Server-authoritative)** 로 설계한다.
- `Replicated` / `RPC` 규칙을 처음부터 지켜 두면 나중에 서버 붙일 때 재작업이 없다.
- 상세 → [06_NETWORKING](06_NETWORKING.md)

### 5. 규칙을 지킨 것만 병합한다
- 아래 **PR 체크리스트**를 통과하지 못하면 병합 금지.
- 상세 → [01_GIT_WORKFLOW](01_GIT_WORKFLOW.md)

---

## ✅ 커밋/PR 전 필수 체크리스트

- [ ] 다른 팀원의 구체 클래스를 `#include` / 직접 캐스팅하지 않았다 (인터페이스/태그/데이터로 대체)
- [ ] 새 기능은 `ActorComponent` 또는 GAS(Ability/Effect)로 구현했다
- [ ] 하드코딩된 수치는 `DataAsset` / `DataTable` / `GameplayEffect` 로 뺐다
- [ ] 상태 변경은 서버에서 일어나고, 필요한 변수는 `Replicated` 로 표시했다
- [ ] 네이밍/폴더 규칙을 지켰다 ([08_CODING_CONVENTIONS](08_CODING_CONVENTIONS.md))
- [ ] `.uasset`/`.umap` 등 바이너리는 LFS로 추적되고 있다 (`git lfs status`)
- [ ] 자기 브랜치에서 작업했고, 대상 브랜치로 PR을 올렸다

전체 PR 템플릿 → [templates/PR_TEMPLATE.md](templates/PR_TEMPLATE.md)

---

## 🚦 하지 말아야 할 것 (Anti-patterns)

| 하지 마라 | 대신 이렇게 |
|-----------|-------------|
| `Cast<AWarrior>(Other)` 로 남의 클래스 참조 | `ICombatTarget` 인터페이스로 참조 |
| 캐릭터 코드에 `Health -= 10;` | `GameplayEffect`로 데미지 적용 |
| enum 으로 상태 하드코딩 후 여기저기 분기 | `GameplayTag` + Tag 반응 |
| `main` 에 직접 push | 자기 브랜치 → PR |
| 에셋을 각자 브랜치에서 대량 추가 | 에셋은 `main`에 통합 관리 |
| 몹/보스가 클라에서 죽음 판정 | 서버에서 판정 후 리플리케이트 |

---

## 🗂 프로젝트 폴더 규칙 (요약)

```
Content/
  _Project/            <- 우리 팀 작업물 (에픽/마켓 에셋과 분리)
    Core/              <- 공용 기반 (CharacterBase, 인터페이스, GAS 공용)
    Characters/
      Warrior/  Mage/  Healer/
    Enemies/
      Common/  Boss/
    Levels/
      Tutorial/  ChaosDungeon/  Raid/
    UI/
    GAS/
      Abilities/  Effects/  Attributes/  Tags/
    Data/              <- DataAsset / DataTable
  ThirdParty/          <- 마켓플레이스/외부 에셋 (건드리지 않음)
```
> 각 담당은 **자기 폴더 안에서만** 작업한다. 공용(`Core`, `GAS/Tags`)은 변경 전 팀 합의.

---

## ⏱ 기간

**2026-07-01 ~ 2026-07-26 (4주)** — 상세 마일스톤은 [07_SCHEDULE](07_SCHEDULE.md).
