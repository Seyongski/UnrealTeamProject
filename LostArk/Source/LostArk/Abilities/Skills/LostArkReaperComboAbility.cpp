#include "Abilities/Skills/LostArkReaperComboAbility.h"
#include "Actor/LostArkShadowClone.h"
#include "Character/LostArkCharacter.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"

ULostArkReaperComboAbility::ULostArkReaperComboAbility()
{
	CloneSpawnOffsetForward = 0.f;
	CloneSpawnOffsetRight = 150.f;
	FinalDashDistance = 600.f;
	FinalDashDuration = 0.3f;
}

void ULostArkReaperComboAbility::PlayComboSegment(int32 Index)
{
	Super::PlayComboSegment(Index);

	if (Index == 2)
	{
		APawn* AvatarPawn = Cast<APawn>(CurrentActorInfo->AvatarActor.Get());
		if (AvatarPawn && bApplyDashForce)
		{
			FVector DashDirection = AvatarPawn->GetActorForwardVector();
			float CalcDashSpeed = FinalDashDuration > 0.f ? (FinalDashDistance / FinalDashDuration) : 0.f;

			UAbilityTask_ApplyRootMotionConstantForce* ForceTask = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
				this,
				TEXT("ReaperComboDashForceTask"),
				DashDirection,
				CalcDashSpeed,
				FinalDashDuration,
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
	else if (Index == 1)
	{
		APawn* AvatarPawn = Cast<APawn>(CurrentActorInfo->AvatarActor.Get());
		if (AvatarPawn)
		{
			if (!ShadowCloneClass.IsNull())
			{
				UClass* LoadedShadowCloneClass = ShadowCloneClass.LoadSynchronous();
				USkeletalMeshComponent* SourceMesh = AvatarPawn->FindComponentByClass<USkeletalMeshComponent>();
				ALostArkCharacter* LAChar = Cast<ALostArkCharacter>(AvatarPawn);
				USkeletalMeshComponent* SourceWeapon = LAChar ? LAChar->GetWeaponMesh() : nullptr;

				if (LoadedShadowCloneClass && SourceMesh && AvatarPawn->HasAuthority())
				{
					FVector PlayerLoc = AvatarPawn->GetActorLocation();
					FRotator PlayerRot = AvatarPawn->GetActorRotation();
					FVector ForwardVector = FRotationMatrix(PlayerRot).GetScaledAxis(EAxis::X);
					FVector RightVector = FRotationMatrix(PlayerRot).GetScaledAxis(EAxis::Y);
					
					FActorSpawnParameters SpawnParams;
					SpawnParams.Owner = AvatarPawn;
					SpawnParams.Instigator = AvatarPawn;
					SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

					// Left Clone
					if (!LeftCloneMontage.IsNull())
					{
						FVector LeftLoc = PlayerLoc + ForwardVector * CloneSpawnOffsetForward - RightVector * CloneSpawnOffsetRight;
						LeftLoc.Z = SourceMesh->GetComponentLocation().Z;
						ALostArkShadowClone* LeftClone = GetWorld()->SpawnActor<ALostArkShadowClone>(LoadedShadowCloneClass, LeftLoc, PlayerRot, SpawnParams);
						if (LeftClone)
						{
							LeftClone->InitShadow(SourceMesh, SourceWeapon, LeftCloneMontage.LoadSynchronous(), 1.f, 0.f, 0.f, GetAbilitySystemComponentFromActorInfo(), DamageEffectClass, CloneDamageShapeParams);
						}
					}

					// Right Clone
					if (!RightCloneMontage.IsNull())
					{
						FVector RightLoc = PlayerLoc + ForwardVector * CloneSpawnOffsetForward + RightVector * CloneSpawnOffsetRight;
						RightLoc.Z = SourceMesh->GetComponentLocation().Z;
						ALostArkShadowClone* RightClone = GetWorld()->SpawnActor<ALostArkShadowClone>(LoadedShadowCloneClass, RightLoc, PlayerRot, SpawnParams);
						if (RightClone)
						{
							RightClone->InitShadow(SourceMesh, SourceWeapon, RightCloneMontage.LoadSynchronous(), 1.f, 0.f, 0.f, GetAbilitySystemComponentFromActorInfo(), DamageEffectClass, CloneDamageShapeParams);
						}
					}
				}
			}
		}
	}
}

void ULostArkReaperComboAbility::OnHitCheckReceived(FGameplayEventData Payload)
{
	FDamageShapeParams OldParams = DamageShapeParams;
	if (CurrentComboIndex == 2)
	{
		DamageShapeParams = FinalDashDamageShapeParams;
	}

	Super::OnHitCheckReceived(Payload);

	DamageShapeParams = OldParams;
}

