// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "BossPatternAbility.generated.h"

class UPatternDataAsset;
class UBossPatternComponent;
class UAbilityTask_PlayMontageAndWait;

/**
 * 패턴 1개(몽타주 스텝 그래프)를 실행하는 GameplayAbility.
 * - 브레인(UBossPatternComponent)이 넣어준 PatternData 의 스텝을 순차 재생
 * - 스텝 재생 '중' 태그 변화를 감지해 분기 조건 충족 시 즉시 몽타주를 끊고 이동
 * - 정상 종료 시엔 분기 평가 -> 없으면 NextStepDefault
 */
UCLASS()
class LOSTARK_API UBossPatternAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UBossPatternAbility();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** 스텝 실행: 윈도우 태그 부여 + 태그감시 등록 + 몽타주 재생 */
	void RunStep(FName StepId);

	/** 현재 스텝의 분기 조건 평가. 매치되면 true + OutNextStepId */
	bool EvaluateBranches(FName& OutNextStepId) const;

	/** 다음 스텝으로 확정 이동(또는 종료). 재진입 방지 가드 포함 */
	void ResolveStep(FName NextStepId);

	/** 현재 스텝 정리 (태그감시 해제, 윈도우 태그 회수, 몽타주 태스크 종료) */
	void CleanupStep();

	/** 패턴 종료 -> 브레인에 보고 */
	void FinishPattern();

	/** 태그가 하나라도 바뀌면 호출 -> 분기 조건 재평가(즉시 인터럽트) */
	void OnAnyTagChanged(const FGameplayTag Tag, int32 NewCount);

	/** 몽타주 정상/중단 종료 콜백 */
	UFUNCTION()
	void OnStepMontageEnded();

	/** 이번 활성화에서 실행 중인 패턴 (브레인에서 주입) */
	UPROPERTY(Transient)
	TObjectPtr<UPatternDataAsset> PatternData;

	/** 소유 보스의 브레인 컴포넌트 */
	UPROPERTY(Transient)
	TObjectPtr<UBossPatternComponent> Brain;

	/** 현재 재생 중인 몽타주 태스크 */
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	FName CurrentStepId;
	bool bStepActive = false;			// 스텝 몽타주 재생 중이고 아직 다음이 정해지지 않음
	FDelegateHandle TagChangeHandle;	// 제네릭 태그 변화 구독 핸들
	FTimerHandle NoMontageTimer;		// 몽타주 미지정 스텝용 폴백 타이머
	FGameplayTagContainer AppliedStepTags;	// 실제로 부여한 스텝 태그 (이중 제거 방지용 기록)
};
