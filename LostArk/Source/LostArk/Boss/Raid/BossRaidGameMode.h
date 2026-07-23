// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Core/LostArkGameMode.h"
#include "BossRaidGameMode.generated.h"

class ABossBase;
class ABossArenaCamera;
class ACameraActor;
class APlayerController;
class UBossChargeGaugeComponent;
class UBossReviveComponent;
class UGameplayEffect;
class USoundBase;

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

	/**
	 * 보스 사망 통지 (ABossBase::HandleDeath 가 호출. 서버, 1회 가드).
	 * 클리어 연출 시퀀스 시작:
	 *  t=0                        : 전 플레이어 뷰타겟 -> 클리어 카메라(보스 정면 줌인) + 글로벌 슬로모
	 *  t=ClearSlomoDuration(실초) : 슬로모 복구
	 *  t=ClearBannerDelay         : GameState.MarkRaidCleared -> 전 머신 클리어 배너
	 *  t=+ClearHoldTime           : EndEncounter (카메라 각자 캐릭터 복귀) + 클리어 카메라 정리
	 */
	void NotifyBossDied(ABossBase* Boss);

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
	/**
	 * 플레이어 폰 possess 완료 콜백 (서버). 조우가 이미 시작된 뒤 늦게 접속/possess 한
	 * 플레이어(리슨서버에서 호스트보다 늦게 join 하는 원격 클라 포함)도 여기서 전하 + 레이드
	 * 컴포넌트를 받는다. StartEncounter 시점에 아직 possess 전이라 누락되던 문제를 메운다.
	 */
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

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

	/**
	 * 초기 전하 부여 후/늦은 접속 후 이 지연(초) 뒤 1회 RebalanceCharges 를 돌려 +/- 인원을 맞춘다.
	 * 부여/복제가 정착된 뒤(타이머) 읽어야 인원수 조회가 신뢰되므로 즉시가 아니라 짧게 늦춘다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Raid|Charge", meta = (ClampMin = "0.0"))
	float ChargeRebalanceDelay = 1.f;

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

	// ─── 클리어 연출 (보스 사망 시. 카메라 줌인 + 슬로모 + 배너 — 레벨 시퀀스 없음) ───

	/** 클리어 카메라: 보스 정면으로부터의 수평 거리(cm) */
	UPROPERTY(EditDefaultsOnly, Category = "Raid|Clear", meta = (ClampMin = "100.0"))
	float ClearCameraDistance = 1300.f;

	/** 클리어 카메라: 높이(cm, 보스 발밑 기준 위) */
	UPROPERTY(EditDefaultsOnly, Category = "Raid|Clear", meta = (ClampMin = "0.0"))
	float ClearCameraHeight = 700.f;

	/** 클리어 카메라가 바라볼 지점 높이(cm, 보스 액터 위치 기준. 가슴/머리 높이로 조정) */
	UPROPERTY(EditDefaultsOnly, Category = "Raid|Clear")
	float ClearCameraFocusHeight = 350.f;

	/** 클리어 카메라 블렌드 시간(초) */
	UPROPERTY(EditDefaultsOnly, Category = "Raid|Clear", meta = (ClampMin = "0.0"))
	float ClearCameraBlendTime = 0.8f;

	/** 사망 순간 글로벌 슬로모 배율 (1이면 슬로모 없음. WorldSettings 복제로 클라도 함께 느려짐) */
	UPROPERTY(EditDefaultsOnly, Category = "Raid|Clear", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float ClearSlomoDilation = 0.35f;

	/** 슬로모 유지 시간 (실제 벽시계 초) */
	UPROPERTY(EditDefaultsOnly, Category = "Raid|Clear", meta = (ClampMin = "0.0"))
	float ClearSlomoDuration = 1.2f;

	/**
	 * 사망 -> 클리어 배너까지 대기(게임시간 초). 사망 몽타주 하이라이트에 맞춰 조정.
	 * 슬로모 구간 동안은 게임시간이 느리게 흘러 실제로는 그만큼 더 늦게 뜬다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Raid|Clear", meta = (ClampMin = "0.0"))
	float ClearBannerDelay = 2.5f;

	/** 배너 표시 후 카메라 복귀(EndEncounter)까지 유지 시간(초) */
	UPROPERTY(EditDefaultsOnly, Category = "Raid|Clear", meta = (ClampMin = "0.0"))
	float ClearHoldTime = 4.f;

	/**
	 * 레벨 진입 시 재생할 BGM (BP_MordumGameMode 에 지정). 미지정 시 재생 없음.
	 * BeginPlay 에서 GameState 로 넘겨 복제 -> 각 클라(리슨 호스트 포함)가 로컬로 1회 재생.
	 * 늦게 접속한 클라도 복제로 자동 재생된다. Looping 은 SoundWave 에셋에서 설정.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Raid|BGM")
	TObjectPtr<USoundBase> LevelBgm;

	/** 테스트용: BeginPlay 후 자동으로 StartEncounter 호출 (실전은 off, 트리거에서 호출) */
	UPROPERTY(EditDefaultsOnly, Category = "Raid|Debug")
	bool bAutoStartOnBeginPlay = false;

	/** 자동 시작 지연(초). 플레이어 폰 possess 이후여야 카메라 뷰타겟이 안 밀림 */
	UPROPERTY(EditDefaultsOnly, Category = "Raid|Debug", meta = (EditCondition = "bAutoStartOnBeginPlay", ClampMin = "0.0"))
	float AutoStartDelay = 0.5f;

	/**
	 * 플레이어 스폰 분산 반경(cm). 여러 명이 한 스폰지점에 겹쳐 나타나면 캡슐끼리 밀어내 한 명이
	 * 튕겨 나가는 문제 방지 — possess 직후 이 반경의 링 위 슬롯으로 흩어 놓는다. 0 이면 분산 끔.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Raid|Spawn", meta = (ClampMin = "0.0"))
	float PlayerSpawnSpacingRadius = 140.f;

private:
	ABossBase* FindBoss() const;

	/** possess 직후 폰을 스폰지점 주변 링 슬롯으로 흩어 캡슐 겹침(튕김)을 방지 (서버) */
	void SpaceOutSpawn(APawn* Pawn);

	/**
	 * 한 명에게 전하 부여 (이미 있으면 스킵). 다른 플레이어 상태를 읽지 않고 '이 플레이어 자신의
	 * PlayerId 홀짝'으로만 +/- 를 정한다 -> 공유 카운터/타인 태그 조회 불필요라 접속 순서·복제
	 * 타이밍·PIE 격리와 무관하게 항상 균형. PlayerId 는 접속 순서대로 연속 부여되므로 홀짝이
	 * 교대해 2명=1:1, 3명=2:1, 4명=2:2 가 된다.
	 */
	void AssignBalancedChargeTo(APawn* Pawn);

	/** 이 폰에 빨강(+)을 줄지 여부 = PlayerId 짝수. (초기 부여용 임시값 — 최종 균형은 RebalanceCharges 가 보장) */
	bool ShouldGiveRedCharge(const APawn* Pawn) const;

	/**
	 * 정착된 전하를 세서 |빨강-파랑| <= 1 이 되도록 많은 쪽 일부를 반대로 뒤집는다 (서버).
	 * 타이머로 늦게 실행되므로 부여/복제가 정착돼 인원수 조회가 신뢰된다 (공명 카운트와 동일 원리).
	 * 2명 -> 1:1, 3명 -> 2:1, 4명 -> 2:2. 접속 순서/PlayerId 간격과 무관하게 항상 균형.
	 */
	void RebalanceCharges();

	/** RebalanceCharges 를 ChargeRebalanceDelay 뒤 1회 예약 (재호출 시 디바운스 -> 마지막 접속 기준 1회) */
	void ScheduleChargeRebalance();

	/** 전 플레이어 컨트롤러의 뷰타겟 일괄 전환. NewViewTarget=null 이면 각자 자기 폰으로 복귀 */
	void SetViewTargetForAll(AActor* NewViewTarget, float BlendTime);

	/** 전하 게이지/부활 컴포넌트를 플레이어 폰에 부착 (조우 시작 시. 이미 있으면 스킵) */
	void SetupRaidComponentsForPlayers();

	/** 폰 1개에 전하 게이지/부활 컴포넌트 부착 (이미 있으면 스킵). 늦게 join 한 플레이어에도 재사용 */
	void SetupRaidComponentsForPawn(APawn* Pawn, ABossBase* Boss);

	/** 클리어 배너 시점: GameState 에 클리어 마킹 (복제 -> 전 머신 배너) */
	void ShowClearBanner();

	/** 슬로모 복구 (타임 딜레이션 1로) */
	void RestoreTimeDilation();

	/** 연출 종료: 슬로모 복구 보증 + EndEncounter + 클리어 카메라 정리 */
	void FinishClearSequence();

	UPROPERTY(Transient)
	TObjectPtr<ABossArenaCamera> ArenaCamera;

	/** 클리어 연출용 고정 카메라 (복제 스폰 — 원격 클라 뷰타겟 지정용) */
	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> ClearCamera;

	bool bEncounterStarted = false;
	bool bBossDied = false;	// NotifyBossDied 1회 가드

	// 스폰 분산: 첫 플레이어 스폰 위치를 링 중심으로 잡고, 접속 순서대로 슬롯을 하나씩 배정
	FVector PartySpawnOrigin = FVector::ZeroVector;
	bool bPartySpawnOriginSet = false;
	int32 SpawnSlotIndex = 0;

	FTimerHandle ResonanceTimer;
	FTimerHandle ChargeRebalanceTimer;
	FTimerHandle ClearSlomoTimer;
	FTimerHandle ClearBannerTimer;
	FTimerHandle ClearEndTimer;
};
