// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_BossScriptedTurn.generated.h"

/**
 * 이 구간 동안 보스를 '직전 추적 회전과 같은 방향'으로 초당 TurnRate(도)만큼 등속 회전시킨다.
 * 방향 부호는 타겟팅 컴포넌트가 TrackTarget(추적) 구간에서 캡처한 값을 재사용
 * -> 플레이어를 쫓느라 시계방향으로 돌았으면 이 회전도 시계방향, 반시계면 반시계.
 * (서버에서만 회전 -> 액터 회전 복제로 클라 전파. TrackTarget 구간과 겹치면 추적이 우선)
 */
UCLASS(meta = (DisplayName = "Boss Scripted Turn"))
class LOSTARK_API UAnimNotifyState_BossScriptedTurn : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	/** 초당 회전량(도). 크기만 지정 — 방향(부호)은 직전 추적 회전에서 자동 결정 */
	UPROPERTY(EditAnywhere, Category = "Boss", meta = (ClampMin = "0.0"))
	float TurnRate = 90.f;

	/** 추적 이력이 없어 방향 미정일 때 쓸 폴백 부호. +1 / -1 (UE yaw 기준 +가 어느 쪽인지는 게임에서 확인) */
	UPROPERTY(EditAnywhere, Category = "Boss")
	float FallbackTurnSign = 1.f;
};
