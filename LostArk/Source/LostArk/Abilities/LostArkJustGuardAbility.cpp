#include "Abilities/LostArkJustGuardAbility.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Core/LostArkPlayerController.h"
#include "EngineUtils.h"
#include "TimerManager.h"

ULostArkJustGuardAbility::ULostArkJustGuardAbility()
{
	// 가드 모션 중 유지될 태그
	FGameplayTag GuardingTag = FGameplayTag::RequestGameplayTag(FName("State.Player.Guarding"), false);
	ActivationOwnedTags.AddTag(GuardingTag);

	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Skill.JustGuard"), false));
}

void ULostArkJustGuardAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 1. 留덉슦??諛⑺뼢?쇰줈 罹먮┃???뚯쟾
	RotateToCursor(ActorInfo);

	// 2. 蹂댁뒪?먭쾶 ?대깽???꾩넚
	SendJustGuardEventToBoss(ActorInfo);

	// 3. ?먯젙 ?대깽??由ъ뒯 ?쒖옉 (蹂묐젹)
	UAbilityTask_WaitGameplayEvent* WaitSuccessEvent = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, FGameplayTag::RequestGameplayTag("Event.Player.JustGuard.Success", false));
	WaitSuccessEvent->EventReceived.AddDynamic(this, &ULostArkJustGuardAbility::OnSuccessEventReceived);
	WaitSuccessEvent->ReadyForActivation();

	UAbilityTask_WaitGameplayEvent* WaitFailEvent = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, FGameplayTag::RequestGameplayTag("Event.Player.JustGuard.Fail", false));
	WaitFailEvent->EventReceived.AddDynamic(this, &ULostArkJustGuardAbility::OnFailEventReceived);
	WaitFailEvent->ReadyForActivation();

	// 4. ?쒖옉 紐쏀?二??ъ깮
	if (GuardStartMontage)
	{
		UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, GuardStartMontage, 1.0f);
		Task->OnBlendOut.AddDynamic(this, &ULostArkJustGuardAbility::OnStartMontageCompleted);
		Task->OnCompleted.AddDynamic(this, &ULostArkJustGuardAbility::OnStartMontageCompleted);
		Task->OnInterrupted.AddDynamic(this, &ULostArkJustGuardAbility::OnMontageCancelledOrInterrupted);
		Task->OnCancelled.AddDynamic(this, &ULostArkJustGuardAbility::OnMontageCancelledOrInterrupted);
		Task->ReadyForActivation();
	}
	else
	{
		PlayLoopMontage();
	}

	// 5. 臾댄븳猷⑦봽 諛⑹???理쒕? ?좎??쒓컙 ??대㉧ ?ㅼ젙 (?ν뙋 諛?臾댄뙋???덉쇅泥섎━)
	GetWorld()->GetTimerManager().SetTimer(GuardTimeoutTimerHandle, this, &ULostArkJustGuardAbility::EndGuardDueToTimeout, MaxGuardDuration, false);
}

void ULostArkJustGuardAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (GuardTimeoutTimerHandle.IsValid())
	{
		GetWorld()->GetTimerManager().ClearTimer(GuardTimeoutTimerHandle);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void ULostArkJustGuardAbility::RotateToCursor(const FGameplayAbilityActorInfo* ActorInfo)
{
	ALostArkPlayerController* PC = Cast<ALostArkPlayerController>(ActorInfo->PlayerController.Get());
	ACharacter* AvatarCharacter = Cast<ACharacter>(ActorInfo->AvatarActor.Get());

	if (PC && AvatarCharacter)
	{
		FHitResult HitResult;
		if (PC->GetHitResultUnderCursor(ECC_Visibility, true, HitResult))
		{
			FVector TargetLocation = HitResult.Location;
			FVector CurrentLocation = AvatarCharacter->GetActorLocation();
			TargetLocation.Z = CurrentLocation.Z;

			FRotator TargetRotation = UKismetMathLibrary::FindLookAtRotation(CurrentLocation, TargetLocation);
			AvatarCharacter->SetActorRotation(TargetRotation);
		}
	}
}

void ULostArkJustGuardAbility::SendJustGuardEventToBoss(const FGameplayAbilityActorInfo* ActorInfo)
{
	UWorld* World = GetWorld();
	if (!World) return;

	AActor* BossActor = FindBossActor(World);
	if (BossActor)
	{
		FGameplayEventData Payload;
		Payload.Instigator = ActorInfo->AvatarActor.Get();
		
		FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(FName("Event.Boss.JustGuardInput"), false);
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(BossActor, EventTag, Payload);
	}
}

AActor* ULostArkJustGuardAbility::FindBossActor(UWorld* World) const
{
	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		ACharacter* Char = *It;
		if (Char == GetAvatarActorFromActorInfo()) continue;

		if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Char))
		{
			UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent();
			if (ASC && ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("State.Boss.JustGuardable"), false)))
			{
				return Char;
			}
		}
	}

	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		ACharacter* Char = *It;
		if (Char != GetAvatarActorFromActorInfo() && !Char->IsPlayerControlled())
		{
			return Char;
		}
	}

	return nullptr;
}

void ULostArkJustGuardAbility::OnStartMontageCompleted()
{
	PlayLoopMontage();
}

void ULostArkJustGuardAbility::PlayLoopMontage()
{
	if (GuardLoopMontage)
	{
		UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, GuardLoopMontage, 1.0f);
		Task->OnInterrupted.AddDynamic(this, &ULostArkJustGuardAbility::OnMontageCancelledOrInterrupted);
		Task->OnCancelled.AddDynamic(this, &ULostArkJustGuardAbility::OnMontageCancelledOrInterrupted);
		// 猷⑦봽 紐쏀?二쇱씠誘濡??먯뿰?ㅻ읇寃??앸굹??寃쎌슦(OnCompleted)??Timeout???섑빐 罹붿뒳?섎뒗 寃껉낵 ?숈씪?섍쾶 泥섎━
		Task->OnCompleted.AddDynamic(this, &ULostArkJustGuardAbility::OnMontageCancelledOrInterrupted);
		Task->ReadyForActivation();
	}
}

void ULostArkJustGuardAbility::OnSuccessEventReceived(FGameplayEventData Payload)
{
	if (GuardTimeoutTimerHandle.IsValid())
	{
		GetWorld()->GetTimerManager().ClearTimer(GuardTimeoutTimerHandle);
	}

	if (GuardSuccessMontage)
	{
		// ?꾩옱 紐쏀?二쇰? ?딄퀬 ?깃났 紐쏀?二??ъ깮
		UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, GuardSuccessMontage, 1.0f);
		Task->OnBlendOut.AddDynamic(this, &ULostArkJustGuardAbility::OnResultMontageCompleted);
		Task->OnCompleted.AddDynamic(this, &ULostArkJustGuardAbility::OnResultMontageCompleted);
		Task->OnInterrupted.AddDynamic(this, &ULostArkJustGuardAbility::OnResultMontageCompleted);
		Task->OnCancelled.AddDynamic(this, &ULostArkJustGuardAbility::OnResultMontageCompleted);
		Task->ReadyForActivation();
	}
	else
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
	}
}

void ULostArkJustGuardAbility::OnFailEventReceived(FGameplayEventData Payload)
{
	if (GuardTimeoutTimerHandle.IsValid())
	{
		GetWorld()->GetTimerManager().ClearTimer(GuardTimeoutTimerHandle);
	}

	if (GuardFailMontage)
	{
		UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, GuardFailMontage, 1.0f);
		Task->OnBlendOut.AddDynamic(this, &ULostArkJustGuardAbility::OnResultMontageCompleted);
		Task->OnCompleted.AddDynamic(this, &ULostArkJustGuardAbility::OnResultMontageCompleted);
		Task->OnInterrupted.AddDynamic(this, &ULostArkJustGuardAbility::OnResultMontageCompleted);
		Task->OnCancelled.AddDynamic(this, &ULostArkJustGuardAbility::OnResultMontageCompleted);
		Task->ReadyForActivation();
	}
	else
	{
		EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
	}
}

void ULostArkJustGuardAbility::OnResultMontageCompleted()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void ULostArkJustGuardAbility::EndGuardDueToTimeout()
{
	// ??꾩븘??諛쒖깮 ???λ젰 醫낅즺 (媛?쒓? ?먯뿰?ㅻ읇寃??由?
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, false);
}

void ULostArkJustGuardAbility::OnMontageCancelledOrInterrupted()
{
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(), true, true);
}


