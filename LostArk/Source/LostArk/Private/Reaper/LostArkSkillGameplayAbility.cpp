#include "LostArkSkillGameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/RootMotionSource.h"

ULostArkSkillGameplayAbility::ULostArkSkillGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	CurrentPlayTask = nullptr;

	bApplyDashForce = false;
	DashDistance = 500.f;
	DashDuration = 0.5f;
	bIgnoreCollisionDuringDash = true;

	// AbilityTags: CancelAbilities() 검색 대상이 되는 태그 (State.Skill은 BlockTags로 각 스킬이 서로를 취소하지 못하게 함)
	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Attacking")));
	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Skill")));

	// ActivationOwnedTags: 어빌리티 실행 중 ASC에 부여되는 태그 (이동 차단용)
	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Attacking")));
	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Skill")));

	// 스킬 사용 중엔 다른 스킬 발동 불가
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Skill")));
}

void ULostArkSkillGameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
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
	}

	if (bApplyDashForce && AvatarPawn)
	{
		// 돌진 중 몬스터 관통을 위해 ECC_Pawn 충돌 무시 설정 (체크박스가 켜져있을 때만)
		if (bIgnoreCollisionDuringDash)
		{
			if (ACharacter* AvatarChar = Cast<ACharacter>(AvatarPawn))
			{
				AvatarChar->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
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
	// 돌진 관통 원복
	if (bApplyDashForce && ActorInfo && ActorInfo->AvatarActor.IsValid())
	{
		if (ACharacter* AvatarChar = Cast<ACharacter>(ActorInfo->AvatarActor.Get()))
		{
			AvatarChar->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		}
	}

	if (CurrentPlayTask)
	{
		CurrentPlayTask->OnCompleted.Clear();
		CurrentPlayTask->OnBlendOut.Clear();
		CurrentPlayTask->OnInterrupted.Clear();
		CurrentPlayTask->OnCancelled.Clear();
		// EndTask()를 제거: 스킬 몽타주가 외부에 의해 끊길 때 다른 어빌리티의 몽타주까지 함께 종료되는 충돌 방지
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
