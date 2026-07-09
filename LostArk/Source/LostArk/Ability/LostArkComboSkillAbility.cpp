#include "LostArk/Ability/LostArkComboSkillAbility.h"
#include "LostArk/Actor/LostArkShadowClone.h"
#include "LostArk/Character/LostArkCharacter.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/RootMotionSource.h"
#include "TimerManager.h"

ULostArkComboSkillAbility::ULostArkComboSkillAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	CurrentComboIndex = 0;
	bIsComboWindowActive = false;
	bHasPendingComboInput = false;

	ComboWindowOpenRatio = 0.5f;
	ComboInputTimeout = 0.8f;
	bIgnoreCollisionDuringDash = false;
	
	CloneSpawnOffsetForward = 0.f;
	CloneSpawnOffsetRight = 150.f;
	FinalDashDistance = 600.f;
	FinalDashDuration = 0.3f;
}

void ULostArkComboSkillAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
	{
		FGameplayTagContainer HasTags;
		HasTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Attacking")));

		FGameplayTagContainer BlockTags;
		BlockTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Skill")));

		ASC->CancelAbilities(&HasTags, &BlockTags, this);
	}

	HandleActivationBasics(ActorInfo);

	SetupHitCheckListener();

	CurrentComboIndex = 0;
	bIsComboWindowActive = false;
	bHasPendingComboInput = false;

	PlayComboSegment(0);
}

void ULostArkComboSkillAbility::PlayComboSegment(int32 Index)
{
	if (!ComboMontages.IsValidIndex(Index) || ComboMontages[Index].IsNull())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	UAnimMontage* MontageToPlay = ComboMontages[Index].LoadSynchronous();
	if (!MontageToPlay)
	{
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

	if (Index == 0 || Index == 2)
	{
		APawn* AvatarPawn = Cast<APawn>(CurrentActorInfo->AvatarActor.Get());
		if (AvatarPawn)
		{
			if (bIgnoreCollisionDuringDash)
			{
				if (ACharacter* AvatarChar = Cast<ACharacter>(AvatarPawn))
				{
					AvatarChar->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
				}
			}

			if (bApplyDashForce)
			{
				FVector DashDirection = AvatarPawn->GetActorForwardVector();
				float TargetDist = (Index == 2) ? FinalDashDistance : DashDistance;
				float TargetDur = (Index == 2) ? FinalDashDuration : DashDuration;
				float CalcDashSpeed = TargetDur > 0.f ? (TargetDist / TargetDur) : 0.f;

				UAbilityTask_ApplyRootMotionConstantForce* ForceTask = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
					this,
					TEXT("ComboSkillDashForceTask"),
					DashDirection,
					CalcDashSpeed,
					TargetDur,
					false,
					nullptr,
					ERootMotionFinishVelocityMode::SetVelocity,
					FVector::ZeroVector,
					0.f,
					false
				);

				if (ForceTask)
				{
					ForceTask->ReadyForActivation();
				}
			}
		}
	}
	else if (Index == 1)
	{
		APawn* AvatarPawn = Cast<APawn>(CurrentActorInfo->AvatarActor.Get());
		if (AvatarPawn)
		{
			if (ACharacter* AvatarChar = Cast<ACharacter>(AvatarPawn))
			{
				AvatarChar->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
			}

			if (!ShadowCloneClass.IsNull())
			{
				UClass* LoadedShadowCloneClass = ShadowCloneClass.LoadSynchronous();
				USkeletalMeshComponent* SourceMesh = AvatarPawn->FindComponentByClass<USkeletalMeshComponent>();
				ALostArkCharacter* LAChar = Cast<ALostArkCharacter>(AvatarPawn);
				USkeletalMeshComponent* SourceWeapon = LAChar ? LAChar->GetWeaponMesh() : nullptr;

				if (LoadedShadowCloneClass && SourceMesh)
				{
					FVector PlayerLoc = AvatarPawn->GetActorLocation();
					FRotator PlayerRot = AvatarPawn->GetActorRotation();
					FVector ForwardVector = FRotationMatrix(PlayerRot).GetScaledAxis(EAxis::X);
					FVector RightVector = FRotationMatrix(PlayerRot).GetScaledAxis(EAxis::Y);
					
					FActorSpawnParameters SpawnParams;
					SpawnParams.Owner = AvatarPawn;
					SpawnParams.Instigator = AvatarPawn;
					SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

					// Left Clone
					if (!LeftCloneMontage.IsNull())
					{
						FVector LeftLoc = PlayerLoc + ForwardVector * CloneSpawnOffsetForward - RightVector * CloneSpawnOffsetRight;
						LeftLoc.Z = SourceMesh->GetComponentLocation().Z;
						ALostArkShadowClone* LeftClone = GetWorld()->SpawnActor<ALostArkShadowClone>(LoadedShadowCloneClass, LeftLoc, PlayerRot, SpawnParams);
						if (LeftClone)
						{
							LeftClone->InitShadow(SourceMesh, SourceWeapon, LeftCloneMontage.LoadSynchronous(), 1.f, 0.f, 0.f, GetAbilitySystemComponentFromActorInfo(), DamageEffectClass, CloneDamageShapeParams);
						}
					}

					// Right Clone
					if (!RightCloneMontage.IsNull())
					{
						FVector RightLoc = PlayerLoc + ForwardVector * CloneSpawnOffsetForward + RightVector * CloneSpawnOffsetRight;
						RightLoc.Z = SourceMesh->GetComponentLocation().Z;
						ALostArkShadowClone* RightClone = GetWorld()->SpawnActor<ALostArkShadowClone>(LoadedShadowCloneClass, RightLoc, PlayerRot, SpawnParams);
						if (RightClone)
						{
							RightClone->InitShadow(SourceMesh, SourceWeapon, RightCloneMontage.LoadSynchronous(), 1.f, 0.f, 0.f, GetAbilitySystemComponentFromActorInfo(), DamageEffectClass, CloneDamageShapeParams);
						}
					}
				}
			}
		}
	}

	UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		TEXT("ComboSkillPlayTask"),
		MontageToPlay,
		1.0f,
		NAME_None,
		false,
		1.0f
	);

	if (!Task)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	CurrentComboIndex = Index;
	bIsComboWindowActive = false;
	bHasPendingComboInput = false;

	CurrentPlayTask = Task;
	CurrentPlayTask->OnCompleted.AddDynamic(this, &ULostArkComboSkillAbility::OnComboMontageCompleted);
	CurrentPlayTask->OnInterrupted.AddDynamic(this, &ULostArkComboSkillAbility::OnComboMontageInterrupted);
	CurrentPlayTask->OnCancelled.AddDynamic(this, &ULostArkComboSkillAbility::OnComboMontageInterrupted);
	CurrentPlayTask->ReadyForActivation();

	if (Index >= 0 && Index < ComboMontages.Num() - 1)
	{
		const float MontageLength = MontageToPlay->GetPlayLength();
		GetWorld()->GetTimerManager().SetTimer(
			ComboWindowOpenTimerHandle,
			this,
			&ULostArkComboSkillAbility::OpenComboWindow,
			MontageLength * ComboWindowOpenRatio,
			false
		);
	}
}

void ULostArkComboSkillAbility::InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	if (bIsComboWindowActive)
	{
		bHasPendingComboInput = true;
		GetWorld()->GetTimerManager().ClearTimer(ComboWindowCloseTimerHandle);
	}
}

void ULostArkComboSkillAbility::OpenComboWindow()
{
	bIsComboWindowActive = true;

	GetWorld()->GetTimerManager().SetTimer(
		ComboWindowCloseTimerHandle,
		this,
		&ULostArkComboSkillAbility::CloseComboWindow,
		ComboInputTimeout,
		false
	);
}

void ULostArkComboSkillAbility::CloseComboWindow()
{
	bIsComboWindowActive = false;
	bHasPendingComboInput = false;
}

void ULostArkComboSkillAbility::OnComboMontageCompleted()
{
	if (bHasPendingComboInput && ComboMontages.IsValidIndex(CurrentComboIndex + 1))
	{
		PlayComboSegment(CurrentComboIndex + 1);
	}
	else
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void ULostArkComboSkillAbility::OnHitCheckReceived(FGameplayEventData Payload)
{
	FDamageShapeParams OldParams = DamageShapeParams;
	if (CurrentComboIndex == 2)
	{
		DamageShapeParams = FinalDashDamageShapeParams;
	}

	Super::OnHitCheckReceived(Payload);

	DamageShapeParams = OldParams;
}

void ULostArkComboSkillAbility::OnComboMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void ULostArkComboSkillAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (ActorInfo && ActorInfo->AvatarActor.IsValid())
	{
		if (ACharacter* AvatarChar = Cast<ACharacter>(ActorInfo->AvatarActor.Get()))
		{
			AvatarChar->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		}
		ActorInfo->AvatarActor->GetWorldTimerManager().ClearTimer(ComboWindowOpenTimerHandle);
		ActorInfo->AvatarActor->GetWorldTimerManager().ClearTimer(ComboWindowCloseTimerHandle);
	}

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

	CurrentComboIndex = 0;
	bIsComboWindowActive = false;
	bHasPendingComboInput = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}



