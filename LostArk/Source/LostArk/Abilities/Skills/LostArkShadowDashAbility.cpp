#include "Abilities/Skills/LostArkShadowDashAbility.h"
#include "Actor/LostArkShadowClone.h"
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
#include "Character/LostArkCharacter.h"

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

	SetupHitCheckListener();

	if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
	{
		FGameplayTagContainer HasTags;
		HasTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Attacking"), false));

		FGameplayTagContainer BlockTags;
		BlockTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Skill"), false));

		ASC->CancelAbilities(&HasTags, &BlockTags, this);
	}

	HandleActivationBasics(ActorInfo);

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
		StartLocation = AvatarPawn->GetActorLocation();

		if (ACharacter* AvatarChar = Cast<ACharacter>(AvatarPawn))
		{
			AvatarChar->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		}

		PlaySkillStepMontage(1);

		if (bApplyDashForce)
		{
			FVector DashDirection = AvatarPawn->GetActorForwardVector();

			// 목적지 안전 보정: 벽/보스 앞에서 멈추고(보스 밀기 방지), 아레나 밖(맵 뚫기)으로 못 나가게 클램프.
			// (ShadowDash 는 대시 중 Pawn 을 Ignore 하지만, 목적지를 보스 앞으로 줄여 겹침->튕김을 막는다)
			const FVector DesiredEnd = StartLocation + DashDirection * DashDistance;
			const FVector SafeEnd = ComputeSafeDashDestination(Cast<ACharacter>(AvatarPawn), StartLocation, DesiredEnd);
			const float SafeDist = FVector::Distance(StartLocation, SafeEnd);
			float CalcDashSpeed = DashDuration > 0.f ? (SafeDist / DashDuration) : 0.f;

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
	if (AvatarPawn && !ShadowCloneClass.IsNull())
	{
		if (ACharacter* AvatarChar = Cast<ACharacter>(AvatarPawn))
		{
			AvatarChar->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		}

		SpawnedShadowCount = 0;

		CurrentActorInfo->AvatarActor->GetWorldTimerManager().SetTimer(
			ShadowSpawnTimerHandle,
			this,
			&ULostArkShadowDashAbility::SpawnNextShadow,
			0.05f,
			true
		);
		
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
	if (!AvatarPawn || ShadowCloneClass.IsNull() || !AvatarPawn->HasAuthority())
	{
		GetWorld()->GetTimerManager().ClearTimer(ShadowSpawnTimerHandle);
		return;
	}

	UClass* LoadedShadowCloneClass = ShadowCloneClass.LoadSynchronous();
	if (!LoadedShadowCloneClass)
	{
		GetWorld()->GetTimerManager().ClearTimer(ShadowSpawnTimerHandle);
		return;
	}

	FVector EndLocation = AvatarPawn->GetActorLocation();
	FVector MiddleLocation = (StartLocation + EndLocation) * 0.5f;
	FRotator BaseRotation = AvatarPawn->GetActorRotation();
	USkeletalMeshComponent* SourceMesh = AvatarPawn->FindComponentByClass<USkeletalMeshComponent>();
	if (SourceMesh)
	{
		MiddleLocation.Z = SourceMesh->GetComponentLocation().Z;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = AvatarPawn;
	SpawnParams.Instigator = AvatarPawn;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	int32 ShadowStepIndex = 2 + SpawnedShadowCount;
	if (SkillSteps.IsValidIndex(ShadowStepIndex) && !SkillSteps[ShadowStepIndex].IsNull())
	{
		float AddYaw = SpawnedShadowCount * (360.f / 8.f);
		FRotator RotatedRotation = FRotator(0.f, BaseRotation.Yaw + AddYaw, 0.f);

		ALostArkShadowClone* Clone = GetWorld()->SpawnActor<ALostArkShadowClone>(
			LoadedShadowCloneClass, 
			MiddleLocation, 
			RotatedRotation, 
			SpawnParams
		);
		if (Clone && SourceMesh)
		{
			ALostArkCharacter* LAChar = Cast<ALostArkCharacter>(AvatarPawn);
			USkeletalMeshComponent* SourceWeapon = LAChar ? LAChar->GetWeaponMesh() : nullptr;
			Clone->InitShadow(SourceMesh, SourceWeapon, SkillSteps[ShadowStepIndex].LoadSynchronous(), 1.f, 0.f, 0.f, GetAbilitySystemComponentFromActorInfo(), DamageEffectClass, CloneDamageShapeParams);
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




