// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Boss/Damage/BossPatternActorBase.h"
#include "BossAoe_Rect.generated.h"

/**
 * 사각(박스) 판정 장판. 방향은 스폰 시 캐싱된 Forward/Right 기준.
 * - HalfLength : 전방(Forward) 반길이
 * - HalfWidth  : 측면(Right) 반너비
 * - ForwardOffset : 중심을 전방으로 밀어 '보스 앞 직사각형' 표현 (0이면 중심 대칭)
 *   (좌우로 미는 건 베이스의 ShapeOffset 을 쓴다 — 모든 도형 공통)
 * 도형 확장 예시: IsInsideShape/BuildTelegraph 두 개만 구현.
 */
UCLASS(Blueprintable)
class LOSTARK_API ABossAoe_Rect : public ABossPatternActorBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Replicated, Category = "Aoe|Shape", meta = (ClampMin = "0.0"))
	float HalfLength = 300.f;

	UPROPERTY(EditDefaultsOnly, Replicated, Category = "Aoe|Shape", meta = (ClampMin = "0.0"))
	float HalfWidth = 150.f;

	UPROPERTY(EditDefaultsOnly, Replicated, Category = "Aoe|Shape")
	float ForwardOffset = 0.f;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool IsInsideShape(const FVector& WorldPoint) const override;
	virtual void BuildTelegraph() override;
	virtual void DebugDrawShape() const override;
	virtual void ConfigureTelegraphEffect(UNiagaraComponent* NiagaraComp) const override;
};
