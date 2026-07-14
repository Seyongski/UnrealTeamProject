// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Combat/BossCounterComponent.h"
#include "Boss/Combat/BossCombatStatics.h"
#include "Boss/Combat/BossGroggyEffect.h"
#include "Boss/BossGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"

namespace
{
	FGameplayTag GetWindowTag(EBossCounterType Type)
	{
		switch (Type)
		{
		case EBossCounterType::MultiCounter:	return LostArkTags::State_Boss_Counterable_Multi.GetTag();
		case EBossCounterType::FakeCounter:		return LostArkTags::State_Boss_Counterable_Fake.GetTag();
		default:								return LostArkTags::State_Boss_Counterable_Normal.GetTag();
		}
	}
}

UBossCounterComponent::UBossCounterComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	GroggyEffectClass = UBossGroggyEffect::StaticClass();
}

void UBossCounterComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	// 플레이어 스킬이 보내는 카운터 적중 이벤트 구독 (판정은 전부 이쪽에서)
	if (UAbilitySystemComponent* ASC = GetASC())
	{
		ASC->GenericGameplayEventCallbacks.FindOrAdd(LostArkTags::Event_Boss_CounterHit.GetTag())
			.AddUObject(this, &UBossCounterComponent::HandleCounterEvent);
	}
}

UAbilitySystemComponent* UBossCounterComponent::GetASC() const
{
	return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
}

void UBossCounterComponent::OpenWindow(EBossCounterType Type, int32 InRequiredPlayers)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetASC();
	if (!ASC)
	{
		return;
	}

	// 기믹 실패 확정 후에는 같은 패턴의 남은 카운터 창을 전부 무시
	// (가짜 카운터를 친 뒤 재생되는 몽타주들에 박힌 창이 여기서 자동으로 죽는다)
	if (ASC->HasMatchingGameplayTag(LostArkTags::State_Boss_PatternResult_CounterFailed.GetTag()))
	{
		return;
	}

	if (bWindowOpen)
	{
		CloseWindow();	// 겹침 방어 (몽타주 전환 타이밍에 End 가 늦는 경우)
	}

	bWindowOpen = true;
	WindowType = Type;
	RequiredPlayers = FMath::Max(1, InRequiredPlayers);
	UniqueAttackers.Reset();

	ASC->AddLooseGameplayTag(GetWindowTag(Type));
}

void UBossCounterComponent::CloseWindow()
{
	if (!bWindowOpen)
	{
		return;
	}
	bWindowOpen = false;
	UniqueAttackers.Reset();

	if (UAbilitySystemComponent* ASC = GetASC())
	{
		ASC->RemoveLooseGameplayTag(GetWindowTag(WindowType));

		// FakeCountered 는 '이 창에서 침' 즉시 분기용 -> 창이 닫히면 무효.
		// (앞 몽타주의 가짜 히트가 뒤 스텝 분기를 오발시키지 않게 여기서 정리)
		ASC->SetLooseGameplayTagCount(LostArkTags::State_Boss_PatternResult_FakeCountered.GetTag(), 0);
	}
}

void UBossCounterComponent::HandleCounterEvent(const FGameplayEventData* Payload)
{
	// 이벤트 페이로드의 Instigator = 카운터 스킬을 쓴 플레이어
	AActor* Attacker = Payload ? const_cast<AActor*>(Payload->Instigator.Get()) : nullptr;
	NotifyCounterAttackHit(Attacker);
}

void UBossCounterComponent::NotifyCounterAttackHit(AActor* Attacker, bool bBypassHeadZone)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || !bWindowOpen || !Attacker)
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetASC();
	if (!ASC)
	{
		return;
	}

	// (중요) 카운터는 약점포착 상태여도 '실제 헤드 존' 적중만 인정 (순수 지오메트리 판정)
	// bBypassHeadZone 은 임시 디버그 강제 카운터 전용 (프로덕션 경로는 항상 false)
	if (!bBypassHeadZone && !UBossCombatStatics::IsHeadZoneHit(Owner, Attacker->GetActorLocation()))
	{
		return;
	}

	switch (WindowType)
	{
	case EBossCounterType::FakeCounter:
		// 치면 안 되는 창: 기믹 실패 확정. 몽타주를 끊을지는 데이터(Branch)가 결정한다.
		// 트리거 태그(FakeCountered)를 붙이는 순간 분기가 동기적으로 실행되어 창이 닫힐 수
		// 있으므로, 지속 태그(CounterFailed)를 먼저 세우고 트리거는 반드시 마지막에.
		if (!ASC->HasMatchingGameplayTag(LostArkTags::State_Boss_PatternResult_FakeCountered.GetTag()))
		{
			ASC->AddLooseGameplayTag(LostArkTags::State_Boss_PatternResult_CounterFailed.GetTag());
			ASC->AddLooseGameplayTag(LostArkTags::State_Boss_PatternResult_FakeCountered.GetTag());
		}
		break;

	case EBossCounterType::Counter:
		ApplyCounterSuccess();
		break;

	case EBossCounterType::MultiCounter:
		UniqueAttackers.Add(Attacker);
		if (UniqueAttackers.Num() >= RequiredPlayers)
		{
			ApplyCounterSuccess();
		}
		break;
	}
}

void UBossCounterComponent::ApplyCounterSuccess()
{
	UAbilitySystemComponent* ASC = GetASC();
	if (!ASC || ASC->HasMatchingGameplayTag(LostArkTags::State_Boss_PatternResult_Countered.GetTag()))
	{
		return;
	}

	// 그로기 GE 를 먼저 적용한다: Countered 태그가 붙는 순간 분기가 동기적으로 그로기 스텝을
	// 실행할 수 있는데, 그때 이미 State.Boss.Groggy 가 서 있어야 루프 스텝의
	// 'NOT Groggy' 종료 분기가 첫 평가에서 오발하지 않는다
	if (GroggyEffectClass)
	{
		FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(GroggyEffectClass, 1.f, ASC->MakeEffectContext());
		if (Spec.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(LostArkTags::Data_Duration.GetTag(), FMath::Max(0.1f, GroggyDuration));
			ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
		}
	}

	// 분기 트리거는 마지막 (이 줄 아래에 창 상태를 만지는 코드를 두지 말 것)
	ASC->AddLooseGameplayTag(LostArkTags::State_Boss_PatternResult_Countered.GetTag());
}
