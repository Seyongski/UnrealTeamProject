// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Boss/Notifies/AnimNotify_BossSpawnAoe.h"
#include "AnimNotify_BossSpawnRect.generated.h"

/**
 * 사각(박스) 장판 스폰 노티파이. (BP_AoeRect 하나를 공유)
 * 크기/전방오프셋을 이 노티파이에 직접 입력 -> 패턴마다 다른 박스를 만든다.
 * AoeClass 에는 ABossAoe_Rect 파생(BP_AoeRect)을 지정할 것.
 */
UCLASS(meta = (DisplayName = "Boss Spawn Rect"))
class LOSTARK_API UAnimNotify_BossSpawnRect : public UAnimNotify_BossSpawnAoe
{
	GENERATED_BODY()

public:
	/** 전방(Forward) 반길이 */
	UPROPERTY(EditAnywhere, Category = "Aoe|Rect", meta = (ClampMin = "0.0"))
	float HalfLength = 300.f;

	/** 측면(Right) 반너비 */
	UPROPERTY(EditAnywhere, Category = "Aoe|Rect", meta = (ClampMin = "0.0"))
	float HalfWidth = 150.f;

	/** 중심을 전방으로 미는 오프셋 (보스 앞 직사각형) */
	UPROPERTY(EditAnywhere, Category = "Aoe|Rect")
	float ForwardOffset = 0.f;

protected:
	virtual void ConfigureAoe(ABossPatternActorBase* Aoe) const override;
};
