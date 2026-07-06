// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "GA_NormalAttack.generated.h"

/**
 * 
 */
UCLASS()
class LOSTARK_API UGA_NormalAttack : public UGameplayAbility
{
	GENERATED_BODY()

public:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData
	) override;

private:
	//
	UPROPERTY()
	int32 ComboIndex = 1;

	//
	const FGameplayAbilityActorInfo* CachedActorInfo = nullptr;
	FGameplayAbilitySpecHandle CachedHandle;
	FGameplayAbilityActivationInfo CachedActivationInfo;

	//
	UPROPERTY(EditDefaultsOnly, Category = "Attack")
	int32 MaxComboCount = 3;

	UPROPERTY(EditDefaultsOnly, Category = "Attack")
	TArray<UAnimMontage*> AttackMontages;


	//
	void PlayCurrentCombo();

	void AdvanceComboOrFinish();

	void ResetCombo();

	//
	UFUNCTION()
	void OnMontageFinished();

	UFUNCTION()
	void OnMontageInterrupted();

	UFUNCTION()
	void OnMontageCancelled();
};
