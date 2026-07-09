#pragma once

#include "CoreMinimal.h"
#include "LostArkSkillGameplayAbility.h"
#include "LostArkSkill_Targeting.generated.h"

UCLASS()
class LOSTARK_API ULostArkSkill_Targeting : public ULostArkSkillGameplayAbility
{
	GENERATED_BODY()

public:
	ULostArkSkill_Targeting();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Targeting")
	TSubclassOf<class AGameplayAbilityTargetActor> TargetActorClass;

	UFUNCTION()
	void OnTargetSelectedDirect(const FVector& Location);

	UFUNCTION()
	void OnTargetCancelledDirect();

	virtual void OnHitCheckReceived(FGameplayEventData Payload) override;

private:
	FVector CachedTargetLocation;

	UPROPERTY()
	class ALostArkTargetActor_GroundSelect* SpawnedTargetActor;
};
