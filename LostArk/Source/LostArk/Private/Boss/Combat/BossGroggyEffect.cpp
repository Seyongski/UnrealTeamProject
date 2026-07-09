// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Combat/BossGroggyEffect.h"
#include "Boss/BossGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UBossGroggyEffect::UBossGroggyEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	// 지속시간은 적용 시점에 SetByCaller 로 주입 (카운터 컴포넌트의 GroggyDuration)
	FSetByCallerFloat DurationSetByCaller;
	DurationSetByCaller.DataTag = LostArkTags::Data_Duration.GetTag();
	DurationMagnitude = FGameplayEffectModifierMagnitude(DurationSetByCaller);

	// 지속 중 대상(보스)에 State.Boss.Groggy 부여
	// (FindOrAddComponent 는 내부가 NewObject 라 UObject 생성자 안에서 사용 불가 -> CDO 생성 시 페이탈.
	//  생성자에서는 CreateDefaultSubobject 로 만들어 GEComponents 에 직접 등록해야 한다)
	UTargetTagsGameplayEffectComponent* TargetTags =
		CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"));
	FInheritedTagContainer TagChanges;
	TagChanges.AddTag(LostArkTags::State_Boss_Groggy.GetTag());
	TargetTags->SetAndApplyTargetTagChanges(TagChanges);
	GEComponents.Add(TargetTags);
}
