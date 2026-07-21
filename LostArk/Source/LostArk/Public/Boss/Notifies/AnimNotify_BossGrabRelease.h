// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_BossGrabRelease.generated.h"

/**
 * 잡기 해제 노티파이. 몽타주의 원하는 프레임에 배치하면 그 순간
 * 이 보스가 잡고 있는 모든 잡기 장판(UBossAoeGrabEffect)을 해제한다(데미지+던지기).
 *
 * 사용법: 잡기 장판 BP 의 OnHitEffect(잡기)에서 bAutoRelease=false 로 두고,
 * '던지는 애니메이션'의 릴리즈 프레임에 이 노티파이를 찍는다.
 * -> 잡힌 플레이어가 그 프레임까지 손(소켓)에 붙어 있다가 노티파이 시점에 날아간다.
 */
UCLASS(meta = (DisplayName = "Boss Grab Release"))
class LOSTARK_API UAnimNotify_BossGrabRelease : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override { return TEXT("GrabRelease"); }
};
