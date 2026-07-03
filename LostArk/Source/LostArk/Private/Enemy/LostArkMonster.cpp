#include "LostArkMonster.h"
#include "LostArkAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagsManager.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

static const float MonsterDefaultRotationRateYaw = 480.f;

ALostArkMonster::ALostArkMonster()
{
	PrimaryActorTick.bCanEverTick = false;

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AttributeSet = CreateDefaultSubobject<ULostArkAttributeSet>(TEXT("AttributeSet"));

	SpawnDuration = 1.5f;
	bIsDead = false;

	GetCapsuleComponent()->SetCollisionProfileName(TEXT("Pawn"));

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, MonsterDefaultRotationRateYaw, 0.f);
}

UAbilitySystemComponent* ALostArkMonster::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ALostArkMonster::BeginPlay()
{
	StateSpawningTag = FGameplayTag::RequestGameplayTag(FName("State.Spawning"));
	StateIdleTag = FGameplayTag::RequestGameplayTag(FName("State.Idle"));
	StateMovingTag = FGameplayTag::RequestGameplayTag(FName("State.Moving"));
	StateAttackingTag = FGameplayTag::RequestGameplayTag(FName("State.Attacking"));
	StateDeadTag = FGameplayTag::RequestGameplayTag(FName("State.Dead"));

	Super::BeginPlay();

	SetMonsterState(StateSpawningTag);

	GetWorldTimerManager().SetTimer(SpawnFinishedTimerHandle, this, &ALostArkMonster::OnSpawnFinished, SpawnDuration, false);
}

void ALostArkMonster::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);

		if (AttackAbilityClass && !AttackAbilityHandle.IsValid())
		{
			AttackAbilityHandle = AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AttackAbilityClass, 1, 0, this));
		}
	}
}

void ALostArkMonster::OnSpawnFinished()
{
	if (!bIsDead)
	{
		SetMonsterState(StateIdleTag);
	}
}

void ALostArkMonster::SetMonsterState(FGameplayTag NewStateTag)
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	if (CurrentStateTag.IsValid())
	{
		AbilitySystemComponent->RemoveLooseGameplayTag(CurrentStateTag);
	}

	CurrentStateTag = NewStateTag;
	if (CurrentStateTag.IsValid())
	{
		AbilitySystemComponent->AddLooseGameplayTag(CurrentStateTag);
	}

	UE_LOG(LogTemp, Log, TEXT("[Monster] %s state updated to: %s"), *GetName(), *NewStateTag.ToString());
}

bool ALostArkMonster::TryActivateAttack()
{
	if (!AbilitySystemComponent || bIsDead)
	{
		return false;
	}

	if (AbilitySystemComponent->HasMatchingGameplayTag(StateSpawningTag) || 
		AbilitySystemComponent->HasMatchingGameplayTag(StateDeadTag))
	{
		return false;
	}

	if (AttackAbilityClass)
	{
		SetMonsterState(StateAttackingTag);

		bool bSuccess = AbilitySystemComponent->TryActivateAbilityByClass(AttackAbilityClass);
		if (!bSuccess)
		{
			SetMonsterState(StateIdleTag);
		}
		return bSuccess;
	}

	return false;
}

void ALostArkMonster::Die()
{
	if (bIsDead)
	{
		return;
	}

	bIsDead = true;
	SetMonsterState(StateDeadTag);

	GetWorldTimerManager().ClearTimer(SpawnFinishedTimerHandle);

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	GetCharacterMovement()->DisableMovement();
	GetCharacterMovement()->StopMovementImmediately();

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->CancelAllAbilities();
	}

	OnMonsterKilled.Broadcast(this);
}

void ALostArkMonster::OnAcquiredFromPool_Implementation()
{
	if (AttributeSet)
	{
		AttributeSet->SetHealth(AttributeSet->GetMaxHealth());
	}
	bIsDead = false;

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GetCharacterMovement()->SetDefaultMovementMode();

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);

		AbilitySystemComponent->RemoveLooseGameplayTag(StateIdleTag);
		AbilitySystemComponent->RemoveLooseGameplayTag(StateMovingTag);
		AbilitySystemComponent->RemoveLooseGameplayTag(StateAttackingTag);
		AbilitySystemComponent->RemoveLooseGameplayTag(StateDeadTag);
	}

	SetMonsterState(StateSpawningTag);
	GetWorldTimerManager().SetTimer(SpawnFinishedTimerHandle, this, &ALostArkMonster::OnSpawnFinished, SpawnDuration, false);

	UE_LOG(LogTemp, Log, TEXT("[Monster] %s acquired from pool, health reset and Spawning state set."), *GetName());
}

void ALostArkMonster::OnReleasedToPool_Implementation()
{
	OnMonsterKilled.Clear();
	GetWorldTimerManager().ClearTimer(SpawnFinishedTimerHandle);

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->CancelAllAbilities();

		if (CurrentStateTag.IsValid())
		{
			AbilitySystemComponent->RemoveLooseGameplayTag(CurrentStateTag);
			CurrentStateTag = FGameplayTag::EmptyTag;
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("[Monster] %s released to pool, clean up complete."), *GetName());
}

