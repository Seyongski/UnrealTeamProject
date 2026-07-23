// Fill out your copyright notice in the Description page of Project Settings.


#include "Boss/Damage/BossAoeChargeSwapEffect.h"
#include "Boss/BossGameplayTags.h"
#include "Boss/Combat/BossCombatStatics.h"
#include "AbilitySystemBlueprintLibrary.h"

UBossAoeChargeSwapEffect::UBossAoeChargeSwapEffect()
{
	RedTag = LostArkTags::State_Charge_Red;
	BlueTag = LostArkTags::State_Charge_Blue;
}

void UBossAoeChargeSwapEffect::OnHit(ABossPatternActorBase* /*Aoe*/, AActor* Target)
{
	// 데미지 GE 대신 전하 스왑만 수행 (과충전 게이지 반전과 동일한 공용 로직)
	UBossCombatStatics::FlipCharge(
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target),
		RedChargeEffect, BlueChargeEffect, this, RedTag, BlueTag);
}
