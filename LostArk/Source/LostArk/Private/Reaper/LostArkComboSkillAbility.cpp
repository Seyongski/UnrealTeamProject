#include "LostArkComboSkillAbility.h"
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
	bIgnoreCollisionDuringDash = false; // 3번 연속 스킬은 기본적으로 돌진 중 관통 비활성화
}

void ULostArkComboSkillAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 평타만 정밀 캔슬
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

	// 1타 찌르기(Index 0)일 때만 돌진 물리 및 몬스터 충돌 무시 적용
	if (Index == 0)
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

			if (bApplyDashForce)
			{
				FVector DashDirection = AvatarPawn->GetActorForwardVector();
				float CalcDashSpeed = DashDuration > 0.f ? (DashDistance / DashDuration) : 0.f;

				UAbilityTask_ApplyRootMotionConstantForce* ForceTask = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
					this,
					TEXT("ComboSkillDashForceTask"),
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
		}
	}
	else
	{
		// 찌르기(돌진)가 끝나고 다음 콤보로 넘어갔으므로 몬스터 충돌 원복
		APawn* AvatarPawn = Cast<APawn>(CurrentActorInfo->AvatarActor.Get());
		if (AvatarPawn)
		{
			if (ACharacter* AvatarChar = Cast<ACharacter>(AvatarPawn))
			{
				AvatarChar->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
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

	// 0번(찌르기) 재생 시에는 콤보 입력을 활성화하지 않고, 1번(제자리 베기) 이후부터 콤보 창 오픈
	if (Index > 0 && Index < 4)
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
	// 찌르기(0) 끝난 직후에는 입력 유무 관계없이 즉각 1타 베기(1) 실행
	if (CurrentComboIndex == 0)
	{
		PlayComboSegment(1);
	}
	// 1, 2, 3타 베기 끝났을 때 사용자의 추가 입력이 누적되어 있으면 다음 타수 진행
	else if (bHasPendingComboInput && ComboMontages.IsValidIndex(CurrentComboIndex + 1))
	{
		PlayComboSegment(CurrentComboIndex + 1);
	}
	else
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void ULostArkComboSkillAbility::OnComboMontageInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void ULostArkComboSkillAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 캔슬되거나 종료되는 경우 안전장치 충돌 원복
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
