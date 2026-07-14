// Fill out your copyright notice in the Description page of Project Settings.


#include "Boss/Raid/BossRaidGameState.h"
#include "Net/UnrealNetwork.h"

void ABossRaidGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABossRaidGameState, DestroyedSliceMask);
	DOREPLIFETIME(ABossRaidGameState, SliceCount);
	DOREPLIFETIME(ABossRaidGameState, ArenaCenter);
	DOREPLIFETIME(ABossRaidGameState, ArenaFloorZ);
}

bool ABossRaidGameState::IsSliceDestroyed(int32 SliceIndex) const
{
	if (SliceIndex < 0 || SliceIndex >= SliceCount)
	{
		return false;
	}
	return (DestroyedSliceMask & (1 << SliceIndex)) != 0;
}

int32 ABossRaidGameState::GetSliceIndexAt(const FVector& WorldLocation) const
{
	if (SliceCount <= 0)
	{
		return INDEX_NONE;
	}

	const FVector Local = WorldLocation - ArenaCenter;
	float Angle = FMath::RadiansToDegrees(FMath::Atan2(Local.Y, Local.X));	// [-180, 180]
	if (Angle < 0.f)
	{
		Angle += 360.f;	// [0, 360)
	}

	const float Step = 360.f / SliceCount;
	return FMath::Clamp(FMath::FloorToInt32(Angle / Step), 0, SliceCount - 1);
}

void ABossRaidGameState::MarkSliceDestroyed(int32 SliceIndex)
{
	if (!HasAuthority() || SliceIndex < 0 || SliceIndex >= SliceCount)
	{
		return;
	}
	if (IsSliceDestroyed(SliceIndex))
	{
		return;
	}

	DestroyedSliceMask |= (1 << SliceIndex);
	ForceNetUpdate();

	// OnRep 은 서버에서 안 불리므로 서버도 직접 방송
	OnArenaSlicesChanged.Broadcast();
}

void ABossRaidGameState::OnRep_DestroyedSliceMask()
{
	OnArenaSlicesChanged.Broadcast();
}
