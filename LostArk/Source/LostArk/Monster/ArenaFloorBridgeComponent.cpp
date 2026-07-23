// Fill out your copyright notice in the Description page of Project Settings.

#include "Monster/ArenaFloorBridgeComponent.h"
#include "Boss/Raid/BossRaidGameState.h"
#include "Engine/World.h"
#include "TimerManager.h"

UArenaFloorBridgeComponent::UArenaFloorBridgeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UArenaFloorBridgeComponent::BeginPlay()
{
	Super::BeginPlay();

	// 컴포넌트 BeginPlay 는 소유 액터의 BP Event BeginPlay 보다 먼저 돈다. 여기서 바로 바인드+동기화하면
	// 이미-파괴 슬라이스 catch-up 방송을, 아직 델리게이트를 바인드하지 않은 매니저가 놓칠 수 있다.
	// -> 다음 틱으로 미뤄 소유 액터의 바인드가 끝난 뒤 최초 동기화가 발동하게 한다.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(this, &UArenaFloorBridgeComponent::TryBind);
	}
}

void UArenaFloorBridgeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BindRetryTimer);
	}
	if (ABossRaidGameState* GS = GetRaidGameState())
	{
		GS->OnArenaSlicesChanged.RemoveDynamic(this, &UArenaFloorBridgeComponent::HandleSlicesChanged);
	}
	Super::EndPlay(EndPlayReason);
}

ABossRaidGameState* UArenaFloorBridgeComponent::GetRaidGameState() const
{
	return GetWorld() ? GetWorld()->GetGameState<ABossRaidGameState>() : nullptr;
}

void UArenaFloorBridgeComponent::TryBind()
{
	if (bBound)
	{
		return;
	}

	if (ABossRaidGameState* GS = GetRaidGameState())
	{
		GS->OnArenaSlicesChanged.AddDynamic(this, &UArenaFloorBridgeComponent::HandleSlicesChanged);
		bBound = true;
		HandleSlicesChanged();	// 늦은 참여/리로드 대비 현재 마스크 즉시 반영
	}
	else if (UWorld* World = GetWorld())
	{
		// 클라에서 GameState 복제가 이 액터 BeginPlay 보다 늦을 수 있음 -> 재시도
		World->GetTimerManager().SetTimer(
			BindRetryTimer, this, &UArenaFloorBridgeComponent::TryBind, 0.25f, false);
	}
}

void UArenaFloorBridgeComponent::HandleSlicesChanged()
{
	ABossRaidGameState* GS = GetRaidGameState();
	if (!GS)
	{
		return;
	}

	// 새로 파괴된 슬라이스만 골라 1회씩 BP 로 통지 (마스크 델타)
	for (int32 Index = 0; Index < GS->SliceCount; ++Index)
	{
		const int32 Bit = 1 << Index;
		if ((FiredMask & Bit) != 0)
		{
			continue;	// 이미 통지함
		}
		if (GS->IsSliceDestroyed(Index))
		{
			FiredMask |= Bit;
			OnSliceDestroyed.Broadcast(Index);
		}
	}
}

int32 UArenaFloorBridgeComponent::GetSliceIndexForLocation(const FVector& WorldLocation) const
{
	const ABossRaidGameState* GS = GetRaidGameState();
	return GS ? GS->GetSliceIndexAt(WorldLocation) : INDEX_NONE;
}

bool UArenaFloorBridgeComponent::IsSliceDestroyed(int32 SliceIndex) const
{
	const ABossRaidGameState* GS = GetRaidGameState();
	return GS && GS->IsSliceDestroyed(SliceIndex);
}
