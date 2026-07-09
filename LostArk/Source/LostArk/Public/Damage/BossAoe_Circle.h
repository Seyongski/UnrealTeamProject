// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Damage/BossPatternActorBase.h"
#include "BossAoe_Circle.generated.h"

/**
 * 원형/도넛 판정 장판.
 * - InnerRadius > 0 이면 도넛(안쪽 InnerRadius 는 안전지대)
 * 새 도형을 추가하려면 이 클래스처럼 IsInsideShape/BuildTelegraph 만 구현하면 됨.
 */
UCLASS(Blueprintable)
class LOSTARK_API ABossAoe_Circle : public ABossPatternActorBase
{
	GENERATED_BODY()

public:
	/** 바깥 반지름(cm) */
	UPROPERTY(EditDefaultsOnly, Category = "Aoe|Shape", meta = (ClampMin = "0.0"))
	float Radius = 300.f;

	/** 안쪽 반지름(cm). >0 이면 도넛 */
	UPROPERTY(EditDefaultsOnly, Category = "Aoe|Shape", meta = (ClampMin = "0.0"))
	float InnerRadius = 0.f;

protected:
	virtual bool IsInsideShape(const FVector& WorldPoint) const override;
	virtual void BuildTelegraph() override;
	virtual void DebugDrawShape() const override;
	virtual void ConfigureTelegraphEffect(UNiagaraComponent* NiagaraComp) const override;
};
