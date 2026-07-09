// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "GameplayTagContainer.h"
#include "LostArkAIController.generated.h"

/**
 * 몬스터의 행동(대기, 이동, 공격) 의사결정을 수행하는 AI 컨트롤러
 */
UCLASS()
class LOSTARK_API ALostArkAIController : public AAIController
{
	GENERATED_BODY()

public:
	ALostArkAIController();

	void RestartAILogic();

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	/** 주기적으로 대상 캐릭터를 찾고 상태 판단 및 거리에 따른 이동/공격 판단을 처리 */
	void UpdateAIBehavior();

protected:
	/** 의사결정 주기 시간 (초 단위) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	float TargetCheckInterval;

private:
	FTimerHandle AIUpdateTimerHandle;

	// 상태 태그 캐싱용 변수
	FGameplayTag StateSpawningTag;
	FGameplayTag StateIdleTag;
	FGameplayTag StateMovingTag;
	FGameplayTag StateAttackingTag;
	FGameplayTag StateDeadTag;
};
