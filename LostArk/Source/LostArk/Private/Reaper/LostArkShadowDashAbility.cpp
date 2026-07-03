#include "LostArkShadowDashAbility.h"
#include "LostArkShadowClone.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/RootMotionSource.h"
#include "TimerManager.h"
#include "Engine/World.h"

ULostArkShadowDashAbility::ULostArkShadowDashAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	bApplyDashForce = true;
	DashDistance = 600.f;
	DashDuration = 0.3f;
	SpawnedShadowCount = 0;
}

void ULostArkShadowDashAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
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

	// 1단계: 준비 애니메이션 재생
	PlaySkillStepMontage(0);
}

void ULostArkShadowDashAbility::PlaySkillStepMontage(int32 Index)
{
	if (CurrentPlayTask)
	{
		CurrentPlayTask->OnCompleted.Clear();
		CurrentPlayTask->OnBlendOut.Clear();
		CurrentPlayTask->OnInterrupted.Clear();
		CurrentPlayTask->OnCancelled.Clear();
		CurrentPlayTask->EndTask();
		CurrentPlayTask = nullptr;
	}

	if (!SkillSteps.IsValidIndex(Index) || SkillSteps[Index].IsNull())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	UAnimMontage* MontageToPlay = SkillSteps[Index].LoadSynchronous();
	if (!MontageToPlay)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		TEXT("ShadowDashPlayTask"),
		MontageToPlay,
		1.0f,
		NAME_None,
		false,
		1.0f
	);

	if (Task)
	{
		CurrentPlayTask = Task;
		
		// 인덱스에 따라 각 단계의 고유 콜백 바인딩
		if (Index == 0)
		{
			CurrentPlayTask->OnCompleted.AddDynamic(this, &ULostArkShadowDashAbility::OnPrepareCompleted);
			CurrentPlayTask->OnBlendOut.AddDynamic(this, &ULostArkShadowDashAbility::OnPrepareCompleted);
		}
		else if (Index == 10)
		{
			CurrentPlayTask->OnCompleted.AddDynamic(this, &ULostArkShadowDashAbility::OnFinishedCompleted);
			CurrentPlayTask->OnBlendOut.AddDynamic(this, &ULostArkShadowDashAbility::OnFinishedCompleted);
		}
		
		CurrentPlayTask->OnInterrupted.AddDynamic(this, &ULostArkShadowDashAbility::OnShadowDashInterrupted);
		CurrentPlayTask->OnCancelled.AddDynamic(this, &ULostArkShadowDashAbility::OnShadowDashInterrupted);
		CurrentPlayTask->ReadyForActivation();
	}
	else
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void ULostArkShadowDashAbility::OnPrepareCompleted()
{
	APawn* AvatarPawn = Cast<APawn>(CurrentActorInfo->AvatarActor.Get());
	if (AvatarPawn)
	{
		// 대시 시작하기 전의 출발 위치 기록
		StartLocation = AvatarPawn->GetActorLocation();

		// 돌진 중 몬스터 관통을 위해 ECC_Pawn 충돌 무시 설정
		if (ACharacter* AvatarChar = Cast<ACharacter>(AvatarPawn))
		{
			AvatarChar->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		}

		// 2단계: 대쉬 애니메이션 재생 (대시는 타이머로 끝나므로 완료 바인딩 없음)
		PlaySkillStepMontage(1);

		// 돌진 물리 적용
		if (bApplyDashForce)
		{
			FVector DashDirection = AvatarPawn->GetActorForwardVector();
			float CalcDashSpeed = DashDuration > 0.f ? (DashDistance / DashDuration) : 0.f;

			UAbilityTask_ApplyRootMotionConstantForce* ForceTask = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
				this,
				TEXT("ShadowDashForceTask"),
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

		// 대쉬 이동 완료 후 그림자 소환 및 마무리 타격 타이머 설정
		CurrentActorInfo->AvatarActor->GetWorldTimerManager().SetTimer(
			DashCompleteTimerHandle,
			this,
			&ULostArkShadowDashAbility::OnDashCompleted,
			DashDuration,
			false
		);
	}
	else
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void ULostArkShadowDashAbility::OnDashCompleted()
{
	APawn* AvatarPawn = Cast<APawn>(CurrentActorInfo->AvatarActor.Get());
	if (AvatarPawn && ShadowCloneClass)
	{
		// 돌진이 끝났으므로 몬스터 충돌 원복
		if (ACharacter* AvatarChar = Cast<ACharacter>(AvatarPawn))
		{
			AvatarChar->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		}

		// 1) 순차 소환 횟수 초기화
		SpawnedShadowCount = 0;

		// 2) 0.05초 간격으로 매우 빠르게 8개의 그림자를 순차 소환
		CurrentActorInfo->AvatarActor->GetWorldTimerManager().SetTimer(
			ShadowSpawnTimerHandle,
			this,
			&ULostArkShadowDashAbility::SpawnNextShadow,
			0.05f,
			true
		);
		
		// 3) 본체는 곧바로 대시 도착지에서 마무리 공격 포즈 재생 (Index 10)
		PlaySkillStepMontage(10);
	}
	else
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void ULostArkShadowDashAbility::SpawnNextShadow()
{
	APawn* AvatarPawn = Cast<APawn>(CurrentActorInfo->AvatarActor.Get());
	if (!AvatarPawn || !ShadowCloneClass)
	{
		GetWorld()->GetTimerManager().ClearTimer(ShadowSpawnTimerHandle);
		return;
	}

	FVector EndLocation = AvatarPawn->GetActorLocation();
	FVector MiddleLocation = (StartLocation + EndLocation) * 0.5f;
	FRotator BaseRotation = AvatarPawn->GetActorRotation();
	USkeletalMeshComponent* SourceMesh = AvatarPawn->FindComponentByClass<USkeletalMeshComponent>();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = AvatarPawn;
	SpawnParams.Instigator = AvatarPawn;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	int32 ShadowStepIndex = 2 + SpawnedShadowCount;
	if (SkillSteps.IsValidIndex(ShadowStepIndex) && !SkillSteps[ShadowStepIndex].IsNull())
	{
		// 360도를 8등분한 각도(45도 배수)로 촥촥촥 교차 회전하여 순차 소환
		float AddYaw = SpawnedShadowCount * (360.f / 8.f);
		FRotator RotatedRotation = FRotator(0.f, BaseRotation.Yaw + AddYaw, 0.f);

		ALostArkShadowClone* Clone = GetWorld()->SpawnActor<ALostArkShadowClone>(
			ShadowCloneClass, 
			MiddleLocation, 
			RotatedRotation, 
			SpawnParams
		);
		if (Clone && SourceMesh)
		{
			Clone->InitShadow(SourceMesh, SkillSteps[ShadowStepIndex].LoadSynchronous());
		}
	}

	SpawnedShadowCount++;
	if (SpawnedShadowCount >= 8)
	{
		GetWorld()->GetTimerManager().ClearTimer(ShadowSpawnTimerHandle);
	}
}

void ULostArkShadowDashAbility::OnFinishedCompleted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void ULostArkShadowDashAbility::OnShadowDashInterrupted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void ULostArkShadowDashAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 캔슬되거나 종료되는 경우 안전장치 충돌 원복
	if (ActorInfo && ActorInfo->AvatarActor.IsValid())
	{
		if (ACharacter* AvatarChar = Cast<ACharacter>(ActorInfo->AvatarActor.Get()))
		{
			AvatarChar->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		}
		ActorInfo->AvatarActor->GetWorldTimerManager().ClearTimer(DashCompleteTimerHandle);
		ActorInfo->AvatarActor->GetWorldTimerManager().ClearTimer(ShadowSpawnTimerHandle);
	}

	if (CurrentPlayTask)
	{
		CurrentPlayTask->OnCompleted.Clear();
		CurrentPlayTask->OnBlendOut.Clear();
		CurrentPlayTask->OnInterrupted.Clear();
		CurrentPlayTask->OnCancelled.Clear();
		if (!bWasCancelled)
		{
			CurrentPlayTask->EndTask();
		}
		CurrentPlayTask = nullptr;
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
