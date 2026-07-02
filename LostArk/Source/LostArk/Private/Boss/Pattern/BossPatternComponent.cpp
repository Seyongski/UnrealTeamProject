// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Pattern/BossPatternComponent.h"
#include "Boss/Pattern/BossPatternAbility.h"
#include "Boss/Pattern/BossPhaseDataAsset.h"
#include "Boss/Pattern/PatternDataAsset.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"

UBossPatternComponent::UBossPatternComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UBossPatternComponent::BeginPlay()
{
	Super::BeginPlay();

	// 기본 패턴 어빌리티를 보스 ASC에 부여 (권한 있는 쪽에서만)
	if (UAbilitySystemComponent* ASC = GetASC())
	{
		if (DefaultPatternAbility && ASC->IsOwnerActorAuthoritative())
		{
			PatternAbilityHandle = ASC->GiveAbility(
				FGameplayAbilitySpec(DefaultPatternAbility, 1, INDEX_NONE, GetOwner()));
		}
	}
}

UAbilitySystemComponent* UBossPatternComponent::GetASC() const
{
	return GetOwner() ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()) : nullptr;
}

void UBossPatternComponent::StartCombat()
{
	if (Phases.Num() == 0)
	{
		return;
	}
	EnterPhase(0);
}

void UBossPatternComponent::NotifyHealthPercent(float HealthPercent)
{
	// 전투 시작 전이면 무시
	if (CurrentPhaseIndex == INDEX_NONE)
	{
		return;
	}

	// 현재 체력으로 진입 가능한 가장 깊은 페이즈 찾기 (Phases 는 EnterHealthPercent 내림차순 가정)
	int32 Target = CurrentPhaseIndex;
	for (int32 i = 0; i < Phases.Num(); ++i)
	{
		if (Phases[i] && HealthPercent <= Phases[i]->EnterHealthPercent)
		{
			Target = i;
		}
	}

	// 더 깊은 페이즈로 가야 하면 '예약'만. 현재 패턴은 끝까지 실행됨
	if (Target > CurrentPhaseIndex)
	{
		PendingPhaseIndex = Target;
	}
}

void UBossPatternComponent::EnterPhase(int32 PhaseIndex)
{
	if (!Phases.IsValidIndex(PhaseIndex) || !Phases[PhaseIndex])
	{
		return;
	}

	CurrentPhaseIndex = PhaseIndex;
	UBossPhaseDataAsset* Phase = Phases[PhaseIndex];

	// 전환 기믹이 있으면 먼저 실행 -> 끝나면 EntryNode 부터 시작
	if (Phase->TransitionGimmick)
	{
		bRunningTransition = true;
		RunPatternData(Phase->TransitionGimmick);
	}
	else
	{
		ActivateNode(Phase->EntryNodeId);
	}
}

void UBossPatternComponent::ActivateNode(FName NodeId)
{
	UBossPhaseDataAsset* Phase = Phases.IsValidIndex(CurrentPhaseIndex) ? Phases[CurrentPhaseIndex] : nullptr;
	if (!Phase)
	{
		return;
	}

	const FBossPatternNode* Node = Phase->FindNode(NodeId);
	if (!Node || !Node->Pattern)
	{
		// 갈 곳이 없으면 진입 노드로 되돌려 루프 (정책은 필요 시 변경)
		if (NodeId != Phase->EntryNodeId && !Phase->EntryNodeId.IsNone())
		{
			ActivateNode(Phase->EntryNodeId);
		}
		return;
	}

	CurrentNodeId = NodeId;
	RunPatternData(Node->Pattern);
}

void UBossPatternComponent::RunPatternData(UPatternDataAsset* Data)
{
	ActivePatternData = Data;

	// 어빌리티 종료 콜백 안에서 곧바로 재활성화하면 재진입 문제 -> 다음 틱에 활성화
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(this, &UBossPatternComponent::ActivateAbilityNow);
	}
}

void UBossPatternComponent::ActivateAbilityNow()
{
	if (UAbilitySystemComponent* ASC = GetASC())
	{
		ASC->TryActivateAbility(PatternAbilityHandle);
	}
}

void UBossPatternComponent::OnPatternAbilityFinished(EPatternResult Result)
{
	// 1) 방금 끝난 게 페이즈 전환 기믹이면 -> 새 페이즈 진입 노드부터 시작
	if (bRunningTransition)
	{
		bRunningTransition = false;
		if (Phases.IsValidIndex(CurrentPhaseIndex))
		{
			ActivateNode(Phases[CurrentPhaseIndex]->EntryNodeId);
		}
		return;
	}

	// 2) 예약된 페이즈 전환이 있으면 (현재 패턴을 다 실행한 지금) 전환
	if (PendingPhaseIndex > CurrentPhaseIndex && Phases.IsValidIndex(PendingPhaseIndex))
	{
		const int32 Target = PendingPhaseIndex;
		PendingPhaseIndex = INDEX_NONE;
		EnterPhase(Target);
		return;
	}

	// 3) 일반 분기: 성공/실패에 따라 다음 노드로
	UBossPhaseDataAsset* Phase = Phases.IsValidIndex(CurrentPhaseIndex) ? Phases[CurrentPhaseIndex] : nullptr;
	if (!Phase)
	{
		return;
	}

	const FBossPatternNode* Node = Phase->FindNode(CurrentNodeId);
	if (!Node)
	{
		return;
	}

	const FName NextId = (Result == EPatternResult::Success) ? Node->NextOnSuccess : Node->NextOnFail;
	ActivateNode(NextId);
}
