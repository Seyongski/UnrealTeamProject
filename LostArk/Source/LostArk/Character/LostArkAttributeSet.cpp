#include "Character/LostArkAttributeSet.h"
#include "GameplayEffectExtension.h"
#include "Core/LostArkCombatInterface.h"
#include "Engine/Engine.h"
#include "TimerManager.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Character/LostArkCharacter.h"
#include "Net/UnrealNetwork.h"

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

void ULostArkAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(ULostArkAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(ULostArkAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(ULostArkAttributeSet, Mana, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(ULostArkAttributeSet, MaxMana, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(ULostArkAttributeSet, IdentityGauge, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(ULostArkAttributeSet, MaxIdentityGauge, COND_None, REPNOTIFY_Always);
}

void ULostArkAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ULostArkAttributeSet, Health, OldHealth);
}

void ULostArkAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ULostArkAttributeSet, MaxHealth, OldMaxHealth);
}

void ULostArkAttributeSet::OnRep_Mana(const FGameplayAttributeData& OldMana)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ULostArkAttributeSet, Mana, OldMana);
}

void ULostArkAttributeSet::OnRep_MaxMana(const FGameplayAttributeData& OldMaxMana)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ULostArkAttributeSet, MaxMana, OldMaxMana);
}

void ULostArkAttributeSet::OnRep_IdentityGauge(const FGameplayAttributeData& OldIdentityGauge)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ULostArkAttributeSet, IdentityGauge, OldIdentityGauge);
}

void ULostArkAttributeSet::OnRep_MaxIdentityGauge(const FGameplayAttributeData& OldMaxIdentityGauge)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ULostArkAttributeSet, MaxIdentityGauge, OldMaxIdentityGauge);
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

				// 대상이 플레이어 캐릭터인지 확인 (PlayerController 보유 여부)
				bool bTargetIsPlayer = false;
				if (ACharacter* TargetChar = Cast<ACharacter>(TargetActor))
				{
					if (Cast<APlayerController>(TargetChar->GetController()))
					{
						bTargetIsPlayer = true;
					}
				}

				if (bTargetIsPlayer)
				{
					// 플레이어가 피격당함 → 피격당한 본인의 클라이언트에서 표시
					if (ALostArkCharacter* TargetLostChar = Cast<ALostArkCharacter>(TargetActor))
					{
						TargetLostChar->Client_ShowDamageText(LocalIncomingDamage, DamageTextSpawnLoc);
					}
				}
				else if (SourceActor)
				{
					// 몬스터/보스를 타격함 → 공격한 플레이어의 클라이언트에서 표시 (타겟 위치에)
					if (ALostArkCharacter* SourceLostChar = Cast<ALostArkCharacter>(SourceActor))
					{
						SourceLostChar->Client_ShowDamageText(LocalIncomingDamage, DamageTextSpawnLoc);
					}
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





