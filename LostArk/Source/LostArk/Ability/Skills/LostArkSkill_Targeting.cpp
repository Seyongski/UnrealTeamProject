#include "LostArk/Ability/Skills/LostArkSkill_Targeting.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "LostArk/Actor/LostArkTargetActor_GroundSelect.h"
#include "AbilitySystemComponent.h"

ULostArkSkill_Targeting::ULostArkSkill_Targeting()
{
	TargetActorClass = ALostArkTargetActor_GroundSelect::StaticClass();
	SpawnedTargetActor = nullptr;

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

	// 시전 시작 시점에 즉각 이동 정지 및 초기 마우스 방향 회전을 수행합니다.
	HandleActivationBasics(ActorInfo);

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

	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (AvatarActor)
	{
		// 확정 클릭 시점에도 최종 타겟 위치를 향해 캐릭터를 즉각 회전시킵니다.
		FVector DirToTarget = CachedTargetLocation - AvatarActor->GetActorLocation();
		DirToTarget.Z = 0.f;
		if (!DirToTarget.IsNearlyZero())
		{
			AvatarActor->SetActorRotation(DirToTarget.Rotation());
		}
	}

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

	// 바닥의 Z(마우스 클릭 지점)에서 캐릭터들의 평균 중심 높이(약 90.f)만큼 
	// 스캔 구체의 중심 높이를 강제 상승 보정해 줍니다. 
	// 이렇게 하면 공통 ApplyDamageShape 내의 단순 Abs(Z차이) 비교 연산 속도를 100% 가볍게 유지할 수 있습니다.
	FVector AdjustedLocation = CachedTargetLocation + FVector(0.f, 0.f, 90.f);

	ApplyDamageShape(
		AdjustedLocation,
		LookRot,
		AvatarActor
	);
}



