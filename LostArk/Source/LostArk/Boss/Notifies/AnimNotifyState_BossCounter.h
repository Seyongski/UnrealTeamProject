// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Boss/Combat/BossCounterComponent.h"	// EBossCounterType
#include "AnimNotifyState_BossCounter.generated.h"

/**
 * 몽타주 구간 동안 보스의 카운터 창을 여는 노티파이 스테이트.
 * 애니메이터가 타임라인에서 카운터 타이밍을 직접 지정하고, 인스턴스에서 종류/인원을 편집한다.
 *
 * - 창 열림/닫힘만 담당. 판정/카운트/결과는 전부 UBossCounterComponent (서버 전용)
 * - 카운터 전조 이펙트는 여기서 터뜨리지 않는다. 이펙트는 별도 이펙트 노티파이로
 *   애니메이터가 원하는 몽타주에만 배치 -> "이펙트 없이 모션만 보고 카운터" 연출 가능
 * - 실패 확정(PatternResult.CounterFailed) 상태면 컴포넌트가 창 열기를 무시하므로,
 *   같은 몽타주를 실패 경로에서 재생해도 창이 살아나지 않는다
 */
UCLASS(meta = (DisplayName = "Boss Counter Window"))
class LOSTARK_API UAnimNotifyState_BossCounter : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

	/** 카운터 종류 (카운터 / 협동 카운터 / 가짜 카운터) */
	UPROPERTY(EditAnywhere, Category = "Counter")
	EBossCounterType CounterType = EBossCounterType::Counter;

	/** 협동 카운터: 성공에 필요한 고유 플레이어 수 */
	UPROPERTY(EditAnywhere, Category = "Counter",
		meta = (ClampMin = "2", EditCondition = "CounterType == EBossCounterType::MultiCounter", EditConditionHides))
	int32 RequiredPlayers = 2;
};
