#include "Abilities/LostArkSkillGameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/RootMotionSource.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameFramework/PlayerController.h"

ULostArkSkillGameplayAbility::ULostArkSkillGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	CurrentPlayTask = nullptr;

	bApplyDashForce = false;
	DashDistance = 500.f;
	DashDuration = 0.5f;
	bIgnoreCollisionDuringDash = true;
	bInvincibleDuringDash = false;

	bRotateToMouseOnActivate = true;
	bAbortNavigationMove = true;

	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Attacking"), false));
	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Skill"), false));

	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Attacking"), false));
	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Skill"), false));

	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Skill"), false));
}

void ULostArkSkillGameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
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

	K2_ActivateAbility();

	if (bApplyDashForce && ActorInfo->AvatarActor.IsValid())
	{
		APawn* AvatarPawn = Cast<APawn>(ActorInfo->AvatarActor.Get());
		if (bIgnoreCollisionDuringDash && AvatarPawn)
		{
			if (ACharacter* AvatarChar = Cast<ACharacter>(AvatarPawn))
			{
				AvatarChar->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
			}
		}

		if (bInvincibleDuringDash)
		{
			if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
			{
				ASC->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("State.Invincible"), false));
			}
		}

		FVector DashDirection = AvatarPawn->GetActorForwardVector();
		float CalcDashSpeed = DashDuration > 0.f ? (DashDistance / DashDuration) : 0.f;

		UAbilityTask_ApplyRootMotionConstantForce* ForceTask = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
			this,
			TEXT("SkillDashForceTask"),
			DashDirection,
			CalcDashSpeed,
			DashDuration,
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
				CurrentPlayTask->OnCompleted.AddDynamic(this, &ULostArkSkillGameplayAbility::OnMontageCompleted);
				CurrentPlayTask->OnBlendOut.AddDynamic(this, &ULostArkSkillGameplayAbility::OnMontageCompleted);
				CurrentPlayTask->OnInterrupted.AddDynamic(this, &ULostArkSkillGameplayAbility::OnMontageInterrupted);
				CurrentPlayTask->OnCancelled.AddDynamic(this, &ULostArkSkillGameplayAbility::OnMontageInterrupted);
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

void ULostArkSkillGameplayAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bApplyDashForce && ActorInfo && ActorInfo->AvatarActor.IsValid())
	{
		if (ACharacter* AvatarChar = Cast<ACharacter>(ActorInfo->AvatarActor.Get()))
		{
			AvatarChar->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		}

		if (bInvincibleDuringDash)
		{
			if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
			{
				ASC->RemoveLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("State.Invincible"), false));
			}
		}
	}

	if (CurrentPlayTask)
	{
		CurrentPlayTask->OnCompleted.Clear();
		CurrentPlayTask->OnBlendOut.Clear();
		CurrentPlayTask->OnInterrupted.Clear();
		CurrentPlayTask->OnCancelled.Clear();
		CurrentPlayTask = nullptr;
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void ULostArkSkillGameplayAbility::OnMontageCompleted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void ULostArkSkillGameplayAbility::OnMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void ULostArkSkillGameplayAbility::HandleActivationBasics(const FGameplayAbilityActorInfo* ActorInfo)
{
	if (!ActorInfo) return;

	APawn* AvatarPawn = Cast<APawn>(ActorInfo->AvatarActor.Get());
	if (AvatarPawn)
	{
		AController* Controller = AvatarPawn->GetController();
		if (Controller)
		{
			Controller->StopMovement();
			
			if (bAbortNavigationMove)
			{
				if (UPathFollowingComponent* PathFollowingComp = Controller->FindComponentByClass<UPathFollowingComponent>())
				{
					PathFollowingComp->AbortMove(*Controller, FPathFollowingResultFlags::MovementStop);
				}
			}
		}

		if (bRotateToMouseOnActivate)
		{
			if (APlayerController* PC = Cast<APlayerController>(Controller))
			{
				FHitResult HitResult;
				if (PC->GetHitResultUnderCursor(ECC_Visibility, false, HitResult))
				{
					FVector TargetLoc = HitResult.ImpactPoint;
					FVector Dir = TargetLoc - AvatarPawn->GetActorLocation();
					Dir.Z = 0.f;
					if (!Dir.IsNearlyZero())
					{
						AvatarPawn->SetActorRotation(Dir.Rotation());
					}
				}
			}
		}
	}
}

UGameplayEffect* ULostArkSkillGameplayAbility::GetCooldownGameplayEffect() const
{
	if (CooldownEffectClass)
	{
		return CooldownEffectClass->GetDefaultObject<UGameplayEffect>();
	}
	return Super::GetCooldownGameplayEffect();
}






