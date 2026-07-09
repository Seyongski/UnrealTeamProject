// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "BossRaidGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnArenaSlicesChanged);

/**
 * 보스 레이드 GameState.
 * GameMode는 서버 전용이므로, 클라 UI/연출이 읽어야 하는 레이드 상태는 여기(복제)에 둔다.
 *  - 지형(피자 조각) 파괴 마스크
 *  - 아레나 중심/조각 수 (조각 판정 수식용)
 * 보스 '자신'의 게이지(체력/무력화 등)는 보스 AttributeSet 이 담당 (여기 두지 않는다).
 */
UCLASS()
class LOSTARK_API ABossRaidGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 파괴 시 방송 (서버/클라 모두. 슬라이스 액터·UI가 구독) */
	UPROPERTY(BlueprintAssignable, Category = "Arena")
	FOnArenaSlicesChanged OnArenaSlicesChanged;

	/** 피자 조각 수 (레벨별 GameState BP에서 설정) */
	UPROPERTY(EditDefaultsOnly, Replicated, BlueprintReadOnly, Category = "Arena")
	int32 SliceCount = 8;

	/** 아레나 중심 (GameMode가 조우 시작 시 세팅) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Arena")
	FVector ArenaCenter = FVector::ZeroVector;

	/** 조각 파괴 여부 */
	UFUNCTION(BlueprintPure, Category = "Arena")
	bool IsSliceDestroyed(int32 SliceIndex) const;

	/** 하나라도 파괴됐는지 (약점포착 판단 등) */
	UFUNCTION(BlueprintPure, Category = "Arena")
	bool IsAnySliceDestroyed() const { return DestroyedSliceMask != 0; }

	/** 월드 좌표가 속한 조각 인덱스 (0 = +X 축부터 반시계, 각 조각 = 360/SliceCount 도) */
	UFUNCTION(BlueprintPure, Category = "Arena")
	int32 GetSliceIndexAt(const FVector& WorldLocation) const;

	/** 조각 파괴 마킹 (서버 전용. GameMode::DestroySlice 가 호출) */
	void MarkSliceDestroyed(int32 SliceIndex);

protected:
	/** 파괴 상태 비트마스크 (bit N = 조각 N 파괴) */
	UPROPERTY(ReplicatedUsing = OnRep_DestroyedSliceMask, BlueprintReadOnly, Category = "Arena")
	int32 DestroyedSliceMask = 0;

	UFUNCTION()
	void OnRep_DestroyedSliceMask();
};
