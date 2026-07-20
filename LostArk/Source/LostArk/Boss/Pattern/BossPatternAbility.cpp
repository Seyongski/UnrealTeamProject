// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Pattern/BossPatternAbility.h"
#include "Boss/Pattern/BossPatternComponent.h"
#include "Boss/Pattern/PatternDataAsset.h"
#include "Boss/Combat/BossCounterComponent.h"
#include "Boss/Combat/BossJustGuardComponent.h"
#include "Boss/BossGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"

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

	AActor* Avatar = GetAvatarActorFromActorInfo();
	Brain = Avatar ? Avatar->FindComponentByClass<UBossPatternComponent>() : nullptr;
	PatternData = Brain ? Brain->GetActivePatternData() : nullptr;

	if (!PatternData || PatternData->Steps.Num() == 0)
	{
		FinishPattern();
		return;
	}

	RunStep(PatternData->EntryStepId);
}

void UBossPatternAbility::RunStep(FName StepId)
{
	const FPatternStep* Step = PatternData ? PatternData->FindStep(StepId) : nullptr;
	if (!Step)
	{
		FinishPattern();
		return;
	}

	CurrentStepId = StepId;
	bStepActive = true;

	// 윈도우 태그 부여 후 태그 변화 감시 등록 (부여 자체는 감시 전에 하여 자기트리거 방지)
	// 부여한 태그는 AppliedStepTags 에 기록 -> CleanupStep 이 중복 호출돼도 한 번만 제거됨
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		AppliedStepTags = Step->GrantedTagsDuringStep;
		ASC->AddLooseGameplayTags(AppliedStepTags);
		TagChangeHandle = ASC->RegisterGenericGameplayTagEvent().AddUObject(this, &UBossPatternAbility::OnAnyTagChanged);
	}

	// TODO: Step->DamageEffect 는 몽타주 애님노티파이 타이밍에 적용 (지금은 데이터만 보관)

	// 소프트 레퍼런스 해석: 페이즈 진입 시 프리로드되어 있는 게 정상.
	// 아직 로드 전이면(프리로드 미완) 동기 로드로 폴백 (히치 가능성 있지만 안전)
	UAnimMontage* Montage = Step->Montage.Get();
	if (!Montage && !Step->Montage.IsNull())
	{
		Montage = Step->Montage.LoadSynchronous();
	}

	if (Montage)
	{
		MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, NAME_None, Montage, 1.f, NAME_None, /*bStopWhenAbilityEnds=*/true);
		// OnBlendOut: 몽타주가 블렌드 아웃을 시작하는 순간 -> 다음 스텝 몽타주를 겹쳐 재생(부드러운 전환)
		MontageTask->OnBlendOut.AddDynamic(this, &UBossPatternAbility::OnStepMontageEnded);
		// OnCompleted/OnInterrupted: 블렌드 아웃이 0이거나 외부 중단된 경우 대비 (가드로 중복 무시)
		MontageTask->OnCompleted.AddDynamic(this, &UBossPatternAbility::OnStepMontageEnded);
		MontageTask->OnInterrupted.AddDynamic(this, &UBossPatternAbility::OnStepMontageEnded);
		MontageTask->ReadyForActivation();
	}
	else if (UWorld* World = GetWorld())
	{
		// 몽타주 미지정 스텝: 짧은 폴백 타이머 후 종료 처리 (미완성/테스트용)
		World->GetTimerManager().SetTimer(NoMontageTimer, this, &UBossPatternAbility::OnStepMontageEnded, 0.5f, false);
	}
	else
	{
		OnStepMontageEnded();
	}
}

bool UBossPatternAbility::EvaluateBranches(FName& OutNextStepId) const
{
	const FPatternStep* Step = PatternData ? PatternData->FindStep(CurrentStepId) : nullptr;
	if (!Step)
	{
		return false;
	}

	const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		return false;
	}

	FGameplayTagContainer OwnedTags;
	ASC->GetOwnedGameplayTags(OwnedTags);

	for (const FPatternBranch& Branch : Step->Branches)
	{
		if (!Branch.Condition.IsEmpty() && Branch.Condition.Matches(OwnedTags))
		{
			OutNextStepId = Branch.NextStepId;
			return true;
		}
	}
	return false;
}

void UBossPatternAbility::OnAnyTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	// 스텝 재생 중에만. 분기 조건 충족 시 몽타주를 즉시 끊고 이동
	if (!bStepActive)
	{
		return;
	}

	FName NextStepId;
	if (EvaluateBranches(NextStepId))
	{
		ResolveStep(NextStepId);
	}
}

void UBossPatternAbility::OnStepMontageEnded()
{
	// 정상 종료: 분기 평가 -> 없으면 기본 다음 스텝
	if (!bStepActive)
	{
		return;
	}

	FName NextStepId;
	if (!EvaluateBranches(NextStepId))
	{
		const FPatternStep* Step = PatternData ? PatternData->FindStep(CurrentStepId) : nullptr;
		NextStepId = Step ? Step->NextStepDefault : FName();
	}

	ResolveStep(NextStepId);
}

void UBossPatternAbility::ResolveStep(FName NextStepId)
{
	if (!bStepActive)
	{
		return;
	}
	bStepActive = false;	// 이후 stale 콜백(우리가 끊은 몽타주의 Interrupted 등)을 무시

	CleanupStep();

	if (NextStepId.IsNone())
	{
		FinishPattern();
	}
	else
	{
		RunStep(NextStepId);
	}
}

void UBossPatternAbility::CleanupStep()
{
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		if (TagChangeHandle.IsValid())
		{
			ASC->RegisterGenericGameplayTagEvent().Remove(TagChangeHandle);
		}
		// 기록해둔 태그만 제거하고 비움 -> CleanupStep 이 두 번 불려도 이중 제거 안 됨
		if (!AppliedStepTags.IsEmpty())
		{
			ASC->RemoveLooseGameplayTags(AppliedStepTags);
			AppliedStepTags.Reset();
		}
	}
	TagChangeHandle.Reset();

	// 카운터 창 강제 닫기: 분기 인터럽트로 몽타주가 끊길 때 NotifyEnd 누락 케이스 방어
	// (창은 스텝 몽타주 안에서만 열리므로 스텝 정리 시점에 닫는 게 항상 안전)
	if (AActor* Avatar = GetAvatarActorFromActorInfo())
	{
		if (UBossCounterComponent* Counter = Avatar->FindComponentByClass<UBossCounterComponent>())
		{
			Counter->CloseWindow();
		}

		// 저스트가드도 동일: NotifyEnd 누락 방어로 창 강제 닫기 + 전용 가드 대상 리셋.
		// ExclusiveGuardPlayer 가 남으면 다음 '전원 가드' 패턴이 그 1명으로 제한되는 leak 이 생긴다.
		if (UBossJustGuardComponent* JustGuard = Avatar->FindComponentByClass<UBossJustGuardComponent>())
		{
			JustGuard->CloseWindow();
			JustGuard->ClearExclusiveGuardPlayer();
		}
	}

	if (MontageTask)
	{
		MontageTask->EndTask();
		MontageTask = nullptr;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(NoMontageTimer);
	}
}

void UBossPatternAbility::FinishPattern()
{
	// 재진입 방지: 먼저 어빌리티를 끝내고(EndAbility에서 정리), 그 다음 브레인에 보고
	UBossPatternComponent* LocalBrain = Brain;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);

	if (LocalBrain)
	{
		LocalBrain->OnPatternAbilityFinished();
	}
}

void UBossPatternAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	bStepActive = false;
	CleanupStep();

	// 패턴 결과 태그(State.Boss.PatternResult.*) 일괄 제거 -> 다음 패턴 Branch 로 안 샘
	// (장판/잡기가 AddLooseGameplayTags 로 부여한 것. 카운트 무관하게 0으로 리셋)
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		FGameplayTagContainer OwnedTags;
		ASC->GetOwnedGameplayTags(OwnedTags);
		const FGameplayTagContainer ResultTags =
			OwnedTags.Filter(FGameplayTagContainer(LostArkTags::State_Boss_PatternResult));
		for (const FGameplayTag& Tag : ResultTags)
		{
			ASC->SetLooseGameplayTagCount(Tag, 0);
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
