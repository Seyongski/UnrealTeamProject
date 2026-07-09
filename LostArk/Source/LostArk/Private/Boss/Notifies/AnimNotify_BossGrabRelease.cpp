// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Notifies/AnimNotify_BossGrabRelease.h"
#include "Damage/BossPatternActorBase.h"
#include "Damage/BossAoeGrabEffect.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"

void UAnimNotify_BossGrabRelease::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	AActor* Boss = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (!Boss || !Boss->HasAuthority())
	{
		return;	// 해제(판정/물리)는 서버 권위에서만
	}

	// 이 보스가 스폰한 잡기 장판 중 대상을 잡고 있는 것 전부 해제
	for (TActorIterator<ABossPatternActorBase> It(Boss->GetWorld()); It; ++It)
	{
		if (It->GetOwner() != Boss)
		{
			continue;
		}
		if (UBossAoeGrabEffect* Grab = Cast<UBossAoeGrabEffect>(It->GetOnHitEffect()))
		{
			if (Grab->HasGrabbedTargets())
			{
				Grab->ReleaseAll();	// 데미지+던지기 후 장판 파괴
			}
		}
	}
}
