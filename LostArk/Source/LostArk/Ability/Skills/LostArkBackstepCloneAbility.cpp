#include "LostArk/Ability/Skills/LostArkBackstepCloneAbility.h"
#include "LostArk/Actor/LostArkShadowClone.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "LostArk/Character/LostArkCharacter.h"

ULostArkBackstepCloneAbility::ULostArkBackstepCloneAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	BackstepDistance = 500.f;
	BackstepDuration = 0.3f;
	CloneSpawnOffset = 150.f;
	CloneRadius = 250.f;
	CloneArcAngle = 90.f;
	ShadowCloneClass = nullptr;
	CloneAttackMontage = nullptr;
	CurrentPlayTask = nullptr;
}

void ULostArkBackstepCloneAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	HandleActivationBasics(ActorInfo);

	ACharacter* AvatarChar = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
	if (!AvatarChar)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	FVector PlayerLoc = AvatarChar->GetActorLocation();
	FRotator PlayerRot = AvatarChar->GetActorRotation();
	FVector PlayerForward = AvatarChar->GetActorForwardVector();
	USkeletalMeshComponent* SourceMesh = AvatarChar->FindComponentByClass<USkeletalMeshComponent>();

	FVector DestLocation = PlayerLoc - PlayerForward * BackstepDistance;

	float CapsuleRadius = AvatarChar->GetCapsuleComponent()->GetScaledCapsuleRadius();
	float CapsuleHalfHeight = AvatarChar->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	FHitResult ObstacleHit;
	FCollisionQueryParams ObstacleParams;
	ObstacleParams.AddIgnoredActor(AvatarChar);

	FCollisionShape CharacterShape = FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight);
	bool bHitObstacle = GetWorld()->SweepSingleByChannel(ObstacleHit, PlayerLoc, DestLocation, FQuat::Identity, ECC_WorldStatic, CharacterShape, ObstacleParams);
	if (bHitObstacle)
	{
		DestLocation = ObstacleHit.Location + ObstacleHit.ImpactNormal * CapsuleRadius;
	}

	float CalcBackstepSpeed = BackstepDuration > 0.f ? (FVector::Distance(PlayerLoc, DestLocation) / BackstepDuration) : 0.f;
	FVector MoveDirection = (DestLocation - PlayerLoc).GetSafeNormal2D();

	UAbilityTask_ApplyRootMotionConstantForce* ForceTask = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
		this,
		TEXT("BackstepForceTask"),
		MoveDirection,
		CalcBackstepSpeed,
		BackstepDuration,
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

	AvatarChar->GetWorldTimerManager().SetTimer(
		BackstepCompleteTimerHandle,
		this,
		&ULostArkBackstepCloneAbility::OnBackstepCompleted,
		BackstepDuration,
		false
	);

	UClass* LoadedShadowCloneClass = ShadowCloneClass.LoadSynchronous();
	UAnimMontage* LoadedCloneMontage = CloneAttackMontage.LoadSynchronous();

	if (LoadedShadowCloneClass && LoadedCloneMontage && SourceMesh)
	{
		FVector SpawnCenter = PlayerLoc + PlayerForward * CloneSpawnOffset;
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = AvatarChar;
		SpawnParams.Instigator = AvatarChar;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		for (int32 i = 0; i < 10; ++i)
		{
			float AngleOffset = i * (360.f / 10.f);
			FRotator RotatedRot = FRotator(0.f, PlayerRot.Yaw + AngleOffset, 0.f);
			FVector Direction = RotatedRot.Vector();
			FVector SpawnLoc = SpawnCenter + Direction * CloneRadius;
			SpawnLoc.Z = SourceMesh->GetComponentLocation().Z;

			FVector TargetDirection = (SpawnCenter - SpawnLoc).GetSafeNormal2D();
			FRotator TargetRotation = TargetDirection.Rotation();

			ALostArkShadowClone* Clone = GetWorld()->SpawnActor<ALostArkShadowClone>(
				LoadedShadowCloneClass,
				SpawnLoc,
				TargetRotation,
				SpawnParams
			);

			if (Clone)
			{
				ALostArkCharacter* LAChar = Cast<ALostArkCharacter>(AvatarChar);
				USkeletalMeshComponent* SourceWeapon = LAChar ? LAChar->GetWeaponMesh() : nullptr;
				Clone->InitShadow(SourceMesh, SourceWeapon, LoadedCloneMontage, 1.f, 0.f, 0.f, GetAbilitySystemComponentFromActorInfo(), DamageEffectClass, CloneDamageShapeParams);
			}
		}
	}

	if (!SkillMontage.IsNull())
	{
		UAnimMontage* LoadedMontage = SkillMontage.LoadSynchronous();
		if (LoadedMontage)
		{
			UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
				this,
				TEXT("BackstepPlayTask"),
				LoadedMontage,
				1.0f,
				NAME_None,
				false,
				1.0f
			);

			if (Task)
			{
				CurrentPlayTask = Task;
				CurrentPlayTask->OnCompleted.AddDynamic(this, &ULostArkBackstepCloneAbility::OnBackstepCompleted);
				CurrentPlayTask->OnBlendOut.AddDynamic(this, &ULostArkBackstepCloneAbility::OnBackstepCompleted);
				CurrentPlayTask->OnInterrupted.AddDynamic(this, &ULostArkBackstepCloneAbility::OnBackstepCompleted);
				CurrentPlayTask->OnCancelled.AddDynamic(this, &ULostArkBackstepCloneAbility::OnBackstepCompleted);
				CurrentPlayTask->ReadyForActivation();
			}
		}
	}
}

void ULostArkBackstepCloneAbility::OnBackstepCompleted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void ULostArkBackstepCloneAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (ActorInfo && ActorInfo->AvatarActor.IsValid())
	{
		ActorInfo->AvatarActor->GetWorldTimerManager().ClearTimer(BackstepCompleteTimerHandle);
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



