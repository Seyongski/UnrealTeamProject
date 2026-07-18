// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Notifies/AnimNotify_BossApplyGroggy.h"
#include "Boss/Notifies/BossNotifyHelpers.h"
#include "Boss/Combat/BossGroggyEffect.h"
#include "Boss/BossGameplayTags.h"

UAnimNotify_BossApplyGroggy::UAnimNotify_BossApplyGroggy()
{
	GroggyEffectClass = UBossGroggyEffect::StaticClass();
}

void UAnimNotify_BossApplyGroggy::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	UAbilitySystemComponent* ASC = BossNotify::GetServerASC(MeshComp);
	if (!ASC || !GroggyEffectClass)
	{
		return;
	}

	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(GroggyEffectClass, 1.f, ASC->MakeEffectContext());
	if (Spec.IsValid())
	{
		Spec.Data->SetSetByCallerMagnitude(LostArkTags::Data_Duration.GetTag(), FMath::Max(0.1f, GroggyDuration));
		ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
	}
}

FString UAnimNotify_BossApplyGroggy::GetNotifyName_Implementation() const
{
	return FString::Printf(TEXT("Apply Groggy (%.1fs)"), GroggyDuration);
}
