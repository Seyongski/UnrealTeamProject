// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Damage/BossAoe_Circle.h"
#include "GameplayTagContainer.h"
#include "BossAoe_ChargeSwap.generated.h"

class UGameplayEffect;

/**
 * 전하 변환장판.
 * - 대상 플레이어 '발밑'에 부착되어 따라다니다(TargetingMode=FollowTarget),
 *   CastTime 이 지나 장판이 사라지는 순간 1회 판정 -> 장판 위의 '모든' 플레이어 전하를 뒤집는다.
 *   (장판 주인 포함, 옆에 서 있던 다른 플레이어 포함 = 요구사항 그대로)
 * - 전하 상태 = State.Charge.Red / Blue 태그를 부여하는 무한 GE.
 *   GameMode(조우 시 랜덤 부여)와 '같은 GE 클래스'를 지정해야 제거가 동작한다.
 * - 데미지는 주지 않음 (ApplyEffectsTo 를 스왑 로직으로 대체)
 */
UCLASS()
class LOSTARK_API ABossAoe_ChargeSwap : public ABossAoe_Circle
{
	GENERATED_BODY()

public:
	ABossAoe_ChargeSwap();

	/** 빨간 전하 GE (State.Charge.Red 부여, Infinite). GameMode와 동일 애셋 지정 */
	UPROPERTY(EditDefaultsOnly, Category = "Aoe|Charge")
	TSubclassOf<UGameplayEffect> RedChargeEffect;

	/** 파란 전하 GE (State.Charge.Blue 부여, Infinite). GameMode와 동일 애셋 지정 */
	UPROPERTY(EditDefaultsOnly, Category = "Aoe|Charge")
	TSubclassOf<UGameplayEffect> BlueChargeEffect;

	/** 판정용 태그 (기본 State.Charge.Red / Blue) */
	UPROPERTY(EditDefaultsOnly, Category = "Aoe|Charge")
	FGameplayTag RedTag;

	UPROPERTY(EditDefaultsOnly, Category = "Aoe|Charge")
	FGameplayTag BlueTag;

protected:
	/** 데미지 대신 전하 스왑: Red -> Blue, Blue -> Red */
	virtual void ApplyEffectsTo(AActor* Target) override;
};
