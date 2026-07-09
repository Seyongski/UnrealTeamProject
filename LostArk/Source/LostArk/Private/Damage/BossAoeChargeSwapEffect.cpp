// Fill out your copyright notice in the Description page of Project Settings.


#include "Damage/BossAoeChargeSwapEffect.h"
#include "Boss/BossGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"

UBossAoeChargeSwapEffect::UBossAoeChargeSwapEffect()
{
	RedTag = LostArkTags::State_Charge_Red;
	BlueTag = LostArkTags::State_Charge_Blue;
}

void UBossAoeChargeSwapEffect::OnHit(ABossPatternActorBase* /*Aoe*/, AActor* Target)
{
	// 데미지 GE 대신 전하 스왑만 수행
	UAbilitySystemComponent* ASC =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
	if (!ASC)
	{
		return;
	}

	const bool bHasRed = ASC->HasMatchingGameplayTag(RedTag);
	const bool bHasBlue = ASC->HasMatchingGameplayTag(BlueTag);
	if (bHasRed == bHasBlue)
	{
		return;	// 전하 미부여(또는 비정상 중복) 대상은 스킵
	}

	const TSubclassOf<UGameplayEffect> RemoveGE = bHasRed ? RedChargeEffect : BlueChargeEffect;
	const TSubclassOf<UGameplayEffect> AddGE = bHasRed ? BlueChargeEffect : RedChargeEffect;

	if (RemoveGE)
	{
		ASC->RemoveActiveGameplayEffectBySourceEffect(RemoveGE, nullptr);
	}

	if (AddGE)
	{
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		Context.AddSourceObject(this);
		FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(AddGE, 1.f, Context);
		if (Spec.IsValid())
		{
			ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
		}
	}
}
