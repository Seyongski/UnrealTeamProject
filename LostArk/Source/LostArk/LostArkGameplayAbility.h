#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "LostArkGameplayAbility.generated.h"

UCLASS()
class LOSTARK_API ULostArkGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	ULostArkGameplayAbility();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability|Damage")
	TSubclassOf<class UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability|Damage")
	float HitRadius;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability|Damage")
	float DamageCoefficient;

	void ApplyDamageInRadius(FVector Origin, float Radius, float DamageAmount, AActor* InstigatorActor);
};
