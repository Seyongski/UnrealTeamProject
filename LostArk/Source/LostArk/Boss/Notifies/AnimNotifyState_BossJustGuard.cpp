// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Notifies/AnimNotifyState_BossJustGuard.h"
#include "Boss/Notifies/BossNotifyHelpers.h"
#include "Boss/Combat/BossJustGuardComponent.h"

void UAnimNotifyState_BossJustGuard::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (UBossJustGuardComponent* JustGuard = BossNotify::GetServerComponent<UBossJustGuardComponent>(MeshComp))
	{
		JustGuard->OpenWindow();
	}
}

void UAnimNotifyState_BossJustGuard::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (UBossJustGuardComponent* JustGuard = BossNotify::GetServerComponent<UBossJustGuardComponent>(MeshComp))
	{
		JustGuard->CloseWindow();
	}
}

FString UAnimNotifyState_BossJustGuard::GetNotifyName_Implementation() const
{
	return TEXT("Just Guard Window");
}
