// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Notifies/AnimNotifyState_BossTrackTarget.h"
#include "Boss/Notifies/BossNotifyHelpers.h"
#include "Boss/BossGameplayTags.h"
#include "Boss/Targeting/BossTargetingComponent.h"

UAnimNotifyState_BossTrackTarget::UAnimNotifyState_BossTrackTarget()
{
	// 네이티브 태그로 기본값 지정 (피커에서 바꿔도 되고, 비어있으면 아래에서 이 값으로 폴백)
	TrackTag = LostArkTags::State_Boss_TrackTarget.GetTag();
}

void UAnimNotifyState_BossTrackTarget::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	// 속도 오버라이드는 태그보다 먼저 — 추적 첫 프레임부터 이 구간 속도가 적용되도록
	if (bOverrideTurnSpeed)
	{
		if (UBossTargetingComponent* Targeting = BossNotify::GetServerComponent<UBossTargetingComponent>(MeshComp))
		{
			Targeting->PushTurnSpeedOverride(RotationInterpSpeed, MaxTurnRate);
		}
	}

	// 필드가 비어있으면(기존 몽타주에 None으로 저장된 경우) 네이티브 태그로 폴백
	const FGameplayTag Tag = TrackTag.IsValid() ? TrackTag : LostArkTags::State_Boss_TrackTarget.GetTag();
	if (UAbilitySystemComponent* ASC = BossNotify::GetServerASC(MeshComp))
	{
		ASC->AddLooseGameplayTag(Tag);
	}
}

void UAnimNotifyState_BossTrackTarget::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	const FGameplayTag Tag = TrackTag.IsValid() ? TrackTag : LostArkTags::State_Boss_TrackTarget.GetTag();
	if (UAbilitySystemComponent* ASC = BossNotify::GetServerASC(MeshComp))
	{
		ASC->RemoveLooseGameplayTag(Tag);
	}

	if (bOverrideTurnSpeed)
	{
		if (UBossTargetingComponent* Targeting = BossNotify::GetServerComponent<UBossTargetingComponent>(MeshComp))
		{
			Targeting->ClearTurnSpeedOverride();
		}
	}
}
