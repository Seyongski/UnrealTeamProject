#pragma once

#include "CoreMinimal.h"
#include "Abilities/LostArkSkillGameplayAbility.h"
#include "LostArkSkill_Targeting.generated.h"

class UInputAction;

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

	UFUNCTION(Server, Reliable)
	void Server_OnTargetSelected(FVector Location);

	UFUNCTION(Server, Reliable)
	void Server_OnTargetCancelled();

	void ContinueTargetingExecution();

	virtual void OnHitCheckReceived(FGameplayEventData Payload) override;

public:
	UPROPERTY()
	TObjectPtr<const UInputAction> SkillInputAction;

	UFUNCTION(BlueprintImplementableEvent, Category = "Skill|Targeting")
	void K2_OnTargetConfirmed(const FVector& TargetLocation);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_OnTargetConfirmed(FVector TargetLocation);

private:
	FVector CachedTargetLocation;
	bool bIsTargetConfirmed = false;

	UPROPERTY()
	class ALostArkTargetActor_GroundSelect* SpawnedTargetActor;
};




