// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Notifies/AnimNotify_BossGimmickSpawnTower.h"
#include "Boss/Notifies/BossNotifyHelpers.h"
#include "Boss/Gimmick/BossTerrainGimmickComponent.h"

void UAnimNotify_BossGimmickSpawnTower::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (UBossTerrainGimmickComponent* Gimmick = BossNotify::GetServerComponent<UBossTerrainGimmickComponent>(MeshComp))
	{
		Gimmick->SpawnGimmickTower(TargetSlice);
		if (bBeginStaggerPhase)
		{
			Gimmick->BeginStaggerPhase(StaggerRequiredAmount, EStaggerResolve::Groggy);
		}
	}
}

FString UAnimNotify_BossGimmickSpawnTower::GetNotifyName_Implementation() const
{
	return FString::Printf(TEXT("Gimmick: Spawn Tower (Slice %d)"), TargetSlice);
}
