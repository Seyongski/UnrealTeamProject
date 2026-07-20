// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Boss/Notifies/AnimNotify_BossSpawnAoe.h"
#include "AnimNotify_BossSpawnSector.generated.h"

/**
 * 부채꼴 장판 스폰 노티파이. (BP_AoeSector 하나를 공유)
 * 각도/반지름을 이 노티파이에 직접 입력 -> 패턴(몽타주)마다 다른 부채꼴을 만든다.
 * AoeClass 에는 ABossAoe_Sector 파생(BP_AoeSector)을 지정할 것.
 */
UCLASS(meta = (DisplayName = "Boss Spawn Sector"))
class LOSTARK_API UAnimNotify_BossSpawnSector : public UAnimNotify_BossSpawnAoe
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Aoe|Sector", meta = (ClampMin = "0.0"))
	float Radius = 500.f;

	UPROPERTY(EditAnywhere, Category = "Aoe|Sector", meta = (ClampMin = "0.0"))
	float InnerRadius = 0.f;

	/** 전방=0, 오른쪽 +, 왼쪽 - */
	UPROPERTY(EditAnywhere, Category = "Aoe|Sector", meta = (ClampMin = "-360.0", ClampMax = "360.0"))
	float StartAngle = -45.f;

	UPROPERTY(EditAnywhere, Category = "Aoe|Sector", meta = (ClampMin = "-360.0", ClampMax = "360.0"))
	float EndAngle = 45.f;

protected:
	virtual void ConfigureAoe(ABossPatternActorBase* Aoe) const override;
};
