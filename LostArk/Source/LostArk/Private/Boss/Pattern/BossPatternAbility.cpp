// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Pattern/BossPatternAbility.h"
#include "Boss/Pattern/BossPatternComponent.h"
#include "Boss/Pattern/PatternDataAsset.h"
#include "AbilitySystemComponent.h"

UBossPatternAbility::UBossPatternAbility()
{
	// 패턴은 순차적으로 하나씩 실행되므로 액터당 1인스턴스면 충분.
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	// 보스 AI 로직 -> 서버에서만 실행 (데디서버 프로젝트)
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void UBossPatternAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 브레인에서 이번에 실행할 패턴 데이터를 읽어온다
	AActor* Avatar = GetAvatarActorFromActorInfo();
	Brain = Avatar ? Avatar->FindComponentByClass<UBossPatternComponent>() : nullptr;
	PatternData = Brain ? Brain->GetActivePatternData() : nullptr;

	if (!PatternData)
	{
		// 데이터가 없으면 실패로 보고하고 종료
		if (Brain)
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
			Brain->OnPatternAbilityFinished(EPatternResult::Fail);
			return;
		}
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	RunPattern();
}

void UBossPatternAbility::RunPattern()
{
	// 시전 중 태그 부여 (카운터 윈도우 등)
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->AddLooseGameplayTags(PatternData->GrantedTagsDuringCast);
	}

	// TODO: 실제 연출 = 텔레그래프 데칼 -> UAbilityTask_PlayMontageAndWait(Montage)
	//       -> 애님노티파이 타이밍에 DamageEffect 적용.
	// 지금은 (Telegraph + Recovery) 시간 뒤 종료하는 스텁.
	const float Duration = FMath::Max(0.01f, PatternData->TelegraphTime + PatternData->RecoveryTime);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(PatternTimer, this, &UBossPatternAbility::FinishPattern, Duration, false);
	}
	else
	{
		FinishPattern();
	}
}

EPatternResult UBossPatternAbility::EvaluateResult() const
{
	if (PatternData && PatternData->FailTag.IsValid())
	{
		if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			if (ASC->HasMatchingGameplayTag(PatternData->FailTag))
			{
				return EPatternResult::Fail;
			}
		}
	}
	return EPatternResult::Success;
}

void UBossPatternAbility::FinishPattern()
{
	const EPatternResult Result = EvaluateResult();

	// 시전 중 부여했던 태그 회수
	if (PatternData)
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->RemoveLooseGameplayTags(PatternData->GrantedTagsDuringCast);
		}
	}

	// 재진입 방지: 먼저 어빌리티를 끝내고, 그 다음 브레인에 결과 보고
	UBossPatternComponent* LocalBrain = Brain;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);

	if (LocalBrain)
	{
		LocalBrain->OnPatternAbilityFinished(Result);
	}
}
