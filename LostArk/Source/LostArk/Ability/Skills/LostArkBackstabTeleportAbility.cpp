#include "LostArk/Ability/Skills/LostArkBackstabTeleportAbility.h"
#include "LostArk/Monster/LostArkMonster.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Controller.h"
#include "Engine/World.h"

ULostArkBackstabTeleportAbility::ULostArkBackstabTeleportAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	TeleportOffset = 150.f;
	MaxSearchDistance = 1000.f;
	TargetSearchRadius = 150.f;
	CachedTargetMonster = nullptr;
}

bool ULostArkBackstabTeleportAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	UE_LOG(LogTemp, Warning, TEXT("[Backstab] CanActivateAbility checking started."));

	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Backstab] CanActivate FAILED at Super::CanActivateAbility check."));
		return false;
	}

	APawn* AvatarPawn = Cast<APawn>(ActorInfo->AvatarActor.Get());
	if (!AvatarPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Backstab] CanActivate FAILED - AvatarPawn is null."));
		return false;
	}

	APlayerController* PC = Cast<APlayerController>(AvatarPawn->GetController());
	if (!PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Backstab] CanActivate FAILED - PlayerController is null."));
		return false;
	}

	FHitResult CursorHit;
	bool bHitCursor = PC->GetHitResultUnderCursor(ECC_Visibility, false, CursorHit);
	UE_LOG(LogTemp, Warning, TEXT("[Backstab] GetHitResultUnderCursor returned %s. ImpactPoint: %s"), bHitCursor ? TEXT("True") : TEXT("False"), *CursorHit.ImpactPoint.ToString());

	AActor* CursorActor = CursorHit.GetActor();
	if (IsValid(CursorActor))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Backstab] Direct Cursor Hit Actor: %s (Class: %s)"), *CursorActor->GetName(), *CursorActor->GetClass()->GetName());
		ALostArkMonster* Monster = Cast<ALostArkMonster>(CursorActor);
		if (Monster && !Monster->IsDead())
		{
			CachedTargetMonster = Monster;
			UE_LOG(LogTemp, Warning, TEXT("[Backstab] Target accepted via Direct Cursor Hit: %s"), *Monster->GetName());
			return true;
		}
	}

	FVector MouseLocation = CursorHit.ImpactPoint;
	FVector CharacterLocation = AvatarPawn->GetActorLocation();
	FVector Direction = (MouseLocation - CharacterLocation).GetSafeNormal2D();

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(AvatarPawn);

	TArray<FHitResult> SweepHits;
	FVector Start = CharacterLocation;
	FVector End = Start + Direction * MaxSearchDistance;
	FCollisionShape Shape = FCollisionShape::MakeSphere(TargetSearchRadius);

	bool bHit = GetWorld()->SweepMultiByChannel(SweepHits, Start, End, FQuat::Identity, ECC_Pawn, Shape, Params);
	if (bHit)
	{
		for (const FHitResult& Hit : SweepHits)
		{
			AActor* HitActor = Hit.GetActor();
			if (IsValid(HitActor))
			{
				UE_LOG(LogTemp, Warning, TEXT("[Backstab] Sweep Hit Actor Candidate: %s (Class: %s)"), *HitActor->GetName(), *HitActor->GetClass()->GetName());
				ALostArkMonster* Monster = Cast<ALostArkMonster>(HitActor);
				if (Monster && !Monster->IsDead())
				{
					CachedTargetMonster = Monster;
					UE_LOG(LogTemp, Warning, TEXT("[Backstab] Target accepted via Sweep: %s"), *Monster->GetName());
					return true;
				}
			}
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[Backstab] CanActivate FAILED - No valid ALostArkMonster target detected."));
	return false;
}

void ULostArkBackstabTeleportAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	HandleActivationBasics(ActorInfo);
	SetupHitCheckListener();

	ALostArkMonster* Target = CachedTargetMonster.Get();
	if (!IsValid(Target))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ACharacter* AvatarChar = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
	if (!AvatarChar)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	FVector TargetLoc = Target->GetActorLocation();
	FVector TargetForward = Target->GetActorForwardVector();
	FVector DestLocation = TargetLoc - TargetForward * TeleportOffset;
	FRotator DestRotation = TargetForward.Rotation();

	float CapsuleRadius = AvatarChar->GetCapsuleComponent()->GetScaledCapsuleRadius();
	float CapsuleHalfHeight = AvatarChar->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	FHitResult ObstacleHit;
	FCollisionQueryParams ObstacleParams;
	ObstacleParams.AddIgnoredActor(AvatarChar);
	ObstacleParams.AddIgnoredActor(Target);

	FCollisionShape CharacterShape = FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight);
	FVector Start = TargetLoc;
	FVector End = DestLocation;

	bool bHitObstacle = GetWorld()->SweepSingleByChannel(ObstacleHit, Start, End, FQuat::Identity, ECC_WorldStatic, CharacterShape, ObstacleParams);
	if (bHitObstacle)
	{
		DestLocation = ObstacleHit.Location + ObstacleHit.ImpactNormal * CapsuleRadius;
	}

	AvatarChar->SetActorLocationAndRotation(DestLocation, DestRotation, false, nullptr, ETeleportType::TeleportPhysics);

	if (!SkillMontage.IsNull())
	{
		UAnimMontage* LoadedMontage = SkillMontage.LoadSynchronous();
		if (LoadedMontage)
		{
			UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
				this,
				TEXT("BackstabPlayTask"),
				LoadedMontage,
				1.0f,
				NAME_None,
				false,
				1.0f
			);

			if (Task)
			{
				CurrentPlayTask = Task;
				CurrentPlayTask->OnCompleted.AddDynamic(this, &ULostArkBackstabTeleportAbility::OnMontageCompleted);
				CurrentPlayTask->OnBlendOut.AddDynamic(this, &ULostArkBackstabTeleportAbility::OnMontageCompleted);
				CurrentPlayTask->OnInterrupted.AddDynamic(this, &ULostArkBackstabTeleportAbility::OnMontageInterrupted);
				CurrentPlayTask->OnCancelled.AddDynamic(this, &ULostArkBackstabTeleportAbility::OnMontageInterrupted);
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

void ULostArkBackstabTeleportAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	CachedTargetMonster = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}



