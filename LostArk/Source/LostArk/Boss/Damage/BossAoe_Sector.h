// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Boss/Damage/BossPatternActorBase.h"
#include "BossAoe_Sector.generated.h"

/**
 * 부채꼴(환형 부채꼴) 판정 장판.
 * - 방향 기준: ShapeForward = 0도, 오른쪽 = +, 왼쪽 = - (액터 Right 축)
 * - StartAngle~EndAngle 사이의 각도만 판정 (예: -60~60 대칭, 0~90 한쪽, 30~120 비대칭)
 * - InnerRadius > 0 이면 안쪽이 비어 "도넛 조각" (환형 부채꼴)
 * 확장 예시: IsInsideShape/BuildTelegraph 두 개만 구현.
 */
UCLASS(Blueprintable)
class LOSTARK_API ABossAoe_Sector : public ABossPatternActorBase
{
	GENERATED_BODY()

public:
	/** 바깥 반지름(cm) */
	UPROPERTY(EditDefaultsOnly, Replicated, Category = "Aoe|Shape", meta = (ClampMin = "0.0"))
	float Radius = 500.f;

	/** 안쪽 반지름(cm). >0 이면 도넛 조각(안쪽 안전지대) */
	UPROPERTY(EditDefaultsOnly, Replicated, Category = "Aoe|Shape", meta = (ClampMin = "0.0"))
	float InnerRadius = 0.f;

	/** 시작 각도(도). 전방=0, 오른쪽 +, 왼쪽 - */
	UPROPERTY(EditDefaultsOnly, Replicated, Category = "Aoe|Shape", meta = (ClampMin = "-360.0", ClampMax = "360.0"))
	float StartAngle = -45.f;

	/** 끝 각도(도). StartAngle 보다 커야 하며 (End-Start) <= 360 */
	UPROPERTY(EditDefaultsOnly, Replicated, Category = "Aoe|Shape", meta = (ClampMin = "-360.0", ClampMax = "360.0"))
	float EndAngle = 45.f;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool IsInsideShape(const FVector& WorldPoint) const override;
	virtual void BuildTelegraph() override;
	virtual void DebugDrawShape() const override;
	virtual void ConfigureTelegraphEffect(UNiagaraComponent* NiagaraComp) const override;

	/**
	 * 넉백 SweepSide/SweepSideBack 용 방향.
	 * 경계 각도(끝 또는 시작)의 반지름 방향에 직교(±90도)하는 수평 방향을 돌려준다
	 * = 부채꼴이 StartAngle -> EndAngle 로 훑을 때 맞은 사람이 쓸려나가는 쪽.
	 */
	virtual bool GetSweepPushDirection(bool bReverse, FVector& OutDir) const override;
};
