# LostArk Character GAS Framework Guide

본 문서는 팀원들이 새로운 캐릭터와 스킬을 제작할 때 기반(Base)으로 사용하게 될 GAS(Gameplay Ability System) 프레임워크의 사용 가이드 및 배포 파일 목록입니다.

## 📦 1. 배포 대상 파일 목록 (C++ Core)

다음의 `.h` 및 `.cpp` 파일들은 게임 내 모든 캐릭터와 몬스터가 공유하는 핵심 전투 및 스킬 시스템의 기반입니다. 프로젝트의 `Source/LostArk/` 디렉토리에 위치해야 합니다.

### 캐릭터 및 스탯 (Character & Attributes)
* `LostArkCharacter.h` / `LostArkCharacter.cpp`
  * **설명**: ASC(Ability System Component)가 부착되어 있으며, 입력 바인딩과 전투 상태(Combat State)를 관리하는 기본 캐릭터 클래스입니다.
* `LostArkAttributeSet.h` / `LostArkAttributeSet.cpp`
  * **설명**: 체력, 최대 체력, 공격력, 공격 사거리 등 캐릭터의 공통 스탯(Attribute)을 정의합니다.

### 스킬 베이스 (Abilities)
* `LostArkGameplayAbility.h` / `LostArkGameplayAbility.cpp`
  * **설명**: 최상위 어빌리티 클래스입니다. `FDamageShapeParams`를 통해 구(Sphere), 상자(Box), 원뿔(Cone) 형태의 범용 데미지 판정을 지원하며, `Gameplay.Event.HitCheck` 이벤트를 수신해 타격을 적용합니다.
* `LostArkSkillGameplayAbility.h` / `LostArkSkillGameplayAbility.cpp`
  * **설명**: 위 클래스를 상속받는 **일반 스킬(Q,W,E,R 등)의 최상위 베이스**입니다. 몽타주 재생, 스킬 돌진(Dash), 쿨타임 및 코스트(자원 소모) 적용 등 범용 스킬 로직을 제공합니다.
* `LostArkComboSkillAbility.h` / `LostArkComboSkillAbility.cpp`
  * **설명**: 여러 번 키를 눌러 연계하는 **콤보형 액티브 스킬**을 만들 때 사용하는 베이스입니다.
* `LostArkCharacterComboAttackAbility.h` / `LostArkCharacterComboAttackAbility.cpp`
  * **설명**: 일반 스킬이 아닌 캐릭터의 **기본 평타(마우스 클릭) 콤보** 전용 어빌리티입니다. 캐릭터 베이스의 `Combo Attack Ability Class`에 할당되어 다단 히트를 처리합니다.

### 인터페이스 및 타입 (Interfaces & Types)
* `LostArkCombatInterface.h`
  * **설명**: 전투 상태 변경 및 죽음 판정 등을 다른 클래스와 결합도(Coupling) 없이 통신하기 위한 인터페이스입니다.
* `LostArkAbilityTypes.h`
  * **설명**: `ELostArkAbilityInputID` (키 입력 식별자) 및 `FLostArkSkillInputBind` (입력-스킬 매핑 구조체) 등 데이터 바인딩을 위한 필수 구조체가 정의되어 있습니다.

### 애니메이션 노티파이 (Animation Utilities)
* `LostArkAnimNotify_HitCheck.h` / `LostArkAnimNotify_HitCheck.cpp`
  * **설명**: 몽타주 재생 중 정확한 타격 시점(Hit)에 데미지 판정을 내리기 위한 노티파이입니다. 몽타주 타임라인에 배치하면 어빌리티 내부로 `Gameplay.Event.HitCheck` 이벤트를 전송하여 데미지를 적용시킵니다.

---

## 📖 2. 팀원용 베이스 사용 메뉴얼

새로운 캐릭터나 스킬을 추가할 때는 코딩(C++) 없이 **Blueprint**만으로 작업(Data-Driven)을 진행할 수 있도록 설계되었습니다. 아래의 단계별 워크플로우를 따라주세요.

### Step 1. 신규 캐릭터 생성하기
1. 콘텐츠 브라우저에서 우클릭 -> **Blueprint Class** 생성.
2. 부모 클래스로 `LostArkCharacter`를 선택합니다. (예: `BP_Warrior`)
3. 생성된 블루프린트를 열고, `Mesh` 컴포넌트에 원하는 Skeletal Mesh와 AnimInstance를 할당합니다.

### Step 2. 신규 스킬(어빌리티) 제작하기
1. 부모 클래스로 `LostArkGameplayAbility`를 선택하여 새로운 Blueprint 스킬(예: `GA_Fireball`)을 생성합니다.
2. `Event Activate Ability` 노드를 시작으로 애니메이션 몽타주 재생(`PlayMontageAndWait`), 파티클 스폰 등의 연출 로직을 구성합니다.
3. 해당 블루프린트의 **Class Defaults(디테일 패널)** 에서 기획에 맞게 수치를 세팅합니다.
   * `Damage Shape Params`: 타격 판정 형태(Sphere, Box, Cone)와 크기(Radius, Extent 등) 및 데미지 계수를 설정합니다. 디버그 라인을 켜고 끌 수 있습니다.
   * `Damage Effect Class`: 타격 시 적용할 데미지 이펙트(GE)
   * `Skill Montage`: 스킬 사용 시 재생할 애니메이션 몽타주
   * `Dash`: 스킬 사용 중 앞으로 돌진할지 여부와 거리, 시간 등을 설정합니다.
4. 로직이 끝나면 반드시 `End Ability` 노드를 연결해 스킬을 종료시켜 줍니다. (단, `LostArkSkillGameplayAbility`를 상속받은 경우 몽타주가 끝나면 자동으로 종료되도록 베이스에 구현되어 있습니다.)

### Step 3. 캐릭터에 스킬과 키 입력 연결하기
1. Step 1에서 만든 캐릭터 블루프린트(`BP_Warrior`)를 엽니다.
2. 디테일 패널에서 **Character | Abilities** 카테고리를 찾습니다.
3. `Combo Attack Ability Class` 항목에 기본 평타로 사용할 어빌리티(GA)를 지정합니다.
4. `Skill Input Binds` 배열 항목의 `+` 버튼을 눌러 스킬 슬롯을 추가합니다.
   * `Input Action`: 누를 키 (Enhanced Input Action)
   * `Input ID`: 내부 식별용 ID (Q, W, E, R 등)
   * `Ability Class`: Step 2에서 만든 발동시킬 스킬(GA) 클래스
5. 이 과정만 완료하면 게임 실행 시 입력한 키를 누를 때 해당 스킬이 자동으로 발동됩니다!

### Step 4. 초기 스탯(체력/공격력) 부여하기
1. 게임 시작 시 캐릭터의 기본 스탯을 결정하기 위해 `GameplayEffect`를 생성합니다. (Duration Policy: Instant)
2. Modifiers 배열에 Health, AttackDamage 등의 속성을 추가하고 원하는 기본값을 입력합니다.
3. 캐릭터 시작 시 해당 Effect를 자기 자신에게 적용(Apply)하도록 세팅하여 스탯을 초기화합니다.

---

## 🛠 3. 설계 의도 및 주의사항
* **핵심 로직 분리**: 전투 판정 및 상태 동기화 같은 복잡한 연산은 C++ 베이스에 모두 구현되어 있습니다. Blueprint에서는 오직 데이터 세팅과 비주얼 연출에만 집중하시면 됩니다.
* **하드코딩 지양**: 캐릭터 블루프린트의 `SkillInputBinds`를 통해 데이터 기반으로 스킬이 관리됩니다. 코드를 수정할 필요 없이 인스펙터에서 스킬을 자유롭게 교체하고 테스트하세요.
