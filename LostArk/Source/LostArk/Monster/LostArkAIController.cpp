// Fill out your copyright notice in the Description page of Project Settings.

#include "LostArk/Monster/LostArkAIController.h"
#include "LostArk/Monster/LostArkMonster.h"
#include "LostArk/Character/LostArkAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"
#include "Navigation/PathFollowingComponent.h"
#include "Components/CapsuleComponent.h"
#include "AITypes.h"

ALostArkAIController::ALostArkAIController()
{
	PrimaryActorTick.bCanEverTick = false;
	TargetCheckInterval = 0.2f;

	StateSpawningTag = FGameplayTag::RequestGameplayTag(FName("State.Spawning"));
	StateIdleTag = FGameplayTag::RequestGameplayTag(FName("State.Idle"));
	StateMovingTag = FGameplayTag::RequestGameplayTag(FName("State.Moving"));
	StateAttackingTag = FGameplayTag::RequestGameplayTag(FName("State.Attacking"));
	StateDeadTag = FGameplayTag::RequestGameplayTag(FName("State.Dead"));
}

void ALostArkAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	GetWorldTimerManager().SetTimer(AIUpdateTimerHandle, this, &ALostArkAIController::UpdateAIBehavior, TargetCheckInterval, true);
}

void ALostArkAIController::OnUnPossess()
{
	GetWorldTimerManager().ClearTimer(AIUpdateTimerHandle);

	Super::OnUnPossess();
}

void ALostArkAIController::RestartAILogic()
{
	GetWorldTimerManager().ClearTimer(AIUpdateTimerHandle);
	GetWorldTimerManager().SetTimer(AIUpdateTimerHandle, this, &ALostArkAIController::UpdateAIBehavior, TargetCheckInterval, true);
}

void ALostArkAIController::UpdateAIBehavior()
{
	ALostArkMonster* Monster = Cast<ALostArkMonster>(GetPawn());
	if (!IsValid(Monster) || Monster->IsDead())
	{
		GetWorldTimerManager().ClearTimer(AIUpdateTimerHandle);
		return;
	}

	UAbilitySystemComponent* ASC = Monster->GetAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	if (ASC->HasMatchingGameplayTag(StateSpawningTag) || ASC->HasMatchingGameplayTag(StateDeadTag))
	{
		StopMovement();
		return;
	}

	if (ASC->HasMatchingGameplayTag(StateAttackingTag))
	{
		return;
	}

	ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
	if (!IsValid(PlayerCharacter))
	{
		ClearFocus(EAIFocusPriority::Gameplay);
		Monster->SetMonsterState(StateIdleTag);
		StopMovement();
		return;
	}

	SetFocus(PlayerCharacter);

	float CenterDistance = Monster->GetDistanceTo(PlayerCharacter);
	
	UCapsuleComponent* MonsterCapsule = Monster->GetCapsuleComponent();
	UCapsuleComponent* PlayerCapsule = PlayerCharacter->GetCapsuleComponent();
	float MonsterRadius = MonsterCapsule ? MonsterCapsule->GetScaledCapsuleRadius() : 0.f;
	float PlayerRadius = PlayerCapsule ? PlayerCapsule->GetScaledCapsuleRadius() : 0.f;
	
	float SurfaceDistance = FMath::Max(0.f, CenterDistance - MonsterRadius - PlayerRadius);

	float AttackRange = ASC->GetNumericAttribute(ULostArkAttributeSet::GetAttackRangeAttribute());

	if (SurfaceDistance <= AttackRange + 15.f)
	{
		StopMovement();
		Monster->TryActivateAttack();
	}
	else
	{
		if (Monster->GetCurrentStateTag() == StateIdleTag)
		{
			Monster->SetMonsterState(StateMovingTag);
		}

		FAIMoveRequest MoveRequest;
		MoveRequest.SetGoalActor(PlayerCharacter);
		MoveRequest.SetAcceptanceRadius(FMath::Max(0.f, AttackRange - 15.f));
		MoveRequest.SetUsePathfinding(true);

		MoveTo(MoveRequest);
	}
}



