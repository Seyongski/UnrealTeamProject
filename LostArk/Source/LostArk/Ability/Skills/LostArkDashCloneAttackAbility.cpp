#include "LostArk/Ability/Skills/LostArkDashCloneAttackAbility.h"
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

ULostArkDashCloneAttackAbility::ULostArkDashCloneAttackAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	DashDistance = 600.f;
	DashDuration = 0.3f;
	CloneDashDistance = 800.f;
	CloneDashDuration = 0.4f;
	ShadowCloneClass = nullptr;
	CloneAttackMontage = nullptr;
	CurrentPlayTask = nullptr;
}

void ULostArkDashCloneAttackAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
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

	FVector PlayerLoc = AvatarChar->GetActorLocation();
	FVector PlayerForward = AvatarChar->GetActorForwardVector();

	DashEndLocation = PlayerLoc + PlayerForward * DashDistance;

	float CapsuleRadius = AvatarChar->GetCapsuleComponent()->GetScaledCapsuleRadius();
	float CapsuleHalfHeight = AvatarChar->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	FHitResult ObstacleHit;
	FCollisionQueryParams ObstacleParams;
	ObstacleParams.AddIgnoredActor(AvatarChar);

	FCollisionShape CharacterShape = FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight);
	bool bHitObstacle = GetWorld()->SweepSingleByChannel(ObstacleHit, PlayerLoc, DashEndLocation, FQuat::Identity, ECC_WorldStatic, CharacterShape, ObstacleParams);
	if (bHitObstacle)
	{
		DashEndLocation = ObstacleHit.Location + ObstacleHit.ImpactNormal * CapsuleRadius;
	}

	float CalcDashSpeed = DashDuration > 0.f ? (FVector::Distance(PlayerLoc, DashEndLocation) / DashDuration) : 0.f;
	FVector MoveDirection = (DashEndLocation - PlayerLoc).GetSafeNormal2D();

	UAbilityTask_ApplyRootMotionConstantForce* ForceTask = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
		this,
		TEXT("DashForceTask"),
		MoveDirection,
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

	AvatarChar->GetWorldTimerManager().SetTimer(
		PlayerDashTimerHandle,
		this,
		&ULostArkDashCloneAttackAbility::OnDashMovementCompleted,
		DashDuration,
		false
	);

	if (!SkillMontage.IsNull())
	{
		UAnimMontage* LoadedMontage = SkillMontage.LoadSynchronous();
		if (LoadedMontage)
		{
			UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
				this,
				TEXT("PlayerDashPlayTask"),
				LoadedMontage,
				1.0f,
				NAME_None,
				false,
				1.0f
			);

			if (Task)
			{
				CurrentPlayTask = Task;
				CurrentPlayTask->OnCompleted.AddDynamic(this, &ULostArkDashCloneAttackAbility::OnPlayerMontageCompleted);
				CurrentPlayTask->OnBlendOut.AddDynamic(this, &ULostArkDashCloneAttackAbility::OnPlayerMontageCompleted);
				CurrentPlayTask->OnInterrupted.AddDynamic(this, &ULostArkDashCloneAttackAbility::OnPlayerMontageCompleted);
				CurrentPlayTask->OnCancelled.AddDynamic(this, &ULostArkDashCloneAttackAbility::OnPlayerMontageCompleted);
				CurrentPlayTask->ReadyForActivation();
			}
		}
	}
}

void ULostArkDashCloneAttackAbility::OnDashMovementCompleted()
{
	ACharacter* AvatarChar = Cast<ACharacter>(CurrentActorInfo->AvatarActor.Get());
	if (!AvatarChar)
	{
		return;
	}

	AvatarChar->GetWorldTimerManager().ClearTimer(PlayerDashTimerHandle);

	UClass* LoadedShadowCloneClass = ShadowCloneClass.LoadSynchronous();
	UAnimMontage* LoadedCloneMontage = CloneAttackMontage.LoadSynchronous();
	USkeletalMeshComponent* SourceMesh = AvatarChar->FindComponentByClass<USkeletalMeshComponent>();

	if (LoadedShadowCloneClass && LoadedCloneMontage && SourceMesh)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = nullptr;
		SpawnParams.Instigator = AvatarChar;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		FRotator BaseRot = AvatarChar->GetActorRotation();

		for (int32 i = 0; i < 5; ++i)
		{
			float AngleOffset = i * (360.f / 5.f);
			FRotator SpawnRot = FRotator(0.f, BaseRot.Yaw + AngleOffset, 0.f);

			FVector SpawnLoc = DashEndLocation;
			SpawnLoc.Z = SourceMesh->GetComponentLocation().Z;

			ALostArkShadowClone* Clone = GetWorld()->SpawnActor<ALostArkShadowClone>(
				LoadedShadowCloneClass,
				SpawnLoc,
				SpawnRot,
				SpawnParams
			);

			if (Clone)
			{
				ALostArkCharacter* LAChar = Cast<ALostArkCharacter>(AvatarChar);
				USkeletalMeshComponent* SourceWeapon = LAChar ? LAChar->GetWeaponMesh() : nullptr;
				UAbilitySystemComponent* InstigatorASC = GetAbilitySystemComponentFromActorInfo();
				
				float CalcCloneDashSpeed = CloneDashDuration > 0.f ? (CloneDashDistance / CloneDashDuration) : 0.f;
				Clone->InitShadow(SourceMesh, SourceWeapon, LoadedCloneMontage, 1.f, CalcCloneDashSpeed, CloneDashDuration, InstigatorASC, DamageEffectClass, CloneDamageShapeParams);
			}
		}
	}
}

void ULostArkDashCloneAttackAbility::OnPlayerMontageCompleted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void ULostArkDashCloneAttackAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (ActorInfo && ActorInfo->AvatarActor.IsValid())
	{
		ActorInfo->AvatarActor->GetWorldTimerManager().ClearTimer(PlayerDashTimerHandle);
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




