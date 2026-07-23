// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Notifies/AnimNotify_BossGimmickDestroySlice.h"
#include "Boss/Notifies/BossNotifyHelpers.h"
#include "Boss/Gimmick/BossTerrainGimmickComponent.h"

void UAnimNotify_BossGimmickDestroySlice::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (UBossTerrainGimmickComponent* Gimmick = BossNotify::GetServerComponent<UBossTerrainGimmickComponent>(MeshComp))
	{
		Gimmick->RequestDestroyGimmickSlice(Delay);
	}
}

FString UAnimNotify_BossGimmickDestroySlice::GetNotifyName_Implementation() const
{
	return FString::Printf(TEXT("Gimmick: Destroy Slice (+%.1fs)"), Delay);
}
