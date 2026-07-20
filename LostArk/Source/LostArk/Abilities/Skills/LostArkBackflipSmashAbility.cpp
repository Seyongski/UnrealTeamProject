#include "Abilities/Skills/LostArkBackflipSmashAbility.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/RootMotionSource.h"
#include "TimerManager.h"

ULostArkBackflipSmashAbility::ULostArkBackflipSmashAbility()
{
	BackflipDistance = 150.f;
	BackflipDuration = 0.2f;
	BackwardJumpDistance = 300.f;
	BackwardJumpDuration = 0.3f;
	SmashDistance = 600.f;
	SmashDuration = 0.3f;
	SmashDelay = 0.1f;
}

void ULostArkBackflipSmashAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
	{
		FGameplayTagContainer HasTags;
		HasTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Attacking"), false));

		FGameplayTagContainer BlockTags;
		BlockTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Skill"), false));

		ASC->CancelAbilities(&HasTags, &BlockTags, this);
	}

	HandleActivationBasics(ActorInfo);

	APawn* AvatarPawn = Cast<APawn>(ActorInfo->AvatarActor.Get());
	if (AvatarPawn)
	{
		FVector BackDirection = -AvatarPawn->GetActorForwardVector();
		float CalcBackflipSpeed = BackflipDuration > 0.f ? (BackflipDistance / BackflipDuration) : 0.f;
		
		UAbilityTask_ApplyRootMotionConstantForce* BackflipTask = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
			this,
			TEXT("Phase1_BackflipTask"),
			BackDirection,
			CalcBackflipSpeed,
			BackflipDuration,
			false,
			nullptr,
			ERootMotionFinishVelocityMode::SetVelocity,
			FVector::ZeroVector,
			0.f,
			false
		);

		if (BackflipTask)
		{
			BackflipTask->ReadyForActivation();
		}

		ActorInfo->AvatarActor->GetWorldTimerManager().SetTimer(
			BackwardJumpTimerHandle,
			this,
			&ULostArkBackflipSmashAbility::OnBackwardJumpDelayFinished,
			BackflipDuration,
			false
		);
	}

	SetupHitCheckListener();

	if (!SkillMontage.IsNull())
	{
		UAnimMontage* LoadedMontage = SkillMontage.LoadSynchronous();
		if (LoadedMontage)
		{
			UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
				this,
				TEXT("SkillPlayTask"),
				LoadedMontage,
				1.0f,
				NAME_None,
				false,
				1.0f
			);

			if (Task)
			{
				CurrentPlayTask = Task;
				CurrentPlayTask->OnCompleted.AddDynamic(this, &ULostArkBackflipSmashAbility::OnMontageCompleted);
				CurrentPlayTask->OnBlendOut.AddDynamic(this, &ULostArkBackflipSmashAbility::OnMontageCompleted);
				CurrentPlayTask->OnInterrupted.AddDynamic(this, &ULostArkBackflipSmashAbility::OnMontageInterrupted);
				CurrentPlayTask->OnCancelled.AddDynamic(this, &ULostArkBackflipSmashAbility::OnMontageInterrupted);
				CurrentPlayTask->ReadyForActivation();
			}
			else
			{
				EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
			}
		}
		else
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		}
	}
	else
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}
}

void ULostArkBackflipSmashAbility::OnBackwardJumpDelayFinished()
{
	APawn* AvatarPawn = Cast<APawn>(CurrentActorInfo->AvatarActor.Get());
	if (AvatarPawn)
	{
		FVector BackDirection = -AvatarPawn->GetActorForwardVector();
		float CalcBackwardJumpSpeed = BackwardJumpDuration > 0.f ? (BackwardJumpDistance / BackwardJumpDuration) : 0.f;

		UAbilityTask_ApplyRootMotionConstantForce* BackwardJumpTask = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
			this,
			TEXT("Phase2_BackwardJumpTask"),
			BackDirection,
			CalcBackwardJumpSpeed,
			BackwardJumpDuration,
			false,
			nullptr,
			ERootMotionFinishVelocityMode::SetVelocity,
			FVector::ZeroVector,
			0.f,
			false
		);

		if (BackwardJumpTask)
		{
			BackwardJumpTask->ReadyForActivation();
		}

		CurrentActorInfo->AvatarActor->GetWorldTimerManager().SetTimer(
			SmashDelayTimerHandle,
			this,
			&ULostArkBackflipSmashAbility::OnSmashDelayFinished,
			BackwardJumpDuration + SmashDelay,
			false
		);
	}
}

void ULostArkBackflipSmashAbility::OnSmashDelayFinished()
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

		FVector ForwardDirection = AvatarPawn->GetActorForwardVector();
		float CalcSmashSpeed = SmashDuration > 0.f ? (SmashDistance / SmashDuration) : 0.f;

		UAbilityTask_ApplyRootMotionConstantForce* SmashTask = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
			this,
			TEXT("Phase3_SmashForceTask"),
			ForwardDirection,
			CalcSmashSpeed,
			SmashDuration,
			false,
			nullptr,
			ERootMotionFinishVelocityMode::SetVelocity,
			FVector::ZeroVector,
			0.f,
			false
		);

		if (SmashTask)
		{
			SmashTask->ReadyForActivation();
		}
	}
}

void ULostArkBackflipSmashAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (ActorInfo && ActorInfo->AvatarActor.IsValid())
	{
		if (ACharacter* AvatarChar = Cast<ACharacter>(ActorInfo->AvatarActor.Get()))
		{
			AvatarChar->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		}
		ActorInfo->AvatarActor->GetWorldTimerManager().ClearTimer(BackwardJumpTimerHandle);
		ActorInfo->AvatarActor->GetWorldTimerManager().ClearTimer(SmashDelayTimerHandle);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}




