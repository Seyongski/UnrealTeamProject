#pragma once

#include "CoreMinimal.h"
#include "Abilities/LostArkSkillGameplayAbility.h"
#include "LostArkDashCloneAttackAbility.generated.h"

class ALostArkShadowClone;

UCLASS()
class LOSTARK_API ULostArkDashCloneAttackAbility : public ULostArkSkillGameplayAbility
{
	GENERATED_BODY()

public:
	ULostArkDashCloneAttackAbility();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Clone")
	TSoftClassPtr<ALostArkShadowClone> ShadowCloneClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Clone")
	TSoftObjectPtr<UAnimMontage> CloneAttackMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Clone")
	float CloneDashDistance;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Clone")
	float CloneDashDuration;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Clone")
	FDamageShapeParams CloneDamageShapeParams;

private:
	FTimerHandle PlayerDashTimerHandle;
	FVector DashEndLocation;

	UFUNCTION()
	void OnDashMovementCompleted();

	UFUNCTION()
	void OnPlayerMontageCompleted();
};




