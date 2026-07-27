// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Notifies/AnimNotify_BossMarkTarget.h"
#include "Boss/Notifies/BossNotifyHelpers.h"
#include "Boss/Targeting/BossTargetingComponent.h"

void UAnimNotify_BossMarkTarget::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (UBossTargetingComponent* Targeting = BossNotify::GetServerComponent<UBossTargetingComponent>(MeshComp))
	{
		Targeting->MarkCurrentTarget();
	}
}

FString UAnimNotify_BossMarkTarget::GetNotifyName_Implementation() const
{
	return TEXT("Mark Target (표식 켜기)");
}
