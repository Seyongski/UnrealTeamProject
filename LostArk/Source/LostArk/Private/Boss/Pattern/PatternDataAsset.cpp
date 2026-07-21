// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Pattern/PatternDataAsset.h"

const FPatternStep* UPatternDataAsset::FindStep(FName StepId) const
{
	return Steps.FindByPredicate([StepId](const FPatternStep& Step)
	{
		return Step.StepId == StepId;
	});
}
