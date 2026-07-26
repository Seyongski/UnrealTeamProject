// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_BossMarkTarget.generated.h"

/**
 * 현재 타겟에게 어그로 표식(State.Player.Marked)을 부여하는 노티파이 (서버).
 * 표식받은 1명 몸통에 마커 위젯이 뜬다. 타겟 선정(Boss Select Target)과 독립이므로
 * 표식을 원하는 기믹 몽타주에서만 이 노티를 배치한다(선정 노티 바로 뒤 권장).
 * 짝은 반드시 'Boss Clear Mark' 로 회수 — 안 그러면 표식이 다음 패턴까지 남는다.
 */
UCLASS(meta = (DisplayName = "Boss Mark Target (표식 켜기)"))
class LOSTARK_API UAnimNotify_BossMarkTarget : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;
};
