#include "Abilities/LostArkSkill_Targeting.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/LostArkTargetActor_GroundSelect.h"
#include "AbilitySystemComponent.h"

ULostArkSkill_Targeting::ULostArkSkill_Targeting()
{
	TargetActorClass = ALostArkTargetActor_GroundSelect::StaticClass();
	SpawnedTargetActor = nullptr;

	// 조준 시점에는 이동 제약을 받지 않도록 태그 등록 제거
	ActivationOwnedTags.RemoveTag(FGameplayTag::RequestGameplayTag(FName("State.Attacking")));
	ActivationOwnedTags.RemoveTag(FGameplayTag::RequestGameplayTag(FName("State.Skill")));
}

void ULostArkSkill_Targeting::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	UE_LOG(LogTemp, Warning, TEXT("[TargetingSkill] ActivateAbility Called (Direct Spawn Style)!"));

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		UE_LOG(LogTemp, Error, TEXT("[TargetingSkill] CommitAbility Failed!"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!TargetActorClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[TargetingSkill] TargetActorClass is Null!"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AActor* AvatarActor = ActorInfo->AvatarActor.Get();
	if (!AvatarActor)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.Owner = AvatarActor;

	// 태스크 대신 월드에 직접 타겟팅 액터 스폰
	SpawnedTargetActor = World->SpawnActor<ALostArkTargetActor_GroundSelect>(
		TargetActorClass,
		AvatarActor->GetActorLocation(),
		FRotator::ZeroRotator,
		SpawnParams
	);

	if (SpawnedTargetActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TargetingSkill] Direct TargetActor Spawn Success!"));
		SpawnedTargetActor->TargetRadius = DamageShapeParams.Radius;
		SpawnedTargetActor->StartTargeting(this);

		SpawnedTargetActor->OnTargetSelected.AddDynamic(this, &ULostArkSkill_Targeting::OnTargetSelectedDirect);
		SpawnedTargetActor->OnTargetCancelled.AddDynamic(this, &ULostArkSkill_Targeting::OnTargetCancelledDirect);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[TargetingSkill] Direct TargetActor Spawn Failed!"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
}

void ULostArkSkill_Targeting::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 스킬 시전 태그 수동 정리
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->RemoveLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("State.Skill")));
		ASC->RemoveLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("State.Attacking")));
	}

	if (SpawnedTargetActor)
	{
		SpawnedTargetActor->Destroy();
		SpawnedTargetActor = nullptr;
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void ULostArkSkill_Targeting::OnTargetSelectedDirect(const FVector& Location)
{
	CachedTargetLocation = Location;
	UE_LOG(LogTemp, Warning, TEXT("[TargetingSkill] Target Point Confirmed: %s"), *Location.ToString());

	// 타겟팅이 확정되었으므로 스킬 시전 태그 수동 부여 (캐릭터 행동 제어용)
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("State.Skill")));
		ASC->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("State.Attacking")));
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
				EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
			}
		}
		else
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		}
	}
	else
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void ULostArkSkill_Targeting::OnTargetCancelledDirect()
{
	UE_LOG(LogTemp, Warning, TEXT("[TargetingSkill] Targeting Cancelled."));
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void ULostArkSkill_Targeting::OnHitCheckReceived(FGameplayEventData Payload)
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!AvatarActor) return;

	FVector DirToTarget = CachedTargetLocation - AvatarActor->GetActorLocation();
	DirToTarget.Z = 0.f;
	FRotator LookRot = DirToTarget.Rotation();

	ApplyDamageShape(
		CachedTargetLocation,
		LookRot,
		AvatarActor
	);
}
