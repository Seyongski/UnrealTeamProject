// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Pattern/BossPhaseDataAsset.h"
#include "Boss/Pattern/PatternDataAsset.h"

UPatternDataAsset* UBossPhaseDataAsset::PickRandomWeighted() const
{
	float Total = 0.f;
	for (const FBossWeightedPattern& P : Patterns)
	{
		if (P.Pattern && P.Weight > 0.f)
		{
			Total += P.Weight;
		}
	}

	if (Total <= 0.f)
	{
		// 가중치가 없으면 첫 유효 패턴으로 폴백
		for (const FBossWeightedPattern& P : Patterns)
		{
			if (P.Pattern)
			{
				return P.Pattern;
			}
		}
		return nullptr;
	}

	float Roll = FMath::FRandRange(0.f, Total);
	for (const FBossWeightedPattern& P : Patterns)
	{
		if (!P.Pattern || P.Weight <= 0.f)
		{
			continue;
		}
		Roll -= P.Weight;
		if (Roll <= 0.f)
		{
			return P.Pattern;
		}
	}
	return nullptr;
}
