// Fill out your copyright notice in the Description page of Project Settings.

#include "LostArkAIController.h"
#include "LostArkMonster.h"
#include "LostArkAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"
#include "Navigation/PathFollowingComponent.h"
#include "AITypes.h"

ALostArkAIController::ALostArkAIController()
{
	PrimaryActorTick.bCanEverTick = false;
	TargetCheckInterval = 0.2f; // 0.2초마다 의사결정 수행

	StateSpawningTag = FGameplayTag::RequestGameplayTag(FName("State.Spawning"));
	StateIdleTag = FGameplayTag::RequestGameplayTag(FName("State.Idle"));
	StateMovingTag = FGameplayTag::RequestGameplayTag(FName("State.Moving"));
	StateAttackingTag = FGameplayTag::RequestGameplayTag(FName("State.Attacking"));
	StateDeadTag = FGameplayTag::RequestGameplayTag(FName("State.Dead"));
}

void ALostArkAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// Possession 직후 행동 타이머 활성화
	GetWorldTimerManager().SetTimer(AIUpdateTimerHandle, this, &ALostArkAIController::UpdateAIBehavior, TargetCheckInterval, true);
}

void ALostArkAIController::OnUnPossess()
{
	GetWorldTimerManager().ClearTimer(AIUpdateTimerHandle);

	Super::OnUnPossess();
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

	// 1. 소환 연출 중이거나 사망인 경우 조종 비활성화 및 이동 차단
	if (ASC->HasMatchingGameplayTag(StateSpawningTag) || ASC->HasMatchingGameplayTag(StateDeadTag))
	{
		StopMovement();
		return;
	}

	// 2. 공격 동작(몽타주 재생 등)이 수행 중인 경우 이동하지 않고 대기
	if (ASC->HasMatchingGameplayTag(StateAttackingTag))
	{
		return;
	}

	// 3. 타겟(플레이어 캐릭터) 탐색
	ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
	if (!IsValid(PlayerCharacter))
	{
		ClearFocus(EAIFocusPriority::Gameplay);
		Monster->SetMonsterState(StateIdleTag);
		StopMovement();
		return;
	}

	// 플레이어 방향을 바라보도록 포커스 설정
	SetFocus(PlayerCharacter);

	// 4. 플레이어와의 거리 측정
	float Distance = FVector::Dist(Monster->GetActorLocation(), PlayerCharacter->GetActorLocation());
	
	// GAS 어트리뷰트 셋으로부터 공격 사거리 동적 획득
	float AttackRange = ASC->GetNumericAttribute(ULostArkAttributeSet::GetAttackRangeAttribute());

	// 5. 조건별 전이 처리
	if (Distance <= AttackRange)
	{
		// 범위 내 진입: 즉시 멈추고 공격 시도
		StopMovement();
		Monster->TryActivateAttack();
	}
	else
	{
		// 범위 밖: 이동 상태 태그로 변경 후 추적 이동 명령
		if (Monster->GetCurrentStateTag() == StateIdleTag)
		{
			Monster->SetMonsterState(StateMovingTag);
		}

		FAIMoveRequest MoveRequest;
		MoveRequest.SetGoalActor(PlayerCharacter);
		MoveRequest.SetAcceptanceRadius(AttackRange - 10.f); // 공격 범위 내부로 완벽히 도달 유도
		MoveRequest.SetUsePathfinding(true);

		MoveTo(MoveRequest);
	}
}
