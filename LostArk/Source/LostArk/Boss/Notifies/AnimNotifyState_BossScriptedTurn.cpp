// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Notifies/AnimNotifyState_BossScriptedTurn.h"
#include "Boss/Targeting/BossTargetingComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

namespace
{
	// 서버 권한일 때만 보스 타겟팅 컴포넌트 반환 (회전은 서버 전용, 복제로 클라 전파)
	UBossTargetingComponent* GetServerTargeting(USkeletalMeshComponent* MeshComp)
	{
		AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
		if (!Owner || !Owner->HasAuthority())
		{
			return nullptr;
		}
		return Owner->FindComponentByClass<UBossTargetingComponent>();
	}
}

void UAnimNotifyState_BossScriptedTurn::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (UBossTargetingComponent* Targeting = GetServerTargeting(MeshComp))
	{
		Targeting->BeginScriptedTurn(TurnRate, FallbackTurnSign);
	}
}

void UAnimNotifyState_BossScriptedTurn::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (UBossTargetingComponent* Targeting = GetServerTargeting(MeshComp))
	{
		Targeting->EndScriptedTurn();
	}
}
