#pragma once

#include "CoreMinimal.h"
#include "LostArk/Ability/LostArkSkillGameplayAbility.h"
#include "LostArkShadowDashAbility.generated.h"

UCLASS()
class LOSTARK_API ULostArkShadowDashAbility : public ULostArkSkillGameplayAbility
{
	GENERATED_BODY()

public:
	ULostArkShadowDashAbility();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Shadow")
	TArray<TSoftObjectPtr<UAnimMontage>> SkillSteps;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clone")
	TSoftClassPtr<class ALostArkShadowClone> ShadowCloneClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clone")
	FDamageShapeParams CloneDamageShapeParams;

private:
	UFUNCTION()
	void OnPrepareCompleted();

	UFUNCTION()
	void OnDashCompleted();

	UFUNCTION()
	void SpawnNextShadow();

	UFUNCTION()
	void OnFinishedCompleted();

	UFUNCTION()
	void OnShadowDashInterrupted();

	void PlaySkillStepMontage(int32 Index);

	FVector StartLocation;
	FTimerHandle DashCompleteTimerHandle;
	FTimerHandle ShadowSpawnTimerHandle;
	int32 SpawnedShadowCount;
};




