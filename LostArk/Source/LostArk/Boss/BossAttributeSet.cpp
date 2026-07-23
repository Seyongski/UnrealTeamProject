// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/BossAttributeSet.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"
#include "Core/LostArkCombatInterface.h"
#include "Engine/Engine.h"
#include "Character/LostArkCharacter.h"

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
				FString TargetName = TargetActor->GetName();
				FString SourceName = SourceActor ? SourceActor->GetName() : TEXT("Unknown");
				FString DebugMsg = FString::Printf(TEXT("[%s] Hit by [%s]! Damage: %.1f | Health: %.1f / %.1f"),
					*TargetName, *SourceName, LocalIncomingDamage, NewHealth, GetMaxHealth());

				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, DebugMsg);
				}
				UE_LOG(LogTemp, Warning, TEXT("%s"), *DebugMsg);

				// 데미지 텍스트 위치 계산 (타겟 위치 기준)
				float RandomX = FMath::RandRange(-50.f, 50.f);
				float RandomY = FMath::RandRange(-50.f, 50.f);
				float RandomZ = FMath::RandRange(50.f, 150.f);
				FVector DamageTextSpawnLoc = TargetActor->GetActorLocation() + FVector(RandomX, RandomY, RandomZ);

				// 보스를 타격함 -> 공격한 플레이어의 클라이언트에서 데미지 텍스트 표시
				if (SourceActor)
				{
					if (ALostArkCharacter* SourceLostChar = Cast<ALostArkCharacter>(SourceActor))
					{
						SourceLostChar->Client_ShowDamageText(LocalIncomingDamage, DamageTextSpawnLoc);
					}
				}
			}

			if (NewHealth <= 0.f)
			{
				if (TargetActor)
				{
					if (ILostArkCombatInterface* CombatInterface = Cast<ILostArkCombatInterface>(TargetActor))
					{
						if (!CombatInterface->IsDead())
						{
							CombatInterface->Die();
						}
					}
				}
			}
		}
	}

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
