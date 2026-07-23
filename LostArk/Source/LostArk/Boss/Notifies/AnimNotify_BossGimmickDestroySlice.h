// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_BossGimmickDestroySlice.generated.h"

/**
 * 이번 기믹 라운드의 대상 슬라이스 파괴를 예약하는 노티파이 (서버).
 * 저스트가드 성공(그로기) / 실패(넉다운 장판) 양쪽 분기 몽타주에 각각 배치하고
 * Delay 로 '일정 시간 후 파괴' 요구사항을 표현한다.
 * 실제 파괴는 UBossTerrainGimmickComponent -> ABossRaidGameMode::DestroySlice (레벨팀 ID 시스템).
 */
UCLASS(meta = (DisplayName = "Boss Gimmick: Destroy Slice"))
class LOSTARK_API UAnimNotify_BossGimmickDestroySlice : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

	/** 노티파이 발동 후 실제 파괴까지의 지연(초). 0이면 즉시 */
	UPROPERTY(EditAnywhere, Category = "Gimmick", meta = (ClampMin = "0.0"))
	float Delay = 2.f;
};
