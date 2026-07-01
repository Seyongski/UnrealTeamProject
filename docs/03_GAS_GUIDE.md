# 03. GAS(Gameplay Ability System) 가이드

GAS는 **필수**다. 전투/스탯/스킬/버프/상태는 전부 GAS로 만든다.

---

## 0. 플러그인/모듈 준비
- `GameplayAbilities`, `GameplayTags`, `GameplayTasks` 플러그인 활성화.
- `Build.cs`:
```csharp
PublicDependencyModuleNames.AddRange(new[] {
    "GameplayAbilities", "GameplayTags", "GameplayTasks"
});
```
- 프로젝트 시작 시 `UAbilitySystemGlobals::Get().InitGlobalData()` 1회 호출.

---

## 1. 구성 요소별 소유 규칙

| 요소 | 무엇 | 규칙 |
|------|------|------|
| **ASC** (`AbilitySystemComponent`) | 능력/효과 허브 | 플레이어: `PlayerState`에 배치(사망/리스폰에도 유지, 멀티 대비). 몬스터/보스: `Pawn`에 배치. |
| **AttributeSet** | 스탯(Health, Mana, AttackPower...) | 직업/몬스터별 AttributeSet. 값 변경은 GE로만. |
| **GameplayAbility (GA)** | 스킬/행동 | 스킬 1개 = GA 1개. 직업 담당이 자기 GA만 관리. |
| **GameplayEffect (GE)** | 데미지/힐/버프/디버프 | 수치 변화는 100% GE로. 코드에서 직접 어트리뷰트 대입 금지. |
| **GameplayCue** | 이펙트/사운드 반응 | 시각/청각 연출은 Cue로 분리(네트워크 자동 처리). |
| **GameplayTag** | 상태/분류/쿨다운 | 태그 사전은 `Config/Tags` 또는 `GAS/Tags`에서 공용 관리. |

---

## 2. 어트리뷰트 규칙
- 데미지·힐 계산은 `AttributeSet::PostGameplayEffectExecute` 또는 `ExecutionCalculation`에서.
- 예: `IncomingDamage`(meta) → Health 감소 → 0 이하면 `State.Dead` 태그 부여.
- `Health -= X` 를 게임플레이 코드 어디서도 직접 쓰지 않는다.

```cpp
// AttributeSet 안에서만 클램프/사망 처리
void UCharacterAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
    {
        const float Dmg = GetIncomingDamage();
        SetIncomingDamage(0.f);
        SetHealth(FMath::Clamp(GetHealth() - Dmg, 0.f, GetMaxHealth()));
        if (GetHealth() <= 0.f) { /* State.Dead 태그 부여 → StatusComponent가 반응 */ }
    }
}
```

---

## 3. 태그 네이밍 컨벤션

```
State.*        State.Dead, State.Stunned, State.Silenced, State.Invulnerable
Class.*        Class.Warrior, Class.Mage, Class.Healer
Ability.*      Ability.Skill.Fireball, Ability.Skill.Taunt
Cooldown.*     Cooldown.Skill.Fireball
Cue.*          Cue.Hit, Cue.Heal, Cue.LevelUp
Boss.Phase.*   Boss.Phase.1, Boss.Phase.2, Boss.Phase.Enrage
Data.*         Data.Damage, Data.HealAmount   (SetByCaller 키)
```
- 새 태그 추가 시 공용 태그 파일에 추가하고 팀에 공지(중복/오타 방지).

---

## 4. 직업 스킬 = GameplayAbility (Strategy)
- 스킬은 서로 독립된 GA. 직업 담당은 **자기 GA만** 만든다 → 서로 간섭 없음.
- 데미지/힐 수치는 GA가 아니라 GA가 적용하는 **GE + SetByCaller(Data.*)** 로 주입.
- 쿨다운/코스트도 GE(`Cooldown.*`, Mana 코스트)로 처리.

---

## 5. 네트워크(데디케이트 서버) 관점 — 지금부터 지킬 것
- ASC의 **Replication Mode**: 플레이어 = `Mixed`, AI/몬스터 = `Minimal`.
- GA는 서버 권위로 실행. 예측이 필요하면 `LocalPredicted` 정책 사용.
- 어트리뷰트/태그 변화는 GAS가 자동 리플리케이트 → **직접 RPC로 스탯 동기화하지 말 것.**
- 연출은 GameplayCue로 → 서버가 멀티캐스트 없이 자동 전파.
- 상세 → [06_NETWORKING](06_NETWORKING.md)

---

## 6. 체크리스트
- [ ] 스탯 변경을 GE로만 했는가
- [ ] 스킬을 GA로 만들었는가 (코드 하드코딩 아님)
- [ ] 수치를 SetByCaller/DataAsset로 뺐는가
- [ ] 태그 네이밍 규칙을 지켰는가
- [ ] 연출을 GameplayCue로 분리했는가
- [ ] 플레이어 ASC를 PlayerState에 두었는가(멀티 대비)
