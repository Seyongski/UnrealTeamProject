#include "LostArkCharacterComboAttackAbility.h"
#include "LostArkCharacter.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "TimerManager.h"
#include "Engine/World.h"

ULostArkCharacterComboAttackAbility::ULostArkCharacterComboAttackAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	ComboWindowOpenRatio = 0.5f;
	ComboInputTimeout = 0.8f;
	PostComboCooldown = 0.3f;
	CurrentComboIndex = 0;
	bIsComboWindowActive = false;
	bHasPendingComboInput = false;
	CurrentPlayTask = nullptr;

	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Attacking"), false));

	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Attacking"), false));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Attacking"), false));

	CooldownTag = FGameplayTag::RequestGameplayTag(FName("State.Cooldown.Attack"), false);
}

bool ULostArkCharacterComboAttackAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		UE_LOG(LogTemp, Warning, TEXT("[ComboAbility] CanActivate FAILED - Super conditions not met"));
		return false;
	}

	if (ActorInfo->AbilitySystemComponent.IsValid())
	{
		if (ActorInfo->AbilitySystemComponent->HasMatchingGameplayTag(CooldownTag))
		{
			UE_LOG(LogTemp, Warning, TEXT("[ComboAbility] CanActivate FAILED - Cooldown active"));
			return false;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[ComboAbility] CanActivate SUCCESS"));
	return true;
}

void ULostArkCharacterComboAttackAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	UE_LOG(LogTemp, Log, TEXT("[ComboAbility] ActivateAbility called"));

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		UE_LOG(LogTemp, Warning, TEXT("[ComboAbility] CommitAbility FAILED"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (APawn* AvatarPawn = Cast<APawn>(ActorInfo->AvatarActor.Get()))
	{
		if (AController* Controller = AvatarPawn->GetController())
		{
			Controller->StopMovement();
		}
	}

	SetupHitCheckListener();

	CurrentComboIndex = 0;
	bIsComboWindowActive = false;
	bHasPendingComboInput = false;

	PlayComboMontage(0);
}

void ULostArkCharacterComboAttackAbility::PlayComboMontage(int32 Index)
{
	if (!ComboMontages.IsValidIndex(Index) || ComboMontages[Index].IsNull())
	{
		UE_LOG(LogTemp, Error, TEXT("[ComboAbility] PlayComboMontage(%d) FAILED - Null or invalid asset"), Index);
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	UAnimMontage* MontageToPlay = ComboMontages[Index].LoadSynchronous();
	if (!MontageToPlay)
	{
		UE_LOG(LogTemp, Error, TEXT("[ComboAbility] PlayComboMontage(%d) FAILED - LoadSynchronous returned null"), Index);
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	GetWorld()->GetTimerManager().ClearTimer(ComboWindowOpenTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(ComboWindowCloseTimerHandle);

	if (CurrentPlayTask)
	{
		CurrentPlayTask->OnCompleted.Clear();
		CurrentPlayTask->OnInterrupted.Clear();
		CurrentPlayTask->OnCancelled.Clear();
		CurrentPlayTask->EndTask();
		CurrentPlayTask = nullptr;
	}

	UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		TEXT("ComboPlayTask"),
		MontageToPlay,
		1.0f,
		NAME_None,
		false,
		1.0f
	);

	if (!Task)
	{
		UE_LOG(LogTemp, Error, TEXT("[ComboAbility] PlayMontageAndWait Task creation FAILED"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	CurrentPlayTask = Task;
	CurrentPlayTask->OnCompleted.AddDynamic(this, &ULostArkCharacterComboAttackAbility::OnMontageCompleted);
	CurrentPlayTask->OnInterrupted.AddDynamic(this, &ULostArkCharacterComboAttackAbility::OnMontageInterrupted);
	CurrentPlayTask->OnCancelled.AddDynamic(this, &ULostArkCharacterComboAttackAbility::OnMontageInterrupted);
	CurrentPlayTask->ReadyForActivation();

	const float MontageLength = MontageToPlay->GetPlayLength();
	GetWorld()->GetTimerManager().SetTimer(
		ComboWindowOpenTimerHandle,
		this,
		&ULostArkCharacterComboAttackAbility::OpenComboWindow,
		MontageLength * ComboWindowOpenRatio,
		false
	);
}

void ULostArkCharacterComboAttackAbility::OpenComboWindow()
{
	bIsComboWindowActive = true;

	GetWorld()->GetTimerManager().SetTimer(
		ComboWindowCloseTimerHandle,
		this,
		&ULostArkCharacterComboAttackAbility::CloseComboWindow,
		ComboInputTimeout,
		false
	);
}

void ULostArkCharacterComboAttackAbility::CloseComboWindow()
{
	bIsComboWindowActive = false;
	bHasPendingComboInput = false;
}

void ULostArkCharacterComboAttackAbility::RequestComboInput()
{
	if (bIsComboWindowActive)
	{
		bHasPendingComboInput = true;
		GetWorld()->GetTimerManager().ClearTimer(ComboWindowCloseTimerHandle);
	}
}

void ULostArkCharacterComboAttackAbility::OnMontageCompleted()
{
	if (bHasPendingComboInput && ComboMontages.IsValidIndex(CurrentComboIndex + 1))
	{
		CurrentComboIndex++;
		bHasPendingComboInput = false;
		bIsComboWindowActive = false;

		PlayComboMontage(CurrentComboIndex);
	}
	else
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void ULostArkCharacterComboAttackAbility::OnMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void ULostArkCharacterComboAttackAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	GetWorld()->GetTimerManager().ClearTimer(ComboWindowOpenTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(ComboWindowCloseTimerHandle);

	if (CurrentPlayTask)
	{
		CurrentPlayTask->OnCompleted.Clear();
		CurrentPlayTask->OnInterrupted.Clear();
		CurrentPlayTask->OnCancelled.Clear();
		if (!bWasCancelled)
		{
			CurrentPlayTask->EndTask();
		}
		CurrentPlayTask = nullptr;
	}

	if (!bWasCancelled && ActorInfo->AvatarActor.IsValid())
	{
		if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
		{
			ASC->AddLooseGameplayTag(CooldownTag);

			FTimerHandle CooldownTimerHandle;
			FTimerDelegate CooldownDelegate;
			CooldownDelegate.BindWeakLambda(ASC, [ASC, Tag = CooldownTag]()
			{
				if (IsValid(ASC))
				{
					ASC->RemoveLooseGameplayTag(Tag);
				}
			});

			ActorInfo->AvatarActor->GetWorldTimerManager().SetTimer(
				CooldownTimerHandle,
				CooldownDelegate,
				PostComboCooldown,
				false
			);
		}
	}

	CurrentComboIndex = 0;
	bIsComboWindowActive = false;
	bHasPendingComboInput = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}


