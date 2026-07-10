#pragma once

#include "CoreMinimal.h"
#include "LostArk/Ability/LostArkSkillGameplayAbility.h"
#include "LostArkBackflipSmashAbility.generated.h"

UCLASS()
class LOSTARK_API ULostArkBackflipSmashAbility : public ULostArkSkillGameplayAbility
{
	GENERATED_BODY()

public:
	ULostArkBackflipSmashAbility();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Phase1_Backflip")
	float BackflipDistance;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Phase1_Backflip")
	float BackflipDuration;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Phase2_BackwardJump")
	float BackwardJumpDistance;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Phase2_BackwardJump")
	float BackwardJumpDuration;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Phase3_Smash")
	float SmashDistance;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Phase3_Smash")
	float SmashDuration;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Phase3_Smash")
	float SmashDelay;

	FTimerHandle BackwardJumpTimerHandle;
	FTimerHandle SmashDelayTimerHandle;

	UFUNCTION()
	void OnBackwardJumpDelayFinished();

	UFUNCTION()
	void OnSmashDelayFinished();
};



