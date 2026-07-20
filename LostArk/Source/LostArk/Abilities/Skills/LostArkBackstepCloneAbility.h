#pragma once

#include "CoreMinimal.h"
#include "Abilities/LostArkSkillGameplayAbility.h"
#include "LostArkBackstepCloneAbility.generated.h"

class ALostArkShadowClone;

UCLASS()
class LOSTARK_API ULostArkBackstepCloneAbility : public ULostArkSkillGameplayAbility
{
	GENERATED_BODY()

public:
	ULostArkBackstepCloneAbility();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clone")
	TSoftClassPtr<ALostArkShadowClone> ShadowCloneClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clone")
	FDamageShapeParams CloneDamageShapeParams;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Clone")
	TSoftObjectPtr<UAnimMontage> CloneAttackMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Backstep")
	float BackstepDistance;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Backstep")
	float BackstepDuration;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Clone")
	float CloneSpawnOffset;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Clone")
	float CloneRadius;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Clone")
	float CloneArcAngle;

private:
	FTimerHandle BackstepCompleteTimerHandle;

	UFUNCTION()
	void OnBackstepCompleted();
};




