// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Notifies/AnimNotifyState_BossScriptedTurn.h"
#include "Boss/Notifies/BossNotifyHelpers.h"
#include "Boss/Targeting/BossTargetingComponent.h"

void UAnimNotifyState_BossScriptedTurn::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (UBossTargetingComponent* Targeting = BossNotify::GetServerComponent<UBossTargetingComponent>(MeshComp))
	{
		Targeting->BeginScriptedTurn(TurnRate, FallbackTurnSign);
	}
}

void UAnimNotifyState_BossScriptedTurn::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (UBossTargetingComponent* Targeting = BossNotify::GetServerComponent<UBossTargetingComponent>(MeshComp))
	{
		Targeting->EndScriptedTurn();
	}
}
