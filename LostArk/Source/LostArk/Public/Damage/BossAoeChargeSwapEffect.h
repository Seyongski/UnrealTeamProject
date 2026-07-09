// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Damage/BossAoeEffect.h"
#include "GameplayTagContainer.h"
#include "BossAoeChargeSwapEffect.generated.h"

class UGameplayEffect;
class ABossPatternActorBase;

/**
 * 전하 스왑 행동. 판정에 걸린 플레이어의 전하를 뒤집는다 (Red <-> Blue).
 * 도형과 무관 -> 원/사각/부채꼴 어떤 장판 BP 에도 OnHitEffect 로 지정 가능.
 *
 * - 전하 상태 = State.Charge.Red / Blue 를 부여하는 무한 GE.
 *   GameMode(조우 시 랜덤 부여)와 '같은 GE 클래스'를 지정해야 제거가 동작한다.
 * - 데미지는 주지 않음.
 * - 보통 발밑 부착 + CastTime 후 1회 판정 연출: 장판 BP 를
 *   TargetingMode=FollowTarget, SpawnOrigin=TargetLocation, Duration=0 으로 두고 쓴다.
 */
UCLASS(DisplayName = "전하 스왑 (Charge Swap)")
class LOSTARK_API UBossAoeChargeSwapEffect : public UBossAoeEffect
{
	GENERATED_BODY()

public:
	UBossAoeChargeSwapEffect();

	/** 빨간 전하 GE (State.Charge.Red 부여, Infinite). GameMode와 동일 애셋 지정 */
	UPROPERTY(EditDefaultsOnly, Category = "Charge")
	TSubclassOf<UGameplayEffect> RedChargeEffect;

	/** 파란 전하 GE (State.Charge.Blue 부여, Infinite). GameMode와 동일 애셋 지정 */
	UPROPERTY(EditDefaultsOnly, Category = "Charge")
	TSubclassOf<UGameplayEffect> BlueChargeEffect;

	/** 판정용 태그 (기본 State.Charge.Red / Blue) */
	UPROPERTY(EditDefaultsOnly, Category = "Charge")
	FGameplayTag RedTag;

	UPROPERTY(EditDefaultsOnly, Category = "Charge")
	FGameplayTag BlueTag;

	virtual void OnHit(ABossPatternActorBase* Aoe, AActor* Target) override;
};
