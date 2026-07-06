// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Notifies/AnimNotify_BossSpawnCircle.h"
#include "Damage/BossAoe_Circle.h"

void UAnimNotify_BossSpawnCircle::ConfigureAoe(ABossPatternActorBase* Aoe) const
{
	if (ABossAoe_Circle* Circle = Cast<ABossAoe_Circle>(Aoe))
	{
		Circle->Radius = Radius;
		Circle->InnerRadius = InnerRadius;
	}
}
