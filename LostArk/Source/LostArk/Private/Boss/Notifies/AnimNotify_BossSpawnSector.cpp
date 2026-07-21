// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Notifies/AnimNotify_BossSpawnSector.h"
#include "Boss/Damage/BossAoe_Sector.h"

void UAnimNotify_BossSpawnSector::ConfigureAoe(ABossPatternActorBase* Aoe) const
{
	if (ABossAoe_Sector* Sector = Cast<ABossAoe_Sector>(Aoe))
	{
		Sector->Radius = Radius;
		Sector->InnerRadius = InnerRadius;
		Sector->StartAngle = StartAngle;
		Sector->EndAngle = EndAngle;
	}
}
