// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Notifies/AnimNotify_BossClearMark.h"
#include "Boss/Notifies/BossNotifyHelpers.h"
#include "Boss/Targeting/BossTargetingComponent.h"

void UAnimNotify_BossClearMark::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (UBossTargetingComponent* Targeting = BossNotify::GetServerComponent<UBossTargetingComponent>(MeshComp))
	{
		Targeting->ClearMark();
	}
}

FString UAnimNotify_BossClearMark::GetNotifyName_Implementation() const
{
	return TEXT("Clear Mark (표식 끄기)");
}
