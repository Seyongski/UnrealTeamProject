// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "BossRaidGameState.generated.h"

class UUserWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnArenaSlicesChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRaidCleared);

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

	/** 레이드 클리어 방송 (서버/클라 모두. 체력바 숨김/연출 등이 구독) */
	UPROPERTY(BlueprintAssignable, Category = "Raid|Clear")
	FOnRaidCleared OnRaidCleared;

	/**
	 * 클리어 배너 위젯 (WBP_RaidClear — GameState BP 에서 지정).
	 * 클리어 복제 시 각 로컬 플레이어 화면에 자동 생성되고 ClearWidgetLifetime 후 제거된다.
	 * 미지정이면 화면 디버그 텍스트 폴백 (WBP 없이도 흐름 테스트 가능).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Raid|Clear")
	TSubclassOf<UUserWidget> ClearWidgetClass;

	/** 클리어 배너 유지 시간(초). 지나면 자동 RemoveFromParent (0이면 위젯이 스스로 관리) */
	UPROPERTY(EditDefaultsOnly, Category = "Raid|Clear", meta = (ClampMin = "0.0"))
	float ClearWidgetLifetime = 5.f;

	/** 클리어 여부 (복제) */
	UFUNCTION(BlueprintPure, Category = "Raid|Clear")
	bool IsRaidCleared() const { return bRaidCleared; }

	/** 클리어 마킹 (서버 전용. GameMode 클리어 연출 시퀀스가 호출) -> 전 머신 방송 + 배너 표시 */
	void MarkRaidCleared();

	/** 피자 조각 수 (레벨별 GameState BP에서 설정) */
	UPROPERTY(EditDefaultsOnly, Replicated, BlueprintReadOnly, Category = "Arena")
	int32 SliceCount = 8;

	/** 아레나 중심 (GameMode가 조우 시작 시 세팅) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Arena")
	FVector ArenaCenter = FVector::ZeroVector;

	/**
	 * 아레나 바닥의 월드 Z (cm). 장판이 바닥에 붙을 때의 기준 높이.
	 * ArenaCenter.Z 는 슬라이스 액터의 피벗 높이라 실제 바닥과 다를 수 있어(피벗이 떠 있는 경우)
	 * 바닥 높이는 이 값으로 따로 지정한다. GameState BP 에서 실제 바닥 Z 를 넣을 것 (예: 521).
	 * 0 이면 '미설정'으로 보고 장판이 시전자 발밑으로 폴백.
	 */
	UPROPERTY(EditDefaultsOnly, Replicated, BlueprintReadWrite, Category = "Arena")
	float ArenaFloorZ = 0.f;

	/** 조각 파괴 여부 */
	UFUNCTION(BlueprintPure, Category = "Arena")
	bool IsSliceDestroyed(int32 SliceIndex) const;

	/** 하나라도 파괴됐는지 (약점포착 판단 등) */
	UFUNCTION(BlueprintPure, Category = "Arena")
	bool IsAnySliceDestroyed() const { return DestroyedSliceMask != 0; }

	/** 월드 좌표가 속한 조각 인덱스 (0 = +X 축부터 반시계, 각 조각 = 360/SliceCount 도) */
	UFUNCTION(BlueprintPure, Category = "Arena")
	int32 GetSliceIndexAt(const FVector& WorldLocation) const;

	/**
	 * 슬라이스 N 중심 방향의 월드 위치 (아레나 중심에서 Radius 만큼). GetSliceIndexAt 의 역함수격.
	 * 보스가 '이번에 파괴되는 슬라이스'를 바라볼 때 등에 사용. 범위 밖 인덱스면 아레나 중심 반환.
	 */
	UFUNCTION(BlueprintPure, Category = "Arena")
	FVector GetSliceCenterLocation(int32 SliceIndex, float Radius = 500.f) const;

	/** 조각 파괴 마킹 (서버 전용. GameMode::DestroySlice 가 호출) */
	void MarkSliceDestroyed(int32 SliceIndex);

protected:
	/** 파괴 상태 비트마스크 (bit N = 조각 N 파괴) */
	UPROPERTY(ReplicatedUsing = OnRep_DestroyedSliceMask, BlueprintReadOnly, Category = "Arena")
	int32 DestroyedSliceMask = 0;

	UFUNCTION()
	void OnRep_DestroyedSliceMask();

	/** 클리어 여부 (bit 아님 — 라운드당 1회) */
	UPROPERTY(ReplicatedUsing = OnRep_RaidCleared, BlueprintReadOnly, Category = "Raid|Clear")
	bool bRaidCleared = false;

	UFUNCTION()
	void OnRep_RaidCleared();

private:
	/** 공통 클리어 처리: 방송 + 로컬 배너 (서버 마킹/클라 OnRep 양쪽에서 호출) */
	void HandleRaidCleared();

	/** 이 머신의 로컬 플레이어 화면마다 클리어 배너 생성 (데디 서버는 로컬 플레이어 없음 -> no-op) */
	void ShowClearBannerLocally();

	/** 배너 수명 종료 -> 제거 */
	void RemoveClearWidgets();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UUserWidget>> ActiveClearWidgets;

	FTimerHandle ClearWidgetTimer;
};
