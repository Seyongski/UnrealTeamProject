// Fill out your copyright notice in the Description page of Project Settings.


#include "Damage/BossAoe_ChargeSwap.h"
#include "Boss/BossGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"

ABossAoe_ChargeSwap::ABossAoe_ChargeSwap()
{
	// 발밑 부착 + 만료 시 1회 전원 판정 (베이스의 CastTime 후 1회 판정 시맨틱과 일치)
	TargetingMode = EAoeTargetingMode::FollowTarget;
	SpawnOrigin = EAoeSpawnOrigin::TargetLocation;
	Duration = 0.f;
	bSingleHitPerTarget = true;

	RedTag = LostArkTags::State_Charge_Red;
	BlueTag = LostArkTags::State_Charge_Blue;
}

void ABossAoe_ChargeSwap::ApplyEffectsTo(AActor* Target)
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
