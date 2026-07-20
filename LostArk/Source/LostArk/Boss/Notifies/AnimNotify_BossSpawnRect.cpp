// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Notifies/AnimNotify_BossSpawnRect.h"
#include "Boss/Damage/BossAoe_Rect.h"

void UAnimNotify_BossSpawnRect::ConfigureAoe(ABossPatternActorBase* Aoe) const
{
	if (ABossAoe_Rect* Rect = Cast<ABossAoe_Rect>(Aoe))
	{
		Rect->HalfLength = HalfLength;
		Rect->HalfWidth = HalfWidth;
		Rect->ForwardOffset = ForwardOffset;
	}
}
