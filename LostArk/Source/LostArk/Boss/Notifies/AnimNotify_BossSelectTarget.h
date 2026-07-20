// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Boss/Targeting/BossTargetingComponent.h"	// ETargetSelectPolicy
#include "AnimNotify_BossSelectTarget.generated.h"

/**
 * 몽타주 중간에 보스 타겟을 재선정하는 노티파이 (서버).
 * 패턴 데이터의 타겟 선정은 '패턴 시작 시 1회'뿐이므로, 한 패턴 안에서 루프를 돌며
 * 매번 다른 랜덤 플레이어를 노리는 흐름(지형파괴 기믹의 레이저 루프 등)은
 * 루프 스텝 몽타주 첫 프레임에 이 노티파이를 배치해 재선정한다.
 */
UCLASS(meta = (DisplayName = "Boss Select Target"))
class LOSTARK_API UAnimNotify_BossSelectTarget : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

	/** 선정 정책 (기믹 레이저는 Random) */
	UPROPERTY(EditAnywhere, Category = "Targeting")
	ETargetSelectPolicy Policy = ETargetSelectPolicy::Random;
};
