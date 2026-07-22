// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Notifies/AnimNotify_BossBeginStaggerPhase.h"
#include "Boss/Notifies/BossNotifyHelpers.h"

void UAnimNotify_BossBeginStaggerPhase::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (UBossTerrainGimmickComponent* Gimmick = BossNotify::GetServerComponent<UBossTerrainGimmickComponent>(MeshComp))
	{
		Gimmick->BeginStaggerPhase(RequiredAmount, Resolve, WindowDuration, HoldSection, ReleaseSection);
	}
}

FString UAnimNotify_BossBeginStaggerPhase::GetNotifyName_Implementation() const
{
	return TEXT("Begin Stagger Phase");
}
