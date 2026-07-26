#include "Abilities/LostArkSkill_Targeting.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Actor/LostArkTargetActor_GroundSelect.h"
#include "AbilitySystemComponent.h"
#include "InputAction.h"
#include "Core/LostArkCombatInterface.h"

ULostArkSkill_Targeting::ULostArkSkill_Targeting()
{
	TargetActorClass = ALostArkTargetActor_GroundSelect::StaticClass();
	SpawnedTargetActor = nullptr;

	ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateYes;

	ActivationOwnedTags.RemoveTag(FGameplayTag::RequestGameplayTag(FName("State.Attacking"), false));
	ActivationOwnedTags.RemoveTag(FGameplayTag::RequestGameplayTag(FName("State.Skill"), false));
}

void ULostArkSkill_Targeting::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	UE_LOG(LogTemp, Warning, TEXT("[TargetingSkill] ActivateAbility Called (Direct Spawn Style)!"));
	bIsTargetConfirmed = false;

	// 1차 클릭 시에는 CommitAbility를 수행하지 않고, 2차 타겟 지정 확정 시점에 CommitAbility를 수행합니다.

	// ?쒖쟾 ?쒖옉 ?쒖젏??利됯컖 ?대룞 ?뺤? 諛?珥덇린 留덉슦??諛⑺뼢 ?뚯쟾???섑뻾?⑸땲??
	HandleActivationBasics(ActorInfo);

	K2_ActivateAbility();

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

	if (ActorInfo->IsLocallyControlled())
	{
		SpawnedTargetActor = World->SpawnActor<ALostArkTargetActor_GroundSelect>(
			TargetActorClass,
			AvatarActor->GetActorLocation(),
			FRotator::ZeroRotator,
			SpawnParams
		);
	}

	if (SpawnedTargetActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TargetingSkill] Direct TargetActor Spawn Success!"));
		SpawnedTargetActor->TargetRadius = DamageShapeParams.Radius;
		SpawnedTargetActor->SkillInputAction = SkillInputAction;
		SpawnedTargetActor->StartTargeting(this);

		SpawnedTargetActor->OnTargetSelected.AddDynamic(this, &ULostArkSkill_Targeting::OnTargetSelectedDirect);
		SpawnedTargetActor->OnTargetCancelled.AddDynamic(this, &ULostArkSkill_Targeting::OnTargetCancelledDirect);
	}
	else if (ActorInfo->IsLocallyControlled())
	{
		UE_LOG(LogTemp, Error, TEXT("[TargetingSkill] Direct TargetActor Spawn Failed!"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
}

void ULostArkSkill_Targeting::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	UE_LOG(LogTemp, Warning, TEXT("[TargetingSkill] EndAbility called on %s. Cancelled: %d"), HasAuthority(&ActivationInfo) ? TEXT("SERVER") : TEXT("CLIENT"), bWasCancelled);

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->RemoveLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("State.Skill"), false));
		ASC->RemoveLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("State.Attacking"), false));
	}

	if (AActor* AvatarActor = GetAvatarActorFromActorInfo())
	{
		if (ILostArkCombatInterface* CombatInterface = Cast<ILostArkCombatInterface>(AvatarActor))
		{
			CombatInterface->SetCombatState(FGameplayTag::EmptyTag);
		}
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
	if (bIsTargetConfirmed) return;
	bIsTargetConfirmed = true;

	CachedTargetLocation = Location;
	UE_LOG(LogTemp, Warning, TEXT("[TargetingSkill] Target Point Confirmed: %s"), *Location.ToString());

	if (SpawnedTargetActor)
	{
		SpawnedTargetActor->Destroy();
		SpawnedTargetActor = nullptr;
	}

	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (AvatarActor && AvatarActor->HasAuthority())
	{
		ContinueTargetingExecution();
	}
	else
	{
		Server_OnTargetSelected(Location);
	}
}

void ULostArkSkill_Targeting::Server_OnTargetSelected_Implementation(FVector Location)
{
	CachedTargetLocation = Location;
	ContinueTargetingExecution();
}

void ULostArkSkill_Targeting::ContinueTargetingExecution()
{
	UE_LOG(LogTemp, Warning, TEXT("[TargetingSkill] ContinueTargetingExecution called on %s"), HasAuthority(&CurrentActivationInfo) ? TEXT("SERVER") : TEXT("CLIENT"));

	if (!CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
	{
		UE_LOG(LogTemp, Error, TEXT("[TargetingSkill] CommitAbility Failed at Second Click! (Already on cooldown or no cost)"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	Multicast_OnTargetConfirmed(CachedTargetLocation);

	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (AvatarActor)
	{
		FVector DirToTarget = CachedTargetLocation - AvatarActor->GetActorLocation();
		DirToTarget.Z = 0.f;
		if (!DirToTarget.IsNearlyZero())
		{
			AvatarActor->SetActorRotation(DirToTarget.Rotation());
		}

		if (ILostArkCombatInterface* CombatInterface = Cast<ILostArkCombatInterface>(AvatarActor))
		{
			CombatInterface->SetCombatState(FGameplayTag::RequestGameplayTag(FName("State.Attacking"), false));
		}
	}

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("State.Skill"), false));
		ASC->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("State.Attacking"), false));
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
				UE_LOG(LogTemp, Warning, TEXT("[TargetingSkill] PlayMontageAndWait Task Created Successfully"));
				CurrentPlayTask = Task;
				CurrentPlayTask->OnCompleted.AddDynamic(this, &ULostArkSkillGameplayAbility::OnMontageCompleted);
				CurrentPlayTask->OnBlendOut.AddDynamic(this, &ULostArkSkillGameplayAbility::OnMontageCompleted);
				CurrentPlayTask->OnInterrupted.AddDynamic(this, &ULostArkSkillGameplayAbility::OnMontageInterrupted);
				CurrentPlayTask->OnCancelled.AddDynamic(this, &ULostArkSkillGameplayAbility::OnMontageInterrupted);
				CurrentPlayTask->ReadyForActivation();
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[TargetingSkill] PlayMontageAndWait Task Failed to Create"));
				EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[TargetingSkill] Failed to load SkillMontage"));
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[TargetingSkill] SkillMontage is Null"));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void ULostArkSkill_Targeting::OnTargetCancelledDirect()
{
	UE_LOG(LogTemp, Warning, TEXT("[TargetingSkill] Targeting Cancelled."));
	
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (AvatarActor && !AvatarActor->HasAuthority())
	{
		Server_OnTargetCancelled();
	}
	
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void ULostArkSkill_Targeting::Server_OnTargetCancelled_Implementation()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void ULostArkSkill_Targeting::OnHitCheckReceived(FGameplayEventData Payload)
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!AvatarActor) return;

	FVector DirToTarget = CachedTargetLocation - AvatarActor->GetActorLocation();
	DirToTarget.Z = 0.f;
	FRotator LookRot = DirToTarget.Rotation();

	// 諛붾떏??Z(留덉슦???대┃ 吏???먯꽌 罹먮┃?곕뱾???됯퇏 以묒떖 ?믪씠(??90.f)留뚰겮 
	// ?ㅼ틪 援ъ껜??以묒떖 ?믪씠瑜?媛뺤젣 ?곸듅 蹂댁젙??以띾땲?? 
	// ?대젃寃??섎㈃ 怨듯넻 ApplyDamageShape ?댁쓽 ?⑥닚 Abs(Z李⑥씠) 鍮꾧탳 ?곗궛 ?띾룄瑜?100% 媛蹂띻쾶 ?좎??????덉뒿?덈떎.
	FVector AdjustedLocation = CachedTargetLocation + FVector(0.f, 0.f, 90.f);

	// 장판 타겟팅 스킬은 마우스 클릭 타겟점이 중심점이 되도록 ForwardOffset 오차를 0으로 보정
	float SavedForwardOffset = DamageShapeParams.ForwardOffset;
	DamageShapeParams.ForwardOffset = 0.f;

	ApplyDamageShape(
		AdjustedLocation,
		LookRot,
		AvatarActor
	);

	DamageShapeParams.ForwardOffset = SavedForwardOffset;
}

void ULostArkSkill_Targeting::Multicast_OnTargetConfirmed_Implementation(FVector TargetLocation)
{
	K2_OnTargetConfirmed(TargetLocation);
}





