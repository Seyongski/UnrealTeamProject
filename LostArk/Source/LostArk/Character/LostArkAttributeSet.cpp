#include "LostArk/Character/LostArkAttributeSet.h"
#include "GameplayEffectExtension.h"
#include "LostArk/Core/LostArkCombatInterface.h"
#include "Engine/Engine.h"

static const float DefaultHealth = 100.f;
static const float DefaultMaxHealth = 100.f;
static const float DefaultAttackDamage = 10.f;
static const float DefaultAttackRange = 300.f;

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
			UAbilitySystemComponent* TargetASC = &Data.Target;
			if (TargetASC && TargetASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("State.Invincible"))))
			{
				return;
			}

			const float NewHealth = FMath::Clamp(GetHealth() - LocalIncomingDamage, 0.f, GetMaxHealth());
			SetHealth(NewHealth);

			AActor* TargetActor = GetOwningActor();
			AActor* SourceActor = Data.EffectSpec.GetContext().GetInstigator();

			if (TargetActor)
			{
				FString TargetName = TargetActor->GetName();
				FString SourceName = SourceActor ? SourceActor->GetName() : TEXT("Unknown");
				FString DebugMsg = FString::Printf(TEXT("[%s] Hit by [%s]! Damage: %.1f | Health: %.1f / %.1f"),
					*TargetName, *SourceName, LocalIncomingDamage, NewHealth, GetMaxHealth());

				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, DebugMsg);
				}
				UE_LOG(LogTemp, Warning, TEXT("%s"), *DebugMsg);
			}

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




