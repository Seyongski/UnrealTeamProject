#pragma once

#include "CoreMinimal.h"
#include "LostArk/Ability/LostArkSkillGameplayAbility.h"
#include "LostArkSkill_Charging.generated.h"

UCLASS()
class LOSTARK_API ULostArkSkill_Charging : public ULostArkSkillGameplayAbility
{
	GENERATED_BODY()

public:
	ULostArkSkill_Charging();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Charging")
	float MaxChargeTime;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Charging")
	float MaxDamageMultiplier;

	UFUNCTION()
	void OnInputRelease(float TimeHeld);

	virtual void OnHitCheckReceived(FGameplayEventData Payload) override;

private:
	float CurrentChargeMultiplier;
	bool bIsCharging;
};




