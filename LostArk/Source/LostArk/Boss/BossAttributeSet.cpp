// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/BossAttributeSet.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

UBossAttributeSet::UBossAttributeSet()
{
}

void UBossAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
	else if (Attribute == GetStaggerGaugeAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxStaggerGauge());
	}
}

void UBossAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));
	}
	else if (Data.EvaluatedData.Attribute == GetStaggerGaugeAttribute())
	{
		SetStaggerGauge(FMath::Clamp(GetStaggerGauge(), 0.f, GetMaxStaggerGauge()));
	}
}

void UBossAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UBossAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBossAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBossAttributeSet, StaggerGauge, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBossAttributeSet, MaxStaggerGauge, COND_None, REPNOTIFY_Always);
}

void UBossAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBossAttributeSet, Health, OldValue);
}

void UBossAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBossAttributeSet, MaxHealth, OldValue);
}

void UBossAttributeSet::OnRep_StaggerGauge(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBossAttributeSet, StaggerGauge, OldValue);
}

void UBossAttributeSet::OnRep_MaxStaggerGauge(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBossAttributeSet, MaxStaggerGauge, OldValue);
}
