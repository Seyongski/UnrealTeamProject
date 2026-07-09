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
};
