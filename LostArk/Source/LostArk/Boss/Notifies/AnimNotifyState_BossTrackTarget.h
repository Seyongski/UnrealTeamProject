// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GameplayTagContainer.h"
#include "AnimNotifyState_BossTrackTarget.generated.h"

/**
 * 이 노티파이 스테이트 구간 동안 보스 ASC에 TrackTarget 태그를 부여한다.
 * 타겟팅 컴포넌트가 이 태그가 있을 때만 타겟 방향으로 회전하므로,
 * 몽타주 타임라인에서 "추적 회전 구간(선딜)"을 애니메이터가 직접 지정할 수 있다.
 * (서버에서만 토글 -> 회전 판정도 서버 전용)
 */
UCLASS(meta = (DisplayName = "Boss Track Target"))
class LOSTARK_API UAnimNotifyState_BossTrackTarget : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UAnimNotifyState_BossTrackTarget();

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	/** 구간 동안 부여할 태그 (기본 State.Boss.TrackTarget) */
	UPROPERTY(EditAnywhere, Category = "Boss")
	FGameplayTag TrackTag;

	/**
	 * 이 구간만 회전 속도를 따로 쓴다. 끄면 타겟팅 컴포넌트의 BP 디폴트 값을 사용.
	 * (패턴마다 "천천히 노려보며 돌기" / "빠르게 스냅" 을 구분하고 싶을 때)
	 */
	UPROPERTY(EditAnywhere, Category = "Boss|Turn Speed")
	bool bOverrideTurnSpeed = false;

	/** 보간 속도 (클수록 빠름). 컴포넌트 기본값 6 */
	UPROPERTY(EditAnywhere, Category = "Boss|Turn Speed", meta = (EditCondition = "bOverrideTurnSpeed", ClampMin = "0.0"))
	float RotationInterpSpeed = 6.f;

	/**
	 * 초당 최대 회전량(도). 0 = 무제한.
	 * "확 도는 느낌"을 잡는 건 보간 속도보다 이 상한이다 (권장 180~360).
	 */
	UPROPERTY(EditAnywhere, Category = "Boss|Turn Speed", meta = (EditCondition = "bOverrideTurnSpeed", ClampMin = "0.0"))
	float MaxTurnRate = 270.f;
};
