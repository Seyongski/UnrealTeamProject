# 04. 직업 설계 — 전사 / 마법사 / 힐러

모든 직업은 공용 `ACharacterBase`(Core, `main` 선반영)를 상속하고,
**차이는 상속이 아니라 AttributeSet + GA(스킬) + DataAsset(스탯)** 로 만든다.

---

## 공통 규격 (모든 직업이 지킴)

- 상속: `ACharacterBase` → `AWarrior/AMage/AHealer` (껍데기, 로직 최소)
- 스탯: 직업별 `AttributeSet` (Health, Mana/Resource, AttackPower, Defense, ...)
- 초기값: `UCharacterDataAsset` (스탯 테이블) — 코드에 하드코딩 금지
- 스킬: `GameplayAbility` 세트 + `GameplayTag`(`Class.*`)
- 인터페이스: `ICombatTarget` 구현(데미지/생존/팀 조회)
- 상태: `State.*` 태그 + `UStatusComponent` 반응

> 캐릭터 담당 2명은 3직업을 나눠 맡되(예: A=전사+힐러, B=마법사),
> **공용 CharacterBase / AttributeSet 베이스 / 공용 태그는 함께 1주차에 확정**한다.

---

## ⚔️ 전사 (Warrior) — 근접 탱커/딜러
- 자원: 분노(Rage) 또는 스태미나
- 역할: 어그로 관리, 근접 딜, 생존
- 예시 스킬(GA):
  - `Ability.Skill.Slash` — 근접 광역 베기 (GE: 물리 데미지)
  - `Ability.Skill.Taunt` — 도발, 적 AI 타겟을 자신으로 (`State.Taunted` 부여)
  - `Ability.Skill.ShieldWall` — 방어 태세 (`State.Invulnerable`/데미지 감소 GE, 지속)
- 태그: `Class.Warrior`

## 🔮 마법사 (Mage) — 원거리 폭딜
- 자원: 마나(Mana)
- 역할: 원거리 광역/단일 폭딜
- 예시 스킬(GA):
  - `Ability.Skill.Fireball` — 투사체(Object Pool 권장) + 폭발 광역 GE
  - `Ability.Skill.FrostNova` — 주변 슬로우/빙결 (`State.Chilled`/`State.Frozen`)
  - `Ability.Skill.Blink` — 순간이동(짧은 거리 텔레포트)
- 태그: `Class.Mage`

## 💚 힐러 (Healer) — 지원/치유
- 자원: 마나(Mana)
- 역할: 아군 회복, 버프, 상태이상 해제
- 예시 스킬(GA):
  - `Ability.Skill.Heal` — 단일 힐 (GE: Health 회복, SetByCaller `Data.HealAmount`)
  - `Ability.Skill.HealOverTime` — 지속 회복(HoT, Duration GE)
  - `Ability.Skill.Cleanse` — `State.*` 디버프 태그 제거
  - `Ability.Skill.Barrier` — 보호막(Shield 어트리뷰트)
- 태그: `Class.Healer`
- 참고: **힐/버프도 데미지처럼 GE로.** 아군 판별은 `UTeamComponent`/`ICombatTarget::GetTeam()`.

---

## 데이터 스키마(예시) — `UCharacterDataAsset`

| 필드 | 타입 | 설명 |
|------|------|------|
| ClassTag | GameplayTag | `Class.Warrior` 등 |
| BaseHealth / BaseMana | float | 기본 스탯 |
| AttackPower / Defense | float | 기본 전투 스탯 |
| GrantedAbilities | TArray<TSubclassOf<GameplayAbility>> | 부여 스킬 |
| Mesh / AnimBP | soft ref | 외형 (LFS 에셋) |

> 밸런싱은 이 DataAsset 값만 조정 → 코드/타 담당 영향 없음.

---

## 결합도 규칙 (직업 간)
- 마법사 담당이 힐러 클래스를 **직접 참조하지 않는다.** 필요하면 `ICombatTarget`/태그로.
- 파티/버프 대상 선정은 팀 컴포넌트 + 태그 쿼리로. "누가 힐러인지"는 `Class.Healer` 태그로 판별.
