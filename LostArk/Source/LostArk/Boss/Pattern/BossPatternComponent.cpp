// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Pattern/BossPatternComponent.h"
#include "Boss/Pattern/BossPatternAbility.h"
#include "Boss/Pattern/BossPhaseDataAsset.h"
#include "Boss/Pattern/PatternDataAsset.h"
#include "Boss/Targeting/BossTargetingComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Engine/AssetManager.h"

UBossPatternComponent::UBossPatternComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UBossPatternComponent::InitializePatterns()
{
	// 기본 패턴 어빌리티를 보스 ASC에 부여 (권한 있는 쪽에서만, 중복 방지)
	if (UAbilitySystemComponent* ASC = GetASC())
	{
		if (DefaultPatternAbility && ASC->IsOwnerActorAuthoritative() && !PatternAbilityHandle.IsValid())
		{
			PatternAbilityHandle = ASC->GiveAbility(
				FGameplayAbilitySpec(DefaultPatternAbility, 1, INDEX_NONE, GetOwner()));
		}
	}

	// 타겟팅 컴포넌트 캐시 (패턴 시작 시 선정에 사용)
	if (AActor* Owner = GetOwner())
	{
		CachedTargeting = Owner->FindComponentByClass<UBossTargetingComponent>();
	}
}

UAbilitySystemComponent* UBossPatternComponent::GetASC() const
{
	return GetOwner() ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()) : nullptr;
}

void UBossPatternComponent::StartCombat()
{
	// 이미 전투 중이면 무시 (재빙의 등으로 중복 호출 방지)
	if (CurrentPhaseIndex != INDEX_NONE || Phases.Num() == 0)
	{
		return;
	}
	EnterPhase(0);
}

void UBossPatternComponent::StopCombat()
{
	bCombatStopped = true;
	PendingPhaseIndex = INDEX_NONE;

	// 그로기 대기 등으로 걸려 있던 발동 재시도 중단
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ActivateRetryTimer);
	}
}

void UBossPatternComponent::NotifyHealthPercent(float HealthPercent)
{
	// 전투 시작 전이면 무시
	if (CurrentPhaseIndex == INDEX_NONE)
	{
		return;
	}

	// 현재 체력으로 진입 가능한 가장 깊은 페이즈 찾기 (Phases 는 EnterHealthPercent 내림차순 가정)
	int32 Target = CurrentPhaseIndex;
	for (int32 i = 0; i < Phases.Num(); ++i)
	{
		if (Phases[i] && HealthPercent <= Phases[i]->EnterHealthPercent)
		{
			Target = i;
		}
	}

	// 더 깊은 페이즈로 가야 하면 '예약'만. 현재 패턴은 끝까지 실행됨
	if (Target > CurrentPhaseIndex)
	{
		PendingPhaseIndex = Target;
	}
}

void UBossPatternComponent::EnterPhase(int32 PhaseIndex)
{
	if (!Phases.IsValidIndex(PhaseIndex) || !Phases[PhaseIndex])
	{
		return;
	}

	CurrentPhaseIndex = PhaseIndex;
	UBossPhaseDataAsset* Phase = Phases[PhaseIndex];

	// 이 페이즈의 몽타주들 프리로드 + 이전 페이즈 몽타주 메모리 해제
	PreloadPhaseAssets(Phase);

	// 페이즈 태그 교체 (다른 시스템/어빌리티가 현재 페이즈를 태그로 조회 가능)
	SwapPhaseTag(Phase->PhaseTag);

	// 전환 기믹이 있으면 먼저 실행 -> 끝나면 일반 패턴 선택
	if (Phase->TransitionGimmick)
	{
		bRunningTransition = true;
		RunPatternData(Phase->TransitionGimmick);
	}
	else
	{
		RunNextPattern();
	}
}

void UBossPatternComponent::SwapPhaseTag(const FGameplayTag& NewPhaseTag)
{
	UAbilitySystemComponent* ASC = GetASC();
	if (!ASC)
	{
		return;
	}

	if (CurrentPhaseTag.IsValid())
	{
		ASC->RemoveLooseGameplayTag(CurrentPhaseTag);
	}
	if (NewPhaseTag.IsValid())
	{
		ASC->AddLooseGameplayTag(NewPhaseTag);
	}
	CurrentPhaseTag = NewPhaseTag;
}

void UBossPatternComponent::RunNextPattern()
{
	UBossPhaseDataAsset* Phase = Phases.IsValidIndex(CurrentPhaseIndex) ? Phases[CurrentPhaseIndex] : nullptr;
	if (!Phase)
	{
		return;
	}

	if (UPatternDataAsset* Next = Phase->PickRandomWeighted())
	{
		RunPatternData(Next);
	}
	// 뽑을 패턴이 없으면 대기 (StartCombat/전환 시 다시 시도됨)
}

void UBossPatternComponent::RunPatternData(UPatternDataAsset* Data)
{
	ActivePatternData = Data;

	// 패턴 시작 시 타겟 선정 (정책은 패턴 데이터에서)
	if (Data && Data->bReselectTargetOnStart && CachedTargeting)
	{
		CachedTargeting->SelectTarget(Data->TargetPolicy);
	}

	// 어빌리티 종료 콜백 안에서 곧바로 재활성화하면 재진입 문제 -> 다음 틱에 활성화
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(this, &UBossPatternComponent::ActivateAbilityNow);
	}
}

void UBossPatternComponent::ActivateAbilityNow()
{
	// 정지 후엔 발동/재시도 모두 금지 (RunPatternData 의 다음 틱 예약이 정지 직후 도착하는 경우 포함)
	if (bCombatStopped)
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetASC();
	if (!ASC)
	{
		return;
	}

	// 발동 실패(그로기 등으로 Activation Blocked) 시 잠시 후 재시도
	// -> 그로기가 풀릴 때까지 자연스럽게 대기하는 효과
	if (!ASC->TryActivateAbility(PatternAbilityHandle))
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				ActivateRetryTimer, this, &UBossPatternComponent::ActivateAbilityNow, 0.5f, false);
		}
	}
}

void UBossPatternComponent::PreloadPhaseAssets(const UBossPhaseDataAsset* Phase)
{
	// 이 페이즈에서 재생될 수 있는 모든 몽타주 경로 수집
	TArray<FSoftObjectPath> Paths;
	auto CollectFromPattern = [&Paths](const UPatternDataAsset* Pattern)
	{
		if (!Pattern)
		{
			return;
		}
		for (const FPatternStep& Step : Pattern->Steps)
		{
			if (!Step.Montage.IsNull())
			{
				Paths.AddUnique(Step.Montage.ToSoftObjectPath());
			}
		}
	};

	CollectFromPattern(Phase->TransitionGimmick);
	for (const FBossWeightedPattern& Weighted : Phase->Patterns)
	{
		CollectFromPattern(Weighted.Pattern);
	}

	// 이전 페이즈 핸들 해제 -> 이전 페이즈에서만 쓰던 몽타주는 GC 대상이 되어 메모리 반환
	if (PhasePreloadHandle.IsValid())
	{
		PhasePreloadHandle->ReleaseHandle();
		PhasePreloadHandle.Reset();
	}

	if (Paths.Num() > 0)
	{
		PhasePreloadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(MoveTemp(Paths));
	}
}

void UBossPatternComponent::OnPatternAbilityFinished()
{
	// 0) 전투 정지(사망 등) 후엔 다음 패턴을 돌리지 않는다
	//    (사망 시 CancelAllAbilities 가 이 콜백을 부르며 들어오는 경로)
	if (bCombatStopped)
	{
		return;
	}

	// 1) 방금 끝난 게 페이즈 전환 기믹이면 -> 새 페이즈의 일반 패턴 시작
	if (bRunningTransition)
	{
		bRunningTransition = false;
		RunNextPattern();
		return;
	}

	// 2) 예약된 페이즈 전환이 있으면 (현재 패턴을 다 실행한 지금) 전환
	if (PendingPhaseIndex > CurrentPhaseIndex && Phases.IsValidIndex(PendingPhaseIndex))
	{
		const int32 Target = PendingPhaseIndex;
		PendingPhaseIndex = INDEX_NONE;
		EnterPhase(Target);
		return;
	}

	// 3) 같은 페이즈에서 다음 패턴을 가중치로 선택
	RunNextPattern();
}
