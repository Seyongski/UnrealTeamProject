#include "LostArk/Ability/Skills/LostArkSkill_Targeting.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "LostArk/Actor/LostArkTargetActor_GroundSelect.h"
#include "AbilitySystemComponent.h"
#include "InputAction.h"

ULostArkSkill_Targeting::ULostArkSkill_Targeting()
{
	TargetActorClass = ALostArkTargetActor_GroundSelect::StaticClass();
	SpawnedTargetActor = nullptr;

	ActivationOwnedTags.RemoveTag(FGameplayTag::RequestGameplayTag(FName("State.Attacking"), false));
	ActivationOwnedTags.RemoveTag(FGameplayTag::RequestGameplayTag(FName("State.Skill"), false));
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
		SpawnedTargetActor->SkillInputAction = SkillInputAction;
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
		ASC->RemoveLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("State.Skill"), false));
		ASC->RemoveLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName("State.Attacking"), false));
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

	// 여기 추가: 블루프린트에 타겟 위치 전달
	K2_OnTargetConfirmed(CachedTargetLocation);

	if (SpawnedTargetActor)
	{
		SpawnedTargetActor->Destroy();
		SpawnedTargetActor = nullptr;
	}

	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (AvatarActor)
	{
		// ?뺤젙 ?대┃ ?쒖젏?먮룄 理쒖쥌 ?寃??꾩튂瑜??ν빐 罹먮┃?곕? 利됯컖 ?뚯쟾?쒗궢?덈떎.
		FVector DirToTarget = CachedTargetLocation - AvatarActor->GetActorLocation();
		DirToTarget.Z = 0.f;
		if (!DirToTarget.IsNearlyZero())
		{
			AvatarActor->SetActorRotation(DirToTarget.Rotation());
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

	// 諛붾떏??Z(留덉슦???대┃ 吏???먯꽌 罹먮┃?곕뱾???됯퇏 以묒떖 ?믪씠(??90.f)留뚰겮 
	// ?ㅼ틪 援ъ껜??以묒떖 ?믪씠瑜?媛뺤젣 ?곸듅 蹂댁젙??以띾땲?? 
	// ?대젃寃??섎㈃ 怨듯넻 ApplyDamageShape ?댁쓽 ?⑥닚 Abs(Z李⑥씠) 鍮꾧탳 ?곗궛 ?띾룄瑜?100% 媛蹂띻쾶 ?좎??????덉뒿?덈떎.
	FVector AdjustedLocation = CachedTargetLocation + FVector(0.f, 0.f, 90.f);

	ApplyDamageShape(
		AdjustedLocation,
		LookRot,
		AvatarActor
	);
}





