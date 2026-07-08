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

### 특수 스킬 베이스 (Special Abilities)
* `LostArkSkill_Targeting.h/cpp` & `LostArkTargetActor_GroundSelect.h/cpp`
  * **설명**: 지정한 마우스 위치(장판)에 데미지를 입히는 타겟팅 스킬 베이스와 바닥 데칼(장판) 액터입니다.
* `LostArkSkill_Projectile.h/cpp` & `LostArkProjectile.h/cpp`
  * **설명**: 스킬 사용 시 전방으로 날아가는 투사체(발사체)를 스폰하는 스킬 베이스와 투사체 물리/폭발 액터입니다.
* `LostArkSkill_Charging.h/cpp`
  * **설명**: 스킬 키를 꾹 누르는 시간(모으기)에 비례하여 데미지가 증폭되는 차징 스킬 베이스입니다.

### 인터페이스 및 타입 (Interfaces & Types)
* `LostArkCombatInterface.h`
  * **설명**: 전투 상태 변경 및 죽음 판정 등을 다른 클래스와 결합도(Coupling) 없이 통신하기 위한 인터페이스입니다.
* `LostArkAbilityTypes.h`
  * **설명**: `ELostArkAbilityInputID` (키 입력 식별자) 및 `FLostArkSkillInputBind` (입력-스킬 매핑 구조체) 등 데이터 바인딩을 위한 필수 구조체가 정의되어 있습니다.

### 애니메이션 노티파이 (Animation Utilities)
* `LostArkAnimNotify_HitCheck.h` / `LostArkAnimNotify_HitCheck.cpp`
  * **설명**: 몽타주 재생 중 정확한 타격 시점(Hit)에 데미지 판정을 내리기 위한 노티파이입니다. 몽타주 타임라인에 배치하면 어빌리티 내부로 `Gameplay.Event.HitCheck` 이벤트를 전송하여 데미지를 적용시킵니다.

### 몬스터 및 스포너 시스템 (Monsters & Spawners)
* `LostArkMonster.h` / `LostArkMonster.cpp`, `LostArkMonsterAttackAbility.h` / `.cpp`
  * **설명**: 상태머신(Spawn, Idle, Move, Attack, Dead)이 구현된 몬스터 베이스와 몬스터 전용 평타 어빌리티입니다.
* `LostArkAIController.h` / `LostArkAIController.cpp`
  * **설명**: 몬스터에 부착되어 플레이어를 탐지하고 거리에 따라 이동/공격을 판단하는 범용 AI 컨트롤러입니다.
* `LostArkMonsterSpawner.h/cpp`, `LostArkObjectPoolSubsystem.h/cpp`
  * **설명**: 몬스터를 지정된 위치 주변에 반복 스폰하는 스포너와, 생성(Spawn)/파괴(Destroy) 대신 메모리를 껐다 켜서 재사용하는 **오브젝트 풀링 최적화 시스템**입니다.

---

## 📖 2. 팀원용 베이스 사용 메뉴얼

새로운 캐릭터나 스킬을 추가할 때는 코딩(C++) 없이 **Blueprint**만으로 작업(Data-Driven)을 진행할 수 있도록 설계되었습니다. 아래의 단계별 워크플로우를 따라주세요.

### Step 1. 신규 캐릭터 생성하기
1. 콘텐츠 브라우저에서 우클릭 -> **Blueprint Class** 생성.
2. 부모 클래스로 `LostArkCharacter`를 선택합니다. (예: `BP_Warrior`)
3. 생성된 블루프린트를 열고, `Mesh` 컴포넌트에 원하는 Skeletal Mesh와 AnimInstance를 할당합니다.

### Step 2. 신규 스킬(어빌리티) 제작하기
1. 부모 클래스로 **`LostArkSkillGameplayAbility`**를 선택하여 새로운 Blueprint 스킬(예: `GA_Fireball`)을 생성합니다. (※ 구버전 `LostArkGameplayAbility`를 직접 상속받지 마세요!)
2. 해당 블루프린트의 **Class Defaults(디테일 패널)** 에서 기획에 맞게 수치를 세팅합니다.
   * `Damage Shape Params`: 타격 판정 형태(Sphere, Box, Cone)와 크기(Radius, Extent 등) 및 데미지 계수를 설정합니다. 
     - **`ZTolerance`**: 오르막길이나 계단 등 높낮이가 다른 곳에서 몬스터가 맞지 않는 현상을 방지하는 Z축 상하 판정 허용 오차입니다. 기본값 200.
     - 디버그 라인을 켜고 끌 수 있습니다.
   * `Damage Effect Class`: 타격 시 적용할 데미지 이펙트(GE)
   * `Skill Montage`: 스킬 사용 시 재생할 애니메이션 몽타주
   * `Dash`: 스킬 사용 중 앞으로 돌진할지 여부와 거리, 시간 등을 설정합니다.

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

## 👾 3. 몬스터 및 스포너 시스템 사용 가이드

몬스터 역시 C++ 코드 수정 없이 블루프린트와 디테일 패널에서 데이터 기반으로 몬스터 군단을 쉽게 만들 수 있습니다.

### Step 1. 신규 몬스터 생성 및 세팅
1. 부모 클래스로 `LostArkMonster`를 상속받는 블루프린트를 생성합니다. (예: `BP_Goblin`)
2. Mesh와 AnimInstance를 할당합니다.
3. 블루프린트 디테일 패널에서 **Monster | Settings** 카테고리를 찾습니다.
   * `Spawn Duration`: 스폰 애니메이션이 끝날 때까지 대기하는 시간(무적/이동불가 상태)
   * `Base Attack Range`: 몬스터의 공격 사거리 (이 사거리 내에 플레이어가 오면 공격합니다)
4. AI 컨트롤러 할당: **Pawn** 카테고리의 `AI Controller Class`를 `LostArkAIController`로 지정합니다.

### Step 2. 몬스터 공격 어빌리티 생성
1. 부모 클래스로 `LostArkMonsterAttackAbility`를 상속받아 몬스터 평타 어빌리티(예: `GA_GoblinAttack`)를 생성합니다.
2. Step 1에서 만든 몬스터 블루프린트를 열어, **Monster | Abilities** 카테고리의 `Attack Ability Class`에 해당 어빌리티를 지정해 줍니다.

### Step 3. 월드에 몬스터 스포너 배치
1. 부모 클래스로 `LostArkMonsterSpawner`를 상속받는 블루프린트를 생성합니다. (예: `BP_GoblinSpawner`)
2. 만든 스포너를 레벨(맵) 위에 드래그 앤 드롭으로 배치합니다.
3. 스포너의 디테일 패널에서 스폰 규칙을 세팅합니다:
   * `Monster Class`: 이 스포너가 생성할 몬스터 블루프린트 지정 (예: `BP_Goblin`)
   * `Total Spawn Limit`: 총 몇 마리를 스폰할 것인지 (예: 50마리)
   * `Spawn Count Per Batch`: 한 번 스폰될 때 몇 마리씩 무리지어 나올지 (예: 3마리)
   * `Max Active Monsters`: 월드에 동시에 존재할 수 있는 최대 마릿수 (예: 10마리로 설정하면 10마리가 될 때까지 스폰하다가 죽으면 다시 채웁니다)
   * `Spawn Radius`: 스포너를 중심으로 어느 반경 내에 무작위로 스폰될지 지정
   * `Spawn Interval`: 스폰 쿨타임 (초 단위)

> **💡 최적화 주의사항 (Object Pooling)**: 몬스터는 스폰/사망 시 렉을 유발하지 않도록 백그라운드 풀링(Pooling) 시스템을 자동으로 탑재하고 있습니다. 따라서 몬스터가 죽을 때 Blueprint 노드에서 임의로 `Destroy Actor`를 호출하지 마세요. 몬스터 베이스의 죽음 판정 로직이 끝나면 자동으로 풀(Pool)로 회수되어 재사용 대기 상태로 전환됩니다.

---

## 🔮 4. 특수 스킬 시스템 (타겟팅, 투사체, 차징) 사용 가이드

단순히 몽타주를 재생하고 전방에 데미지를 주는 스킬 외에, 특수한 로직을 가진 스킬들을 만들 수 있는 베이스가 추가되었습니다.

### 🎯 4.1 타겟팅(장판) 스킬 만들기
마우스 커서를 따라다니는 장판 범위를 표시하고, 클릭 시 해당 위치로 스킬을 시전합니다.
1. 부모 클래스로 **`LostArkSkill_Targeting`**을 상속받아 스킬 블루프린트를 생성합니다.
2. 디테일 패널의 **Targeting** 카테고리에서 `Target Actor Class`를 할당해야 합니다. (기본 제공되는 `LostArkTargetActor_GroundSelect` 혹은 이를 상속받은 커스텀 블루프린트)
3. 해당 타겟 액터 블루프린트에서 `Target Radius`를 조절해 데칼(장판)의 크기를 조절할 수 있습니다.

### ☄️ 4.2 발사체(투사체) 스킬 만들기
전방으로 날아가는 마법학 발사체 등을 스폰합니다.
1. 부모 클래스로 **`LostArkProjectile`**을 상속받는 발사체 블루프린트(예: `BP_FireballProjectile`)를 먼저 생성합니다. 해당 블루프린트에서 스피드, 모델링, 폭발 파티클, 폭발 반경(`Explode Radius`) 등을 세팅합니다.
2. 부모 클래스로 **`LostArkSkill_Projectile`**을 상속받아 스킬 블루프린트를 생성합니다.
3. 디테일 패널의 **Projectile** 카테고리에서 `Projectile Class`에 방금 만든 발사체 블루프린트를 할당합니다.

### ⏳ 4.3 차징(모으기) 스킬 만들기
스킬 키를 누르는 시간에 비례해 스킬의 데미지가 증폭됩니다.
1. 부모 클래스로 **`LostArkSkill_Charging`**을 상속받아 스킬 블루프린트를 생성합니다.
2. 디테일 패널의 **Charging** 카테고리에서 수치를 세팅합니다:
   * `Max Charge Time`: 최대 충전 도달 시간(초)
   * `Max Damage Multiplier`: 최대 충전 시 적용될 데미지 증폭 배수 (예: 2.0 = 데미지 2배)

---

## 🛠 5. 설계 의도 및 주의사항
* **핵심 로직 분리**: 전투 판정 및 상태 동기화 같은 복잡한 연산은 C++ 베이스에 모두 구현되어 있습니다. Blueprint에서는 오직 데이터 세팅과 비주얼 연출에만 집중하시면 됩니다.
* **하드코딩 지양**: 캐릭터 블루프린트의 `SkillInputBinds`를 통해 데이터 기반으로 스킬이 관리됩니다. 코드를 수정할 필요 없이 인스펙터에서 스킬을 자유롭게 교체하고 테스트하세요.
