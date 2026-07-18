// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Boss/Damage/BossAoeEffect.h"
#include "BossAoeChargeGaugeEffect.generated.h"

class ABossPatternActorBase;

/**
 * 전하 게이지 충전 행동. '게이지를 채우는 패턴'의 장판 BP 에 OnHitEffect 로 지정.
 * 판정에 걸린 플레이어의 UBossChargeGaugeComponent 게이지를 올린다
 * (절반 돌파 시 전하 반전 / 가득 시 과충전 폭발은 컴포넌트가 처리).
 * 도형과 무관 -> 원/사각/부채꼴 어떤 장판에도 조합 가능.
 */
UCLASS(DisplayName = "전하 게이지 충전 (Charge Gauge)")
class LOSTARK_API UBossAoeChargeGaugeEffect : public UBossAoeEffect
{
	GENERATED_BODY()

public:
	/** 1회 적중당 게이지 증가량 */
	UPROPERTY(EditDefaultsOnly, Category = "Charge", meta = (ClampMin = "0.0"))
	float GaugeAmount = 25.f;

	/** 게이지와 함께 장판의 기본 데미지+상태이상 GE 도 적용할지 (끄면 게이지만) */
	UPROPERTY(EditDefaultsOnly, Category = "Charge")
	bool bAlsoApplyDamage = true;

	virtual void OnHit(ABossPatternActorBase* Aoe, AActor* Target) override;
};
