#include "LostArk/Character/LostArkAttributeSet.h"
#include "GameplayEffectExtension.h"
#include "LostArk/Core/LostArkCombatInterface.h"
#include "Engine/Engine.h"
#include "TimerManager.h"

static const float DefaultHealth = 100.f;
static const float DefaultMaxHealth = 100.f;
static const float DefaultAttackDamage = 10.f;
static const float DefaultAttackRange = 300.f;
static const float DefaultMana = 100.f;
static const float DefaultMaxMana = 100.f;
static const float DefaultMaxIdentityGauge = 100.f;

ULostArkAttributeSet::ULostArkAttributeSet()
{
	InitHealth(DefaultHealth);
	InitMaxHealth(DefaultMaxHealth);
	InitAttackDamage(DefaultAttackDamage);
	InitAttackRange(DefaultAttackRange);
	InitMana(DefaultMana);
	InitMaxMana(DefaultMaxMana);
	InitIdentityGauge(0.f);
	InitMaxIdentityGauge(DefaultMaxIdentityGauge);
}

void ULostArkAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
	else if (Attribute == GetManaAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxMana());
	}
	else if (Attribute == GetIdentityGaugeAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxIdentityGauge());
	}
}

void ULostArkAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);

	if (Attribute == GetIdentityGaugeAttribute())
	{
		UAbilitySystemComponent* TargetASC = GetOwningAbilitySystemComponent();
		if (TargetASC && NewValue >= GetMaxIdentityGauge())
		{
			FGameplayTag BurstTag = FGameplayTag::RequestGameplayTag(FName("State.IdentityBurst"), false);
			if (!TargetASC->HasMatchingGameplayTag(BurstTag))
			{
				TargetASC->AddLooseGameplayTag(BurstTag);
				
				AActor* TargetActor = GetOwningActor();
				if (TargetActor && TargetActor->GetWorld())
				{
					TargetActor->GetWorld()->GetTimerManager().SetTimer(IdentityDecayTimerHandle, this, &ULostArkAttributeSet::OnIdentityDecayTick, 1.0f, true);
				}
			}
		}
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
			if (TargetASC && TargetASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("State.Invincible"), false)))
			{
				return;
			}

			const float NewHealth = FMath::Clamp(GetHealth() - LocalIncomingDamage, 0.f, GetMaxHealth());
			SetHealth(NewHealth);

			AActor* TargetActor = GetOwningActor();
			AActor* SourceActor = Data.EffectSpec.GetContext().GetInstigator();

			if (TargetActor)
			{
				if (ILostArkCombatInterface* CombatInterface = Cast<ILostArkCombatInterface>(TargetActor))
				{
					CombatInterface->ShowDamageText(LocalIncomingDamage);
				}

				FString TargetName = TargetActor->GetName();
				FString SourceName = SourceActor ? SourceActor->GetName() : TEXT("Unknown");
				FString DebugMsg = FString::Printf(TEXT("[%s] Hit by [%s]! Damage: %.1f | Health: %.1f / %.1f"),
					*TargetName, *SourceName, LocalIncomingDamage, NewHealth, GetMaxHealth());

				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, DebugMsg);
				}
				UE_LOG(LogTemp, Warning, TEXT("%s"), *DebugMsg);
				
				// ?곕?吏 ?띿뒪???앹뾽???꾪빐 ?명꽣?섏씠???몄텧
				ILostArkCombatInterface* CombatInterface = Cast<ILostArkCombatInterface>(TargetActor);
				if (CombatInterface)
				{
					CombatInterface->ShowDamageText(LocalIncomingDamage);
				}
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
	else if (Data.EvaluatedData.Attribute == GetManaAttribute())
	{
		SetMana(FMath::Clamp(GetMana(), 0.f, GetMaxMana()));
	}
	else if (Data.EvaluatedData.Attribute == GetIdentityGaugeAttribute())
	{
		SetIdentityGauge(FMath::Clamp(GetIdentityGauge(), 0.f, GetMaxIdentityGauge()));
	}
}

void ULostArkAttributeSet::OnIdentityDecayTick()
{
	float NewIdentity = GetIdentityGauge() - 5.0f;
	if (NewIdentity <= 0.f)
	{
		NewIdentity = 0.f;
		
		AActor* TargetActor = GetOwningActor();
		if (TargetActor && TargetActor->GetWorld())
		{
			TargetActor->GetWorld()->GetTimerManager().ClearTimer(IdentityDecayTimerHandle);
		}

		UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
		if (ASC)
		{
			ASC->RemoveLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("State.IdentityBurst"), false));
		}
	}
	SetIdentityGauge(NewIdentity);
}





