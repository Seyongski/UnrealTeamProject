#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "LostArkAttributeSet.generated.h"

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS()
class LOSTARK_API ULostArkAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	ULostArkAttributeSet();

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

public:
	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(ULostArkAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(ULostArkAttributeSet, MaxHealth)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData AttackDamage;
	ATTRIBUTE_ACCESSORS(ULostArkAttributeSet, AttackDamage)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData AttackRange;
	ATTRIBUTE_ACCESSORS(ULostArkAttributeSet, AttackRange)
};

