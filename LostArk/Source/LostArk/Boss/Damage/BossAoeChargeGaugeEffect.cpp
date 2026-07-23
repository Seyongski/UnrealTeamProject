// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Damage/BossAoeChargeGaugeEffect.h"
#include "Boss/Damage/BossPatternActorBase.h"
#include "Boss/Raid/BossChargeGaugeComponent.h"

void UBossAoeChargeGaugeEffect::OnHit(ABossPatternActorBase* Aoe, AActor* Target)
{
	if (bAlsoApplyDamage && Aoe)
	{
		Aoe->ApplyDamageAndStatus(Target);
	}

	if (UBossChargeGaugeComponent* Gauge =
		Target ? Target->FindComponentByClass<UBossChargeGaugeComponent>() : nullptr)
	{
		Gauge->AddGauge(GaugeAmount);
	}
}
