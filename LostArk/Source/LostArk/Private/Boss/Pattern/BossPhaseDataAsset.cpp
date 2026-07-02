// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Pattern/BossPhaseDataAsset.h"

const FBossPatternNode* UBossPhaseDataAsset::FindNode(FName NodeId) const
{
	return Nodes.FindByPredicate([NodeId](const FBossPatternNode& Node)
	{
		return Node.NodeId == NodeId;
	});
}
