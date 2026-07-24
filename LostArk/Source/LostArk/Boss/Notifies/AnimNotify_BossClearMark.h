// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_BossClearMark.generated.h"

/**
 * 어그로 표식(State.Player.Marked)을 회수하는 노티파이 (서버).
 * 'Boss Mark Target' 의 짝. 기믹 시퀀스(레이저 → 저스트가드)가 끝나는 프레임에 배치한다.
 * (보스 사망 시에는 ABossBase::HandleDeath 가 ClearMark 를 호출하므로 몽타주가 중단돼도 안전)
 */
UCLASS(meta = (DisplayName = "Boss Clear Mark (표식 끄기)"))
class LOSTARK_API UAnimNotify_BossClearMark : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;
};
