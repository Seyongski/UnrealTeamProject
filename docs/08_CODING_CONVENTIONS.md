# 08. 코딩 & 네이밍 컨벤션

Epic 표준을 따른다. 팀 병합 충돌과 온보딩 비용을 줄이는 게 목적.

---

## C++ 네이밍 (언리얼 표준)
| 종류 | 접두사 | 예 |
|------|--------|----|
| Actor 파생 | `A` | `AWarrior`, `AStageManager` |
| UObject 파생 | `U` | `UStatusComponent`, `UCharacterDataAsset` |
| ActorComponent | `U` + `Component` 접미 | `UTargetingComponent` |
| 인터페이스 | `U`(리플렉션)/`I`(구현) | `UCombatTarget` / `ICombatTarget` |
| 구조체 | `F` | `FStageInfo` |
| 열거형 | `E` | `EEnemyRole` |
| 템플릿 | `T` | `TArray` |
| bool 변수 | `b` | `bIsDead` |

- 함수/변수: PascalCase. 지역변수도 PascalCase(언리얼 관례).
- 불필요한 `Tick` 금지(필요할 때만 켠다).
- `TObjectPtr<>` 사용(UE5 권장), 원시 포인터 멤버 지양.

## 블루프린트 에셋 접두사
| 종류 | 접두사 | 예 |
|------|--------|----|
| Blueprint | `BP_` | `BP_Warrior` |
| Widget BP | `WBP_` | `WBP_HUD` |
| GameplayAbility | `GA_` | `GA_Fireball` |
| GameplayEffect | `GE_` | `GE_Damage`, `GE_Heal` |
| GameplayCue | `GC_` | `GC_Hit` |
| AttributeSet | `AS_` | `AS_Warrior` |
| DataAsset | `DA_` | `DA_WarriorStats` |
| DataTable | `DT_` | `DT_ChaosSpawn` |
| Material / Instance | `M_` / `MI_` | `M_Boss`, `MI_Boss_Red` |
| Texture | `T_` | `T_Warrior_D` |
| Static/Skeletal Mesh | `SM_` / `SK_` | `SK_Boss` |
| Niagara | `NS_` | `NS_Explosion` |
| Sound(Cue/Wave) | `SC_` / `SW_` | `SC_Hit` |
| Animation BP / Montage | `ABP_` / `AM_` | `ABP_Warrior`, `AM_Slash` |
| Level(Map) | `L_` | `L_Tutorial`, `L_ChaosDungeon`, `L_Raid` |

## 폴더 규칙 (재게시)
```
Content/_Project/
  Core/  Characters/{Warrior,Mage,Healer}/  Enemies/{Common,Boss}/
  Levels/{Tutorial,ChaosDungeon,Raid}/  UI/
  GAS/{Abilities,Effects,Attributes,Tags}/  Data/
Content/ThirdParty/   <- 외부/마켓 에셋(수정 금지)
```
- **자기 폴더 안에서만** 에셋 생성/수정. 공용은 합의.

---

## 코드 스타일
- 헤더 최소 포함(전방 선언 우선) → 컴파일 의존/시간 감소, 결합도↓.
- 남의 모듈 헤더 include 최소화. 경계는 인터페이스로([02_ARCHITECTURE](02_ARCHITECTURE.md)).
- 매직 넘버 금지 → DataAsset/const.
- 주석은 "왜"를 남긴다(무엇은 코드가 말한다).

## 블루프린트 vs C++
- **핵심/공용 시스템(Core, GAS 베이스, 네트워크)** = C++.
- **콘텐츠/연출/밸런스 조립(스킬 세부, UI, 레벨 스크립트)** = 블루프린트 허용.
- BP는 노드 스파게티 금지: 함수/매크로로 정리, 로직 큰 건 C++로 승격.

## 커밋/PR
- Conventional Commits + PR 체크리스트([01_GIT_WORKFLOW](01_GIT_WORKFLOW.md)).
