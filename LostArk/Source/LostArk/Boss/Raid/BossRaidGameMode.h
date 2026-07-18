// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Core/LostArkGameMode.h"
#include "BossRaidGameMode.generated.h"

class ABossBase;
class ABossArenaCamera;
class UBossChargeGaugeComponent;
class UBossReviveComponent;
class UGameplayEffect;

/**
 * 보스 레이드 GameMode (서버 전용 규칙).
 * 레벨 World Settings 에서 Override -> 그 보스 레벨에서만 적용된다.
 * 보스별로 이 클래스를 상속(BP)해 전하 GE/카메라/조각 수 등을 다르게 설정.
 *
 * 역할: 조우 시작(전하 랜덤 부여 + 아레나 카메라 전환), 지형 파괴 명령(+약점포착),
 *       조우 종료(카메라 복귀). 클라가 읽을 상태는 전부 BossRaidGameState 로.
 */
UCLASS()
class LOSTARK_API ABossRaidGameMode : public ALostArkGameMode
{
	GENERATED_BODY()

public:
	ABossRaidGameMode();

	virtual void BeginPlay() override;

	/** 조우 시작: 전하 랜덤 부여 + 아레나 중심 세팅 + 카메라 전환 (트리거/레벨BP/보스 어그로에서 호출) */
	UFUNCTION(BlueprintCallable, Category = "Raid")
	void StartEncounter();

	/** 조우 종료: 카메라를 각자 캐릭터로 복귀 + 아레나 카메라 제거 */
	UFUNCTION(BlueprintCallable, Category = "Raid")
	void EndEncounter();

	/** 피자 조각 파괴 (기믹/패턴에서 호출). 첫 파괴 시 보스에 약점포착 태그 부여 */
	UFUNCTION(BlueprintCallable, Category = "Raid")
	void DestroySlice(int32 SliceIndex);

	/** 생존 플레이어 전원에게 빨강/파랑 전하 랜덤 부여 (이미 있으면 스킵) */
	UFUNCTION(BlueprintCallable, Category = "Raid")
	void AssignRandomCharges();

	/**
	 * 전하 공명 1회 판정: 생존자의 빨강/파랑 인원수를 세서 |차이| 만큼 전원에게 데미지.
	 * 4:4 균등이면 상쇄(무피해), 3:5 -> 차이 2, 0:8 -> 차이 8 로 어긋날수록 아프다.
	 * 기본은 조우 중 주기 자동 실행(ChargeResonanceInterval). 패턴에서 수동 호출도 가능.
	 */
	UFUNCTION(BlueprintCallable, Category = "Raid|Charge")
	void ApplyChargeResonancePulse();

protected:
	/** 빨간 전하 GE (State.Charge.Red 부여, Infinite). 변환장판과 동일 애셋 사용 */
	UPROPERTY(EditDefaultsOnly, Category = "Raid|Charge")
	TSubclassOf<UGameplayEffect> RedChargeEffect;

	/** 파란 전하 GE (State.Charge.Blue 부여, Infinite). 변환장판과 동일 애셋 사용 */
	UPROPERTY(EditDefaultsOnly, Category = "Raid|Charge")
	TSubclassOf<UGameplayEffect> BlueChargeEffect;

	/**
	 * 전하 공명 데미지 GE (SetByCaller Data.Damage 로 크기 전달). 미지정 시 공명 비활성.
	 * 크기 = |빨강 인원 - 파랑 인원| x ResonanceDamagePerImbalance
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Raid|Charge")
	TSubclassOf<UGameplayEffect> ResonanceDamageEffect;

	/** 전하 공명 주기(초). 0 이면 자동 펄스 없음 (패턴에서 수동 호출만) */
	UPROPERTY(EditDefaultsOnly, Category = "Raid|Charge", meta = (ClampMin = "0.0"))
	float ChargeResonanceInterval = 5.f;

	/** 인원 차이 1당 데미지 계수 (GE SetByCaller Data.Damage 에 곱해 전달) */
	UPROPERTY(EditDefaultsOnly, Category = "Raid|Charge", meta = (ClampMin = "0.0"))
	float ResonanceDamagePerImbalance = 1.f;

	/** 조우 시 플레이어에게 부착할 전하 게이지 컴포넌트 (BP 서브클래스로 게이지/과충전 장판 설정) */
	UPROPERTY(EditDefaultsOnly, Category = "Raid|Charge")
	TSubclassOf<UBossChargeGaugeComponent> ChargeGaugeComponentClass;

	/** 조우 시 플레이어에게 부착할 부활 컴포넌트 (BP 서브클래스로 30초/수동 대기 등 설정) */
	UPROPERTY(EditDefaultsOnly, Category = "Raid|Revive")
	TSubclassOf<UBossReviveComponent> ReviveComponentClass;

	/** 보스 레벨 전용 카메라 클래스. 미지정 시 카메라 전환 없음 */
	UPROPERTY(EditDefaultsOnly, Category = "Raid|Camera")
	TSubclassOf<ABossArenaCamera> ArenaCameraClass;

	/** 카메라 전환 블렌드 시간(초) */
	UPROPERTY(EditDefaultsOnly, Category = "Raid|Camera", meta = (ClampMin = "0.0"))
	float CameraBlendTime = 1.f;

	/** 테스트용: BeginPlay 후 자동으로 StartEncounter 호출 (실전은 off, 트리거에서 호출) */
	UPROPERTY(EditDefaultsOnly, Category = "Raid|Debug")
	bool bAutoStartOnBeginPlay = false;

	/** 자동 시작 지연(초). 플레이어 폰 possess 이후여야 카메라 뷰타겟이 안 밀림 */
	UPROPERTY(EditDefaultsOnly, Category = "Raid|Debug", meta = (EditCondition = "bAutoStartOnBeginPlay", ClampMin = "0.0"))
	float AutoStartDelay = 0.5f;

private:
	ABossBase* FindBoss() const;
	void ApplyChargeTo(APawn* Pawn);

	/** 전하 게이지/부활 컴포넌트를 플레이어 폰에 부착 (조우 시작 시. 이미 있으면 스킵) */
	void SetupRaidComponentsForPlayers();

	UPROPERTY(Transient)
	TObjectPtr<ABossArenaCamera> ArenaCamera;

	bool bEncounterStarted = false;

	FTimerHandle ResonanceTimer;
};
