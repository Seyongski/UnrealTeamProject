#include "Abilities/Skills/LostArkThrustComboAbility.h"
#include "Actor/LostArkShadowClone.h"
#include "Character/LostArkCharacter.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"

ULostArkThrustComboAbility::ULostArkThrustComboAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	CurrentComboState = EThrustComboState::Idle;
	ThrustMontage = nullptr;
	ThrustComboFinishMontage = nullptr;
	ShadowCloneClass = nullptr;
	CloneSpinMontage = nullptr;
	ComboWindowStartTime = 0.2f;
	ComboWindowEndTime = 0.8f;
	CloneSpawnOffset = 250.f;
	CloneRadius = 120.f;
	CurrentPlayTask = nullptr;
}

void ULostArkThrustComboAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	HandleActivationBasics(ActorInfo);
	SetupHitCheckListener();

	ACharacter* AvatarChar = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
	if (!AvatarChar)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAnimMontage* LoadedThrust = ThrustMontage.LoadSynchronous();
	if (LoadedThrust)
	{
		CurrentComboState = EThrustComboState::Thrusting;

		UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this,
			TEXT("ThrustPlayTask"),
			LoadedThrust,
			1.0f,
			NAME_None,
			false,
			1.0f
		);

		if (Task)
		{
			CurrentPlayTask = Task;
			CurrentPlayTask->OnCompleted.AddDynamic(this, &ULostArkThrustComboAbility::OnThrustAnimCompleted);
			CurrentPlayTask->OnBlendOut.AddDynamic(this, &ULostArkThrustComboAbility::OnThrustAnimCompleted);
			CurrentPlayTask->OnInterrupted.AddDynamic(this, &ULostArkThrustComboAbility::OnThrustAnimCompleted);
			CurrentPlayTask->OnCancelled.AddDynamic(this, &ULostArkThrustComboAbility::OnThrustAnimCompleted);
			CurrentPlayTask->ReadyForActivation();
		}
	}
	else
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AvatarChar->GetWorldTimerManager().SetTimer(
		ComboWindowStartTimerHandle,
		this,
		&ULostArkThrustComboAbility::OnComboWindowOpened,
		ComboWindowStartTime,
		false
	);

	AvatarChar->GetWorldTimerManager().SetTimer(
		ComboWindowEndTimerHandle,
		this,
		&ULostArkThrustComboAbility::OnComboWindowClosed,
		ComboWindowEndTime,
		false
	);
}

void ULostArkThrustComboAbility::InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	if (CurrentComboState == EThrustComboState::ComboWindowOpen)
	{
		PlayThrustComboFinish();
	}
}

void ULostArkThrustComboAbility::OnComboWindowOpened()
{
	if (CurrentComboState == EThrustComboState::Thrusting)
	{
		CurrentComboState = EThrustComboState::ComboWindowOpen;
	}
}

void ULostArkThrustComboAbility::OnComboWindowClosed()
{
	if (CurrentComboState == EThrustComboState::ComboWindowOpen)
	{
		CurrentComboState = EThrustComboState::Finished;
	}
}

void ULostArkThrustComboAbility::OnThrustAnimCompleted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void ULostArkThrustComboAbility::PlayThrustComboFinish()
{
	ACharacter* AvatarChar = Cast<ACharacter>(CurrentActorInfo->AvatarActor.Get());
	if (!AvatarChar)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	AvatarChar->GetWorldTimerManager().ClearTimer(ComboWindowStartTimerHandle);
	AvatarChar->GetWorldTimerManager().ClearTimer(ComboWindowEndTimerHandle);

	CurrentComboState = EThrustComboState::SpinActive;

	if (CurrentPlayTask)
	{
		CurrentPlayTask->EndTask();
		CurrentPlayTask = nullptr;
	}

	UAnimMontage* LoadedFinish = ThrustComboFinishMontage.LoadSynchronous();
	float FinishLength = 1.0f;

	if (LoadedFinish)
	{
		UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this,
			TEXT("ComboFinishPlayTask"),
			LoadedFinish,
			1.0f,
			NAME_None,
			false,
			1.0f
		);

		if (Task)
		{
			CurrentPlayTask = Task;
			CurrentPlayTask->OnCompleted.AddDynamic(this, &ULostArkThrustComboAbility::OnThrustAnimCompleted);
			CurrentPlayTask->OnBlendOut.AddDynamic(this, &ULostArkThrustComboAbility::OnThrustAnimCompleted);
			CurrentPlayTask->OnInterrupted.AddDynamic(this, &ULostArkThrustComboAbility::OnThrustAnimCompleted);
			CurrentPlayTask->OnCancelled.AddDynamic(this, &ULostArkThrustComboAbility::OnThrustAnimCompleted);
			CurrentPlayTask->ReadyForActivation();
		}
		FinishLength = LoadedFinish->GetPlayLength();
	}

	UClass* LoadedShadowCloneClass = ShadowCloneClass.LoadSynchronous();
	UAnimMontage* LoadedCloneMontage = CloneSpinMontage.LoadSynchronous();
	USkeletalMeshComponent* SourceMesh = AvatarChar->FindComponentByClass<USkeletalMeshComponent>();
	ALostArkCharacter* LAChar = Cast<ALostArkCharacter>(AvatarChar);
	USkeletalMeshComponent* SourceWeapon = LAChar ? LAChar->GetWeaponMesh() : nullptr;

	if (LoadedShadowCloneClass && LoadedCloneMontage && SourceMesh && AvatarChar->HasAuthority())
	{
		FVector PlayerLoc = AvatarChar->GetActorLocation();
		FVector PlayerForward = AvatarChar->GetActorForwardVector();
		FRotator PlayerRot = AvatarChar->GetActorRotation();
		FVector SpawnCenter = PlayerLoc + PlayerForward * CloneSpawnOffset;

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = nullptr;
		SpawnParams.Instigator = AvatarChar;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		for (int32 i = 0; i < 5; ++i)
		{
			float AngleOffset = i * (360.f / 5.f);
			FRotator SpawnRot = FRotator(0.f, PlayerRot.Yaw + AngleOffset, 0.f);
			FVector Direction = SpawnRot.Vector();
			FVector SpawnLoc = SpawnCenter + Direction * CloneRadius;
			SpawnLoc.Z = SourceMesh->GetComponentLocation().Z;

			ALostArkShadowClone* Clone = GetWorld()->SpawnActor<ALostArkShadowClone>(
				LoadedShadowCloneClass,
				SpawnLoc,
				SpawnRot,
				SpawnParams
			);

			if (Clone)
			{
				Clone->InitShadow(SourceMesh, SourceWeapon, LoadedCloneMontage, 1.f, 0.f, 0.f, GetAbilitySystemComponentFromActorInfo(), DamageEffectClass, CloneDamageShapeParams);
			}
		}
	}

	AvatarChar->GetWorldTimerManager().SetTimer(
		SkillTimeoutTimerHandle,
		this,
		&ULostArkThrustComboAbility::OnThrustAnimCompleted,
		FinishLength + 0.1f,
		false
	);
}

void ULostArkThrustComboAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (ActorInfo && ActorInfo->AvatarActor.IsValid())
	{
		ActorInfo->AvatarActor->GetWorldTimerManager().ClearTimer(ComboWindowStartTimerHandle);
		ActorInfo->AvatarActor->GetWorldTimerManager().ClearTimer(ComboWindowEndTimerHandle);
		ActorInfo->AvatarActor->GetWorldTimerManager().ClearTimer(SkillTimeoutTimerHandle);
	}

	if (CurrentPlayTask)
	{
		CurrentPlayTask->OnCompleted.Clear();
		CurrentPlayTask->OnBlendOut.Clear();
		CurrentPlayTask->OnInterrupted.Clear();
		CurrentPlayTask->OnCancelled.Clear();
		CurrentPlayTask = nullptr;
	}

	CurrentComboState = EThrustComboState::Idle;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}




