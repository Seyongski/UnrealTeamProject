// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Notifies/AnimNotify_BossSelectTarget.h"
#include "Boss/Notifies/BossNotifyHelpers.h"

void UAnimNotify_BossSelectTarget::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (UBossTargetingComponent* Targeting = BossNotify::GetServerComponent<UBossTargetingComponent>(MeshComp))
	{
		Targeting->SelectTarget(Policy);
	}
}

FString UAnimNotify_BossSelectTarget::GetNotifyName_Implementation() const
{
	switch (Policy)
	{
	case ETargetSelectPolicy::Farthest:	return TEXT("Select Target (최원거리)");
	case ETargetSelectPolicy::Random:	return TEXT("Select Target (랜덤)");
	default:							return TEXT("Select Target (최근접)");
	}
}
