// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Boss/Notifies/AnimNotify_BossSpawnAoe.h"
#include "AnimNotify_BossSpawnCircle.generated.h"

/**
 * 원/도넛 장판 스폰 노티파이. (BP_AoeCircle 하나를 공유)
 * 반지름을 이 노티파이에 직접 입력 -> 패턴마다 다른 원/도넛을 만든다.
 * AoeClass 에는 ABossAoe_Circle 파생(BP_AoeCircle)을 지정할 것.
 */
UCLASS(meta = (DisplayName = "Boss Spawn Circle"))
class LOSTARK_API UAnimNotify_BossSpawnCircle : public UAnimNotify_BossSpawnAoe
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Aoe|Circle", meta = (ClampMin = "0.0"))
	float Radius = 300.f;

	/** >0 이면 도넛(안쪽 안전지대) */
	UPROPERTY(EditAnywhere, Category = "Aoe|Circle", meta = (ClampMin = "0.0"))
	float InnerRadius = 0.f;

protected:
	virtual void ConfigureAoe(ABossPatternActorBase* Aoe) const override;
};
