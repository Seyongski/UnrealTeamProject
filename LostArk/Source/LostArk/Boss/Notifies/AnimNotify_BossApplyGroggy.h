// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_BossApplyGroggy.generated.h"

class UGameplayEffect;

/**
 * 보스에 그로기 GE 를 적용하는 노티파이 (서버).
 * 저스트가드 성공 분기 스텝의 첫 프레임 등, 몽타주 타이밍으로 그로기를 걸고 싶을 때 배치.
 * (카운터 성공 그로기는 BossCounterComponent 가 자동 적용하므로 이 노티파이 불필요)
 *
 * State.Boss.Groggy 태그가 서므로, 그로기 루프 스텝의 'NOT Groggy' 분기가
 * 지속시간 만료와 함께 종료 스텝으로 넘어간다 (기존 카운터 그로기와 동일 관용구).
 */
UCLASS(meta = (DisplayName = "Boss Apply Groggy"))
class LOSTARK_API UAnimNotify_BossApplyGroggy : public UAnimNotify
{
	GENERATED_BODY()

public:
	UAnimNotify_BossApplyGroggy();

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

	/** 그로기 지속시간(초). GE 에 SetByCaller(Data.Duration)로 전달 */
	UPROPERTY(EditAnywhere, Category = "Groggy", meta = (ClampMin = "0.1"))
	float GroggyDuration = 5.f;

	/** 적용할 그로기 GE (기본 UBossGroggyEffect) */
	UPROPERTY(EditAnywhere, Category = "Groggy")
	TSubclassOf<UGameplayEffect> GroggyEffectClass;
};
