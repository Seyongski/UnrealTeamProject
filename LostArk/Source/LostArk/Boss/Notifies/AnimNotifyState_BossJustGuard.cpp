// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Notifies/AnimNotifyState_BossJustGuard.h"
#include "Boss/Notifies/BossNotifyHelpers.h"
#include "Boss/Combat/BossJustGuardComponent.h"
#include "Boss/Targeting/BossTargetingComponent.h"

void UAnimNotifyState_BossJustGuard::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (UBossJustGuardComponent* JustGuard = BossNotify::GetServerComponent<UBossJustGuardComponent>(MeshComp))
	{
		// 기믹 대상 전용: 현재 타겟(레이저 대상 = 기믹 플레이어)에게만 가드 허용.
		// OpenWindow 가 GuardReady 를 부여하므로 반드시 그 전에 지정한다
		if (bOnlyCurrentTarget)
		{
			if (UBossTargetingComponent* Targeting = BossNotify::GetServerComponent<UBossTargetingComponent>(MeshComp))
			{
				JustGuard->SetExclusiveGuardPlayer(Targeting->GetCurrentTarget());
			}
		}
		else
		{
			// 전원 가드 창: 이전 기믹에서 stale 하게 남았을 수 있는 전용 대상을 명시적으로 해제(자가 치유)
			// -> 인터럽트로 NotifyEnd 가 누락돼도 이 창은 전원에게 GuardReady 를 준다
			JustGuard->ClearExclusiveGuardPlayer();
		}
		JustGuard->OpenWindow(GuardStateDuration);
	}
}

void UAnimNotifyState_BossJustGuard::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (UBossJustGuardComponent* JustGuard = BossNotify::GetServerComponent<UBossJustGuardComponent>(MeshComp))
	{
		JustGuard->CloseWindow();
		if (bOnlyCurrentTarget)
		{
			JustGuard->ClearExclusiveGuardPlayer();	// 다음 일반 저스트가드 패턴에 새지 않게
		}
	}
}

FString UAnimNotifyState_BossJustGuard::GetNotifyName_Implementation() const
{
	return TEXT("Just Guard Window");
}
