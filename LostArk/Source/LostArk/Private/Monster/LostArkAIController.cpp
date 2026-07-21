// Fill out your copyright notice in the Description page of Project Settings.

#include "Monster/LostArkAIController.h"
#include "Monster/LostArkMonster.h"
#include "Abilities/LostArkAttributeSet.h"
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

	// 4. 플레이어와의 거리 측정 (캡슐 반경을 뺀 표면 간의 실제 거리를 계산)
	float CenterDistance = Monster->GetDistanceTo(PlayerCharacter);
	
	UCapsuleComponent* MonsterCapsule = Monster->GetCapsuleComponent();
	UCapsuleComponent* PlayerCapsule = PlayerCharacter->GetCapsuleComponent();
	float MonsterRadius = MonsterCapsule ? MonsterCapsule->GetScaledCapsuleRadius() : 0.f;
	float PlayerRadius = PlayerCapsule ? PlayerCapsule->GetScaledCapsuleRadius() : 0.f;
	
	float SurfaceDistance = FMath::Max(0.f, CenterDistance - MonsterRadius - PlayerRadius);

	// GAS 어트리뷰트 셋으로부터 공격 사거리 동적 획득
	float AttackRange = ASC->GetNumericAttribute(ULostArkAttributeSet::GetAttackRangeAttribute());

	// 5. 조건별 전이 처리 (네비게이션 도착 오차를 고려하여 사거리에 약간의 여유(Tolerance) 부여)
	if (SurfaceDistance <= AttackRange + 15.f)
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
		MoveRequest.SetAcceptanceRadius(FMath::Max(0.f, AttackRange - 15.f)); // 공격 범위 내부로 완벽히 도달 유도 (오차 보정)
		MoveRequest.SetUsePathfinding(true);

		MoveTo(MoveRequest);
	}
}
