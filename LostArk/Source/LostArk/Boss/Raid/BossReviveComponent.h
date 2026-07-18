// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "BossReviveComponent.generated.h"

class UAbilitySystemComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnReviveStateChanged, bool, bDead);

/**
 * 플레이어 부활 컴포넌트 (레이드 조우 시 BossRaidGameMode 가 각 플레이어 폰에 런타임 부착, 복제).
 *
 * 사망 감지: 플레이어 ASC 의 State.Dead 태그 (체력 0 사망 / 낙사(ArenaKillVolume) 공통 경로).
 *  - AutoReviveDelay(기본 30초) 경과 시 자동 부활
 *  - ManualReviveDelay(개발자 지정) 경과 시부터 UI '부활' 버튼 클릭으로 즉시 부활 가능
 *
 * 부활 처리 (서버):
 *  - State.Dead 제거(로컬+복제) + 체력 회복 + 캐릭터 Revive()(콜리전/이동 복구)
 *  - 낙사 대비 아레나 중심으로 텔레포트 (bReviveAtArenaCenter)
 *
 * UI 바인딩 (사망 화면 위젯):
 *  - OnReviveStateChanged : 사망/부활 시 위젯 표시/숨김
 *  - GetAutoReviveRemaining / GetManualReviveRemaining : 카운트다운 표기 (틱 폴링)
 *  - RequestManualRevive : '부활' 버튼 OnClicked -> 클라에서 호출하면 서버 RPC 로 전달
 */
UCLASS(Blueprintable, ClassGroup = (Boss), meta = (BlueprintSpawnableComponent))
class LOSTARK_API UBossReviveComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBossReviveComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 사망 후 자동 부활까지의 시간(초) */
	UPROPERTY(EditDefaultsOnly, Category = "Revive", meta = (ClampMin = "1.0"))
	float AutoReviveDelay = 30.f;

	/** 사망 후 이 시간(초)이 지나면 '직접 클릭 부활' 버튼 활성화 (개발자 지정) */
	UPROPERTY(EditDefaultsOnly, Category = "Revive", meta = (ClampMin = "0.0"))
	float ManualReviveDelay = 5.f;

	/** 부활 시 회복 체력 비율 (1 = 최대 체력) */
	UPROPERTY(EditDefaultsOnly, Category = "Revive", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float ReviveHealthPercent = 1.f;

	/** 부활 시 아레나 중심으로 텔레포트 (낙사 시체가 맵 밖에 있는 경우 필수) */
	UPROPERTY(EditDefaultsOnly, Category = "Revive")
	bool bReviveAtArenaCenter = true;

	/** 현재 사망 상태인지 (복제 -> 클라 UI) */
	UFUNCTION(BlueprintPure, Category = "Revive")
	bool IsDeadState() const { return bDeadState; }

	/** 자동 부활까지 남은 시간(초). 생존 중이면 0 */
	UFUNCTION(BlueprintPure, Category = "Revive")
	float GetAutoReviveRemaining() const;

	/** 클릭 부활 가능까지 남은 시간(초). 0 이하면 버튼 활성화 */
	UFUNCTION(BlueprintPure, Category = "Revive")
	float GetManualReviveRemaining() const;

	/** '부활' 버튼 클릭 (클라/서버 어디서 불러도 안전 — 클라면 서버 RPC 로 전달) */
	UFUNCTION(BlueprintCallable, Category = "Revive")
	void RequestManualRevive();

	/** 사망(true)/부활(false) 방송 — 사망 화면 위젯 표시/숨김 바인딩 */
	UPROPERTY(BlueprintAssignable, Category = "Revive")
	FOnReviveStateChanged OnReviveStateChanged;

protected:
	virtual void BeginPlay() override;

	UFUNCTION(Server, Reliable)
	void ServerRequestManualRevive();

private:
	UAbilitySystemComponent* GetOwnerASC() const;

	/** 서버 기준 현재 시각 (GameState 동기 시계 — 클라 카운트다운 계산용) */
	float GetServerNow() const;

	/** State.Dead 태그 변화 감시 (서버) */
	void HandleDeadTagChanged(const FGameplayTag Tag, int32 NewCount);

	/** 실제 부활 처리 (서버) */
	void DoRevive();

	UFUNCTION()
	void OnRep_DeadState();

	UPROPERTY(ReplicatedUsing = OnRep_DeadState)
	bool bDeadState = false;

	/** 사망 시각 (서버 월드 시간 기준, 복제 — 클라 카운트다운) */
	UPROPERTY(Replicated)
	float DeathServerTime = 0.f;

	FTimerHandle AutoReviveTimer;
};
