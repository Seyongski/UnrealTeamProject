# 02. 아키텍처 — 컴포넌트 기반 + 결합도 최소화

목표: **각자 작업물이 서로에게 컴파일/실행 의존을 거의 주지 않게** 한다.
그래야 먼저 끝낸 사람이 다른 파트에 자연스럽게 합류할 수 있다.

---

## A. 결합도 최소화 3대 도구

### 1) 인터페이스(UInterface)로만 경계를 넘는다
남의 구체 클래스를 참조하는 대신, 공용 인터페이스를 통해 통신한다.
공용 인터페이스는 `Content/_Project/Core` (C++: `Source/.../Core/Interfaces`)에 둔다.

권장 공용 인터페이스:
| 인터페이스 | 용도 | 예시 함수 |
|-----------|------|-----------|
| `ICombatTarget` | 데미지 대상 | `GetASC()`, `IsAlive()`, `GetTeam()` |
| `IInteractable` | 상호작용(문/NPC/포탈) | `Interact(Instigator)` |
| `IDamageable` | 데미지 수신 훅(비-GAS 폴백) | `ApplyDamage(...)` |
| `IStageActor` | 레벨/스테이지 이벤트 반응 | `OnStageBegin()`, `OnStageEnd()` |

```cpp
// ❌ 나쁨: 남의 클래스에 직접 의존
if (AWarrior* W = Cast<AWarrior>(Target)) { W->Health -= Dmg; }

// ✅ 좋음: 인터페이스로 의존 제거
if (Target->Implements<UCombatTarget>())
    ICombatTarget::Execute_ApplyGameplayEffectToSelf(Target, DamageEffect);
```

### 2) GameplayTag 로 상태/식별을 표현한다
enum·bool 하드코딩 대신 태그로. 태그 사전은 `GAS/Tags`에서 공용 관리.
- 예: `State.Dead`, `State.Stunned`, `Class.Warrior`, `Ability.Skill.Fireball`
- "마법사는 이런 상태" 같은 판단을 남의 클래스 몰라도 태그로 할 수 있다.

### 3) 데이터 주도(Data-Driven) 설계
수치/밸런스/스킬 구성은 코드가 아니라 `DataAsset`/`DataTable`에 둔다.
- 캐릭터 스탯, 스킬 목록, 몬스터 스폰 테이블, 드랍 테이블 등.
- 밸런싱은 코드 재컴파일 없이 데이터만 바꿔서 진행 → 담당 간 간섭 0.

---

## B. 컴포넌트 조립 원칙

캐릭터/몬스터는 얇은 `Pawn/Character` 껍데기에 컴포넌트를 조립한다.

```
ACharacterBase (Core, main에 선반영)
 ├─ UAbilitySystemComponent      (GAS)
 ├─ U<Class>AttributeSet         (스탯)
 ├─ UStatusComponent             (상태/사망 처리, 태그 반응)
 ├─ UTargetingComponent          (타겟 탐색)
 ├─ UInteractionComponent        (상호작용)
 └─ UTeamComponent               (팀/적아 구분)
```
- **새 기능 = 새 컴포넌트.** 캐릭터 클래스 자체는 최대한 비운다.
- 컴포넌트는 자기 데이터를 캡슐화하고, 외부와는 델리게이트/인터페이스로 통신한다.

---

## C. 사용할 디자인 패턴 (판단 근거 포함)

| 패턴 | 어디에 | 왜 |
|------|--------|-----|
| **Component (Composition)** | 캐릭터/몬스터 전반 | 상속 폭발 방지, 기능 재사용, 담당 분리 |
| **Data-Driven (DataAsset/Table)** | 스탯/스킬/스폰/밸런스 | 코드-데이터 분리, 밸런싱 병렬화 |
| **State 패턴** | 보스 페이즈, 몹 AI | 페이즈/행동 전환을 격리 (State Tree/BT로 구현) |
| **Observer(델리게이트/GameplayMessage)** | 사망·스테이지·UI 갱신 | 발신자와 수신자 분리 → 참조 제거 |
| **Factory / Spawner** | 몬스터·투사체 생성 | 생성 지식을 한곳에, 데이터로 무엇을 만들지 결정 |
| **Object Pool** | 투사체/이펙트 | 런타임 스폰 비용·GC 스파이크 감소 |
| **Strategy(=GameplayAbility)** | 직업별 스킬 | 스킬을 교체 가능한 단위로. GAS가 곧 Strategy |
| **Subsystem(Singleton 대체)** | 스테이지 매니저, 파티 매니저 | 전역 접근을 안전하게. 직접 싱글턴 금지 |

> **Observer 통신은 `GameplayMessageSubsystem`(Lyra식) 또는 태그 기반 델리게이트를 권장.**
> UI는 캐릭터를 몰라도 "체력 변경 메시지"만 구독하면 된다 → UI 담당과 캐릭터 담당이 분리된다.

---

## D. "먼저 끝낸 사람이 돕는" 것을 가능하게 하는 규칙
1. 공용 계약(인터페이스/태그/데이터 스키마)을 **1주차에 먼저 확정**한다.
2. 각 파트는 이 계약만 지키면 내부 구현을 자유롭게 바꿔도 남에게 영향 없음.
3. 그래서 합류할 때 "인터페이스만 보면" 파악 가능 → 온보딩 비용 최소.

---

## E. 모듈/폴더 경계
- C++: `Source/<Project>/` 아래 `Core / Characters / Enemies / GAS / UI / Level / Net` 로 폴더 분리.
- 공용(`Core`, `GAS/Tags`, 인터페이스) 변경은 **팀 합의 후** 반영.
- 자기 폴더 내부는 자유. 남의 폴더는 읽기만.
