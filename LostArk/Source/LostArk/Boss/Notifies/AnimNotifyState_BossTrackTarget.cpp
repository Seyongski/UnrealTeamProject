// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Notifies/AnimNotifyState_BossTrackTarget.h"
#include "Boss/Notifies/BossNotifyHelpers.h"
#include "Boss/BossGameplayTags.h"

UAnimNotifyState_BossTrackTarget::UAnimNotifyState_BossTrackTarget()
{
	// 네이티브 태그로 기본값 지정 (피커에서 바꿔도 되고, 비어있으면 아래에서 이 값으로 폴백)
	TrackTag = LostArkTags::State_Boss_TrackTarget.GetTag();
}

void UAnimNotifyState_BossTrackTarget::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

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
}
