// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BossChargeGaugeComponent.generated.h"

class ABossPatternActorBase;
class UAbilitySystemComponent;
class UGameplayEffect;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnChargeGaugeChanged, float, NewGauge, float, MaxGauge);

/**
 * 플레이어 전하 게이지 (레이드 조우 시 BossRaidGameMode 가 각 플레이어 폰에 런타임 부착, 복제).
 *
 * 규칙:
 *  - '게이지를 채우는' 보스 패턴(UBossAoeChargeGaugeEffect 행동을 단 장판)에 맞으면 게이지 상승
 *  - 절반(50%)을 '넘는 순간' 1회: 전하 반전 (Plus<->Minus = Red<->Blue GE 스왑)
 *  - 가득 차는 순간: 게이지/전하는 그대로 두고, 이 캐릭터를 따라다니는
 *    거대 원형 장판(OverchargeAoeClass)이 예고 후 폭발 (과충전 페널티)
 *
 * UI 바인딩:
 *  - 발밑 게이지 바 : OnChargeGaugeChanged 델리게이트 (Gauge 는 복제되어 클라에서도 발동)
 *  - 머리 위 전하 아이콘 : 기존 State.Charge.Red / Blue 태그 (전하 GE가 부여, 복제됨)
 */
UCLASS(Blueprintable, ClassGroup = (Boss), meta = (BlueprintSpawnableComponent))
class LOSTARK_API UBossChargeGaugeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBossChargeGaugeComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 부착 직후 GameMode 가 호출: 보스(과충전 장판 시전자) + 전하 GE 클래스 주입 */
	void InitChargeGauge(AActor* InBoss,
		TSubclassOf<UGameplayEffect> InRedChargeEffect, TSubclassOf<UGameplayEffect> InBlueChargeEffect);

	/** 게이지 증감 (서버). 절반 상향 돌파 시 전하 반전, 가득 차는 순간 과충전 장판 */
	UFUNCTION(BlueprintCallable, Category = "Boss|Charge")
	void AddGauge(float Amount);

	/** 게이지 0으로 리셋 (서버. 기믹 연출 등 외부에서 필요 시) */
	UFUNCTION(BlueprintCallable, Category = "Boss|Charge")
	void ResetGauge();

	UFUNCTION(BlueprintPure, Category = "Boss|Charge")
	float GetGauge() const { return Gauge; }

	UFUNCTION(BlueprintPure, Category = "Boss|Charge")
	float GetMaxGauge() const { return MaxGauge; }

	/** 게이지 변경 방송 (서버 로직 + 클라 OnRep 양쪽에서 발동 -> 발밑 게이지 UI 바인딩) */
	UPROPERTY(BlueprintAssignable, Category = "Boss|Charge")
	FOnChargeGaugeChanged OnChargeGaugeChanged;

	/** 게이지 최대치 */
	UPROPERTY(EditDefaultsOnly, Category = "Boss|Charge", meta = (ClampMin = "1.0"))
	float MaxGauge = 100.f;

	/**
	 * 과충전(가득) 시 이 캐릭터 기준으로 스폰할 거대 원형 장판.
	 * BP 권장 설정: SpawnOrigin=TargetLocation, TargetingMode=FollowTarget(따라다님),
	 * CastTime=예고 시간, Duration=0(1회 폭발), 큰 Radius.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Boss|Charge|Overcharge")
	TSubclassOf<ABossPatternActorBase> OverchargeAoeClass;

	/** 과충전 장판 공격력계수 (GE SetByCaller Data.Damage) */
	UPROPERTY(EditDefaultsOnly, Category = "Boss|Charge|Overcharge")
	float OverchargeDamageCoefficient = 3.f;

	/** 과충전 폭발 후 게이지를 0으로 리셋할지 (기본: 유지 — '게이지는 변하지 않는다') */
	UPROPERTY(EditDefaultsOnly, Category = "Boss|Charge|Overcharge")
	bool bResetGaugeAfterOvercharge = false;

private:
	UAbilitySystemComponent* GetOwnerASC() const;

	/** 소유 플레이어의 전하 반전 (Red<->Blue GE 스왑 — 변환장판과 동일 로직) */
	void FlipOwnerCharge();

	/** 과충전: 이 캐릭터를 따라다니는 거대 원장판 스폰 */
	void TriggerOvercharge();

	UFUNCTION()
	void OnRep_Gauge();

	/** 현재 게이지 (복제 -> 클라 UI) */
	UPROPERTY(ReplicatedUsing = OnRep_Gauge)
	float Gauge = 0.f;

	/** 과충전 장판 시전자 (보스. 데미지 귀속) */
	TWeakObjectPtr<AActor> Boss;

	TSubclassOf<UGameplayEffect> RedChargeEffect;
	TSubclassOf<UGameplayEffect> BlueChargeEffect;
};
