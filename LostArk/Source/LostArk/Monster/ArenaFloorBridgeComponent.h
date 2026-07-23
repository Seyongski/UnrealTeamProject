// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ArenaFloorBridgeComponent.generated.h"

class ABossRaidGameState;

/** 슬라이스 파괴 방송 (SliceIndex). BlueprintAssignable -> 소유 액터(BP)의 Events 패널에서 바인드 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnArenaSliceDestroyed, int32, SliceIndex);

/**
 * 지형 파괴 브리지 (레벨 <-> 보스 파괴 파이프라인 연결).
 *
 * 보스 노티파이(AnimNotify_BossGimmickDestroySlice) -> BossTerrainGimmickComponent
 * -> GameMode::DestroySlice -> GameState::MarkSliceDestroyed 로 이어지는 기존 파괴 흐름은
 * 결과를 GameState 의 복제 마스크 + OnArenaSlicesChanged 방송으로만 알린다.
 *
 * 이 컴포넌트를 레벨의 바닥 관리 액터(BP_MordoomArenaManager)에 붙이면, 그 방송을 대신 듣고
 * '이번에 파괴된 슬라이스 인덱스'를 OnSliceDestroyed 델리게이트로 한 번씩 방송한다. BP 는 그
 * 인덱스에 해당하는 BP_MordoomFloorPiece 를 찾아 BreakFloor() 만 호출하면 된다.
 *
 * 멀티플레이 안전:
 *  - 마스크는 복제되고 방송은 서버(직접)와 클라(OnRep) 양쪽에서 발동한다. 따라서 이 컴포넌트를
 *    붙인 액터는 '모든 머신에 존재하는 레벨 배치 액터'여야 각 머신이 자기 바닥을 로컬로 무너뜨린다.
 *  - 각 슬라이스에 대해 OnSliceDestroyed 는 정확히 1회만 발동(FiredMask 로 중복 방지).
 *  - 늦게 접속/리로드한 머신도 바인드 시점의 이미-파괴된 슬라이스를 즉시 재생한다.
 */
UCLASS(ClassGroup = (Arena), meta = (BlueprintSpawnableComponent))
class LOSTARK_API UArenaFloorBridgeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UArenaFloorBridgeComponent();

	/**
	 * 슬라이스가 파괴될 때마다 슬라이스 인덱스를 넘겨 1회 방송 (서버+클라).
	 * 이 컴포넌트를 붙인 BP(BP_MordoomArenaManager)의 Details > Events 패널에서 + 로 바인드해,
	 * 이 인덱스에 대응하는 바닥 조각의 BreakFloor() 를 호출한다.
	 * PieceID 하드매핑 대신 GetSliceIndexForLocation(조각 위치) 로 대응 조각을 찾는 것을 권장.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Arena|Floor")
	FOnArenaSliceDestroyed OnSliceDestroyed;

	/**
	 * 월드 위치가 속한 슬라이스 인덱스 (GameState 규약 그대로: +X 축=0, 반시계, 각 조각=360/SliceCount 도).
	 * 바닥 조각의 월드 위치를 넣어 '이 조각이 이번에 파괴되는 조각인가'를 매핑 테이블 없이 판정.
	 * GameState 미준비 시 INDEX_NONE.
	 */
	UFUNCTION(BlueprintPure, Category = "Arena|Floor")
	int32 GetSliceIndexForLocation(const FVector& WorldLocation) const;

	/** 현재까지 파괴된 슬라이스인지 (직접 조회용) */
	UFUNCTION(BlueprintPure, Category = "Arena|Floor")
	bool IsSliceDestroyed(int32 SliceIndex) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	ABossRaidGameState* GetRaidGameState() const;

	/** GameState 복제가 늦으면(특히 클라) 잠시 후 재시도해 바인드 (AArenaSliceActor 와 동일 관용구) */
	void TryBind();

	UFUNCTION()
	void HandleSlicesChanged();

	FTimerHandle BindRetryTimer;

	/** 이미 OnSliceDestroyed 를 발동한 슬라이스 비트 (중복 방지) */
	int32 FiredMask = 0;

	bool bBound = false;
};
