#include "LostArkBackflipSmashAbility.h"
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

	// State.Attacking 보유 & State.Skill 미보유인 어빌리티만 정밀 캔슬 (콤보 평타만 대상)
	if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
	{
		FGameplayTagContainer HasTags;
		HasTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Attacking")));

		FGameplayTagContainer BlockTags;
		BlockTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Skill")));

		ASC->CancelAbilities(&HasTags, &BlockTags, this);
	}

	APawn* AvatarPawn = Cast<APawn>(ActorInfo->AvatarActor.Get());
	if (AvatarPawn)
	{
		AController* Controller = AvatarPawn->GetController();
		if (Controller)
		{
			Controller->StopMovement();
		}

		// 1. Phase 1: 뒤로 물러나는 백덤블링 힘 가하기 (후방)
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

		// Phase 1 이후 Phase 2(Backward Jump) 타이머 가동
		ActorInfo->AvatarActor->GetWorldTimerManager().SetTimer(
			BackwardJumpTimerHandle,
			this,
			&ULostArkBackflipSmashAbility::OnBackwardJumpDelayFinished,
			BackflipDuration,
			false
		);
	}

	// 부모와 동일한 몽타주 재생 로직
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
		// 2. Phase 2: 추가로 뒤로 물러나는 점프 힘 가하기 (후방)
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

		// Phase 2 이후 Phase 3(Smash) 타이머 가동
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
		// 돌진 중 몬스터 관통을 위해 ECC_Pawn 충돌 무시 설정 (체크박스가 켜져있을 때만)
		if (bIgnoreCollisionDuringDash)
		{
			if (ACharacter* AvatarChar = Cast<ACharacter>(AvatarPawn))
			{
				AvatarChar->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
			}
		}

		// 3. Phase 3: 앞으로 도약하며 강타하는 힘 가하기 (전방)
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
	// 캔슬되거나 종료되는 경우 안전장치 충돌 원복
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
