// Fill out your copyright notice in the Description page of Project Settings.


#include "Boss/BossBase.h"
#include "Boss/BackHeadDecalComponent.h"
#include "Boss/BossAttributeSet.h"
#include "Boss/BossGameplayTags.h"
#include "Boss/Combat/BossCombatStatics.h"
#include "Boss/Combat/BossCounterComponent.h"
#include "Boss/Combat/BossJustGuardComponent.h"
#include "Boss/Damage/BossPatternActorBase.h"
#include "Boss/Gimmick/BossGimmickTower.h"
#include "Boss/Pattern/BossPatternComponent.h"
#include "Boss/Raid/BossRaidGameMode.h"
#include "Boss/Targeting/BossTargetingComponent.h"
#include "Boss/Gimmick/BossTerrainGimmickComponent.h"
#include "Boss/Weapon/BossWeaponComponent.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"

// Sets default values
ABossBase::ABossBase()
{
	// 기본 틱은 꺼두고(성능), 디버그 표시가 필요할 때만 BeginPlay에서 켠다
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// 백헤드 데칼: 캡슐(루트)에 부착 -> 보스 회전을 따라가며 앞=헤드 / 뒤=백 정렬
	BackHeadDecal = CreateDefaultSubobject<UBackHeadDecalComponent>(TEXT("BackHeadDecal"));
	BackHeadDecal->SetupAttachment(GetCapsuleComponent());

	// 데칼이 보스 몸에 묻지 않도록 메쉬는 데칼을 받지 않게 설정
	if (GetMesh())
	{
		GetMesh()->SetReceivesDecals(false);

		// 데디서버에서도 몽타주/노티파이/루트모션이 동작하도록 항상 포즈 틱
		// (기본값 OnlyTickPoseWhenRendered 면 서버에서 안 렌더 -> 노티파이 안 불림)
		GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}

	// GAS: ASC + 어트리뷰트 (다수 플레이어 대상 보스이므로 이펙트 복제는 Minimal)
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UBossAttributeSet>(TEXT("AttributeSet"));

	// 패턴 흐름 브레인
	PatternComponent = CreateDefaultSubobject<UBossPatternComponent>(TEXT("PatternComponent"));

	// 타겟 선정 + 추적 회전
	TargetingComponent = CreateDefaultSubobject<UBossTargetingComponent>(TEXT("TargetingComponent"));

	// 카운터 창/판정
	CounterComponent = CreateDefaultSubobject<UBossCounterComponent>(TEXT("CounterComponent"));

	// 저스트가드 창/판정
	JustGuardComponent = CreateDefaultSubobject<UBossJustGuardComponent>(TEXT("JustGuardComponent"));

	// 무기 착용 상태 표시 (맨손/양손/합체 토글)
	WeaponComponent = CreateDefaultSubobject<UBossWeaponComponent>(TEXT("WeaponComponent"));

	// 지형파괴 기믹 (미설정 시 아무것도 안 함 — 기믹 없는 보스도 안전)
	TerrainGimmickComponent = CreateDefaultSubobject<UBossTerrainGimmickComponent>(TEXT("TerrainGimmickComponent"));
}

UAbilitySystemComponent* ABossBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ABossBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// 서버: ActorInfo 초기화 -> 어트리뷰트 세팅 -> 체력 변화 구독
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);

		// 재빙의 시 어트리뷰트 리셋/중복 바인딩/전투 재시작 방지
		if (!bGASInitialized)
		{
			bGASInitialized = true;

			InitializeAttributes();

			AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UBossAttributeSet::GetHealthAttribute())
				.AddUObject(this, &ABossBase::OnHealthChanged);

			// ASC 초기화가 끝난 지금 시점에 어빌리티 부여 + 전투 시작
			// (전투 시작은 임시로 여기서. 이후 어그로/트리거 시점으로 옮기면 됨)
			if (PatternComponent)
			{
				PatternComponent->InitializePatterns();
				PatternComponent->StartCombat();
			}
		}
	}
}

// Called when the game starts or when spawned
void ABossBase::BeginPlay()
{
	Super::BeginPlay();

	UpdateBackHeadDecal();

	// 회전 검증 디버그가 켜져 있을 때만 틱 활성화
	SetActorTickEnabled(bDrawFacingDebug);

	// 보스는 '회전만' 하고 절대 이동하지 않는 설계.
	// 캐릭터끼리 캡슐이 겹치면 각자의 CharacterMovement 가 매 틱 겹침을 해소하며 자기 위치를 옮긴다
	// (속도 0이어도). Walking 모드면 보스가 스스로 밀려난다 -> 이동을 꺼서(MOVE_None) 원천 차단.
	// 회전은 SetActorRotation(BossTargetingComponent)이라 영향 없고, 캡슐은 계속 Pawn 을 Block 하므로
	// 플레이어는 보스를 통과하지 못한다(백/헤드어택 위치 판정 유지). 전 머신에서 적용.
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();                 // MOVE_None: 겹침 해소로 자기 위치를 옮기지 않음 = 안 밀림
		Move->bEnablePhysicsInteraction = false; // 물리 상호작용 push 로도 안 밀리게
	}

	// 클라이언트: 태그/큐 표현을 위해 ActorInfo 초기화 (서버는 PossessedBy에서 처리)
	if (AbilitySystemComponent && !HasAuthority())
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);

		// 서버 소유가 아니므로 PossessedBy가 안 불린다 -> 체력바 위젯 갱신을 위해
		// 여기서 체력 변화(리플리케이션 OnRep)를 구독한다. (서버는 PossessedBy에서 1회 바인딩)
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UBossAttributeSet::GetHealthAttribute())
			.AddUObject(this, &ABossBase::OnHealthChanged);
	}
}

void ABossBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bDrawFacingDebug)
	{
		return;
	}

	// 캡슐(=액터, 루트) forward 를 화살표로. 캡슐이 실제로 얼마나 돌았는지 실시간 확인용
	const FVector Origin = GetActorLocation();
	const float Length = (GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleRadius() : 50.f) + 150.f;
	DrawDebugDirectionalArrow(GetWorld(), Origin, Origin + GetActorForwardVector() * Length,
		40.f, FColor::Yellow, false, -1.f, 0, 4.f);
}

void ABossBase::InitializeAttributes()
{
	if (!AttributeSet)
	{
		return;
	}

	AttributeSet->InitMaxHealth(InitialMaxHealth);
	AttributeSet->InitHealth(InitialMaxHealth);
	AttributeSet->InitMaxStaggerGauge(InitialMaxStaggerGauge);
	AttributeSet->InitStaggerGauge(InitialMaxStaggerGauge);
}

void ABossBase::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	if (!AttributeSet)
	{
		return;
	}

	const float Max = AttributeSet->GetMaxHealth();

	// 체력바 위젯 갱신 방송 (서버/클라 공통). 줄 수 계산은 위젯이 UBossHealthBarLibrary로 수행.
	OnBossHealthChanged.Broadcast(Data.NewValue, Max);

	// 체력 0 -> 사망 (서버 권위, 1회). 이후 페이즈 전환 로직은 타지 않는다.
	if (Data.NewValue <= 0.f && HasAuthority())
	{
		HandleDeath();
		return;
	}

	// 페이즈 전환 예약 (현재 패턴 완주 후 반영). 클라에선 CurrentPhaseIndex==INDEX_NONE 라 no-op.
	if (PatternComponent && !bDead)
	{
		const float Percent = (Max > 0.f) ? (Data.NewValue / Max) * 100.f : 0.f;
		PatternComponent->NotifyHealthPercent(Percent);
	}
}

void ABossBase::Die()
{
	if (HasAuthority())
	{
		HandleDeath();
	}
}

void ABossBase::SetCombatState(FGameplayTag NewStateTag)
{
	CurrentStateTag = NewStateTag;
	// Boss uses BossPatternComponent for state management usually, but store the tag just in case
}

void ABossBase::ShowDamageText(float DamageAmount)
{
	// 몬스터와 동일하게, 보스가 피격될 때는 공격한 주체(플레이어)의 클라이언트에서 데미지 텍스트를 처리합니다.
	// 자체 처리 로직이 필요한 경우 이 곳 또는 블루프린트에서 위젯 컴포넌트를 통해 구현할 수 있습니다.
}

void ABossBase::HandleDeath()
{
	if (bDead || !HasAuthority())
	{
		return;
	}
	bDead = true;

	// 1) 사망 태그 (복제 -> 클라 UI/플레이어 파트가 State.Dead 로 감지)
	if (AbilitySystemComponent)
	{
		UBossCombatStatics::AddReplicatedLooseTag(AbilitySystemComponent, LostArkTags::State_Dead);
	}

	// 2) 패턴 정지 -> 진행 중 어빌리티 취소 (순서 중요: 취소가 부르는 종료 콜백이 다음 패턴을 못 돌게 먼저 정지)
	if (PatternComponent)
	{
		PatternComponent->StopCombat();
	}
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->CancelAllAbilities();
	}

	// 3) 기믹 정리: 무력화 페이즈 내리기 (게이지 UI 숨김)
	if (TerrainGimmickComponent)
	{
		TerrainGimmickComponent->EndStaggerPhase();
	}

	// 4) 남아있는 장판/타워 정리. 잡힌 플레이어는 장판 EndPlay(OnEndPlay)가 안전 복구한다
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<ABossPatternActorBase> It(World); It; ++It)
		{
			It->DestroyAoeNow();
		}
		for (TActorIterator<ABossGimmickTower> It(World); It; ++It)
		{
			It->Destroy();
		}
	}

	// 5) 이동 정지 + 플레이어 통과 허용 (시체가 길을 막지 않게. 바닥 지지용 나머지 채널은 유지)
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
	}
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	}

	// 6) 사망 몽타주 (전 머신 재생, 종료 시 포즈 고정)
	MulticastPlayDeathMontage();

	// 7) 사망 방송 + 클리어 연출은 레이드 게임모드가 오케스트레이션 (레이드 모드가 아니면 통지 생략)
	OnBossDied.Broadcast();
	if (ABossRaidGameMode* RaidGM = GetWorld() ? GetWorld()->GetAuthGameMode<ABossRaidGameMode>() : nullptr)
	{
		RaidGM->NotifyBossDied(this);
	}
}

void ABossBase::MulticastPlayDeathMontage_Implementation()
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	UAnimInstance* Anim = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
	if (!Anim)
	{
		return;
	}

	// 패턴 몽타주 잔여분 중단 후 사망 몽타주
	Anim->StopAllMontages(0.1f);

	if (DeathMontage)
	{
		Anim->Montage_Play(DeathMontage);

		// 몽타주가 끝나면 마지막 포즈로 고정 -> 블렌드아웃으로 일어나는(Idle 복귀) 현상 방지
		FOnMontageEnded EndDelegate;
		EndDelegate.BindUObject(this, &ABossBase::OnDeathMontageEnded);
		Anim->Montage_SetEndDelegate(EndDelegate, DeathMontage);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Boss] %s DeathMontage 미지정 — 사망 모션 없이 현재 포즈 유지"), *GetName());
	}
}

void ABossBase::OnDeathMontageEnded(UAnimMontage* /*Montage*/, bool /*bInterrupted*/)
{
	// 사망 후엔 어떤 이유로 끝났든 포즈 고정 (인터럽트 포함 — 죽은 보스가 다시 움직이면 안 됨)
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->bPauseAnims = true;
	}
}

float ABossBase::GetCurrentHealth() const
{
	return AttributeSet ? AttributeSet->GetHealth() : 0.f;
}

float ABossBase::GetMaxHealthValue() const
{
	return AttributeSet ? AttributeSet->GetMaxHealth() : 0.f;
}

void ABossBase::UpdateBackHeadDecal()
{
	if (!BackHeadDecal || !GetCapsuleComponent())
	{
		return;
	}

	// 데칼을 발밑으로 내리고(캡슐 중심 기준 아래), 반경에 맞춰 크기 갱신
	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	BackHeadDecal->SetRelativeLocation(FVector(0.f, 0.f, -HalfHeight));
	BackHeadDecal->UpdateRadius(GetCapsuleComponent()->GetScaledCapsuleRadius());
}
