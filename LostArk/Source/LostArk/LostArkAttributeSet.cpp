#include "LostArkAttributeSet.h"
#include "GameplayEffectExtension.h"
#include "LostArkCombatInterface.h"

static const float DefaultHealth = 100.f;
static const float DefaultMaxHealth = 100.f;
static const float DefaultAttackDamage = 10.f;
static const float DefaultAttackRange = 150.f;

ULostArkAttributeSet::ULostArkAttributeSet()
{
	InitHealth(DefaultHealth);
	InitMaxHealth(DefaultMaxHealth);
	InitAttackDamage(DefaultAttackDamage);
	InitAttackRange(DefaultAttackRange);
}

void ULostArkAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
}

void ULostArkAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		const float LocalIncomingDamage = GetIncomingDamage();
		SetIncomingDamage(0.f);

		if (LocalIncomingDamage > 0.f)
		{
			const float NewHealth = FMath::Clamp(GetHealth() - LocalIncomingDamage, 0.f, GetMaxHealth());
			SetHealth(NewHealth);

			if (NewHealth <= 0.f)
			{
				AActor* OwnerActor = GetOwningActor();
				if (OwnerActor)
				{
					ILostArkCombatInterface* CombatInterface = Cast<ILostArkCombatInterface>(OwnerActor);
					if (CombatInterface && !CombatInterface->IsDead())
					{
						CombatInterface->Die();
					}
				}
			}
		}
	}

	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));
	}
}

