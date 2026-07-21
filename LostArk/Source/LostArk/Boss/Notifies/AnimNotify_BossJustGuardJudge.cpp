// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Notifies/AnimNotify_BossJustGuardJudge.h"
#include "Boss/Notifies/BossNotifyHelpers.h"
#include "Boss/Combat/BossJustGuardComponent.h"

void UAnimNotify_BossJustGuardJudge::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (UBossJustGuardComponent* JustGuard = BossNotify::GetServerComponent<UBossJustGuardComponent>(MeshComp))
	{
		JustGuard->JudgeGuardAtAttack(GuardAngleTolerance, bDebugBypassDirection,
			DamageEffect, DamageCoefficient, bGroggyOnSuccess);
	}
}

FString UAnimNotify_BossJustGuardJudge::GetNotifyName_Implementation() const
{
	return TEXT("Just Guard Judge");
}
