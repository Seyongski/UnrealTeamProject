#include "LostArk/Ability/Skills/LostArkSkill_Charging.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

ULostArkSkill_Charging::ULostArkSkill_Charging()
{
	MaxChargeTime = 2.0f;
	MaxDamageMultiplier = 2.0f;
	CurrentChargeMultiplier = 1.0f;
	bIsCharging = false;
}

void ULostArkSkill_Charging::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	bIsCharging = true;
	CurrentChargeMultiplier = 1.0f;

	// 부모의 공통 헬퍼를 사용해 네비게이션 강제 Abort 및 마우스 회전을 처리합니다.
	HandleActivationBasics(ActorInfo);

	UAbilityTask_WaitInputRelease* WaitReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, false);
	if (WaitReleaseTask)
	{
		WaitReleaseTask->OnRelease.AddDynamic(this, &ULostArkSkill_Charging::OnInputRelease);
		WaitReleaseTask->ReadyForActivation();
	}
}

void ULostArkSkill_Charging::OnInputRelease(float TimeHeld)
{
	bIsCharging = false;

	float ChargeRatio = FMath::Clamp(TimeHeld / MaxChargeTime, 0.0f, 1.0f);
	CurrentChargeMultiplier = FMath::Lerp(1.0f, MaxDamageMultiplier, ChargeRatio);

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

void ULostArkSkill_Charging::OnHitCheckReceived(FGameplayEventData Payload)
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!AvatarActor) return;

	float OriginalCoefficient = DamageShapeParams.DamageCoefficient;
	
	DamageShapeParams.DamageCoefficient *= CurrentChargeMultiplier;

	ApplyDamageShape(
		AvatarActor->GetActorLocation(),
		AvatarActor->GetActorRotation(),
		AvatarActor
	);

	DamageShapeParams.DamageCoefficient = OriginalCoefficient;
}



