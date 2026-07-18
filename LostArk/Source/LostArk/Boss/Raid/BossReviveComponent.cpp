// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Raid/BossReviveComponent.h"
#include "Boss/BossGameplayTags.h"
#include "Boss/Raid/BossRaidGameState.h"
#include "Core/LostArkCombatInterface.h"
#include "Character/LostArkAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameStateBase.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"

UBossReviveComponent::UBossReviveComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);	// 런타임 부착이라도 사망 상태/시각 복제가 동작하게
}

void UBossReviveComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UBossReviveComponent, bDeadState);
	DOREPLIFETIME(UBossReviveComponent, DeathServerTime);
}

void UBossReviveComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	// 사망 감지: 체력 0(캐릭터 Die())과 낙사(ArenaKillVolume) 모두 State.Dead 를 세우므로 한 곳에서 잡힌다
	if (UAbilitySystemComponent* ASC = GetOwnerASC())
	{
		ASC->RegisterGameplayTagEvent(LostArkTags::State_Dead, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &UBossReviveComponent::HandleDeadTagChanged);

		// 부착 시점에 이미 죽어 있으면 즉시 반영
		if (ASC->HasMatchingGameplayTag(LostArkTags::State_Dead))
		{
			HandleDeadTagChanged(LostArkTags::State_Dead, 1);
		}
	}
}

UAbilitySystemComponent* UBossReviveComponent::GetOwnerASC() const
{
	return GetOwner() ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()) : nullptr;
}

float UBossReviveComponent::GetServerNow() const
{
	// GameState 의 동기화된 서버 시계 -> 클라에서도 같은 기준으로 카운트다운 계산 가능
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	return GS ? GS->GetServerWorldTimeSeconds() : 0.f;
}

float UBossReviveComponent::GetAutoReviveRemaining() const
{
	if (!bDeadState)
	{
		return 0.f;
	}
	return FMath::Max(0.f, (DeathServerTime + AutoReviveDelay) - GetServerNow());
}

float UBossReviveComponent::GetManualReviveRemaining() const
{
	if (!bDeadState)
	{
		return 0.f;
	}
	return FMath::Max(0.f, (DeathServerTime + ManualReviveDelay) - GetServerNow());
}

void UBossReviveComponent::HandleDeadTagChanged(const FGameplayTag /*Tag*/, int32 NewCount)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	const bool bNowDead = NewCount > 0;
	if (bNowDead == bDeadState)
	{
		return;
	}
	bDeadState = bNowDead;
	OnReviveStateChanged.Broadcast(bDeadState);	// 클라는 OnRep 에서 방송

	if (bNowDead)
	{
		DeathServerTime = GetServerNow();
		GetWorld()->GetTimerManager().SetTimer(
			AutoReviveTimer, this, &UBossReviveComponent::DoRevive, AutoReviveDelay, false);
	}
	else
	{
		// 외부 시스템이 먼저 살렸으면 자동 부활 취소
		GetWorld()->GetTimerManager().ClearTimer(AutoReviveTimer);
	}
}

void UBossReviveComponent::RequestManualRevive()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	if (!Owner->HasAuthority())
	{
		ServerRequestManualRevive();	// 클라(사망 화면 버튼) -> 서버
		return;
	}
	ServerRequestManualRevive_Implementation();
}

void UBossReviveComponent::ServerRequestManualRevive_Implementation()
{
	// 수동 부활은 개발자 지정 시간 경과 후에만 (조작/랙으로 이른 요청이 와도 서버가 걸러냄)
	if (!bDeadState || GetManualReviveRemaining() > 0.f)
	{
		return;
	}
	DoRevive();
}

void UBossReviveComponent::DoRevive()
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || !bDeadState)
	{
		return;
	}

	GetWorld()->GetTimerManager().ClearTimer(AutoReviveTimer);

	// 1) 사망 태그 제거 (캐릭터 Die() 는 로컬만, 낙사 볼륨은 복제까지 세우므로 양쪽 다 회수)
	if (UAbilitySystemComponent* ASC = GetOwnerASC())
	{
		ASC->RemoveLooseGameplayTag(LostArkTags::State_Dead);
		ASC->RemoveReplicatedLooseGameplayTag(LostArkTags::State_Dead);

		// 2) 체력 회복
		const float MaxHealth = ASC->GetNumericAttribute(ULostArkAttributeSet::GetMaxHealthAttribute());
		if (MaxHealth > 0.f)
		{
			ASC->SetNumericAttributeBase(
				ULostArkAttributeSet::GetHealthAttribute(), MaxHealth * ReviveHealthPercent);
		}
	}

	// 3) 캐릭터 상태 복구 (bIsDead/콜리전/이동 — 플레이어 쪽 Revive 구현이 담당)
	if (ILostArkCombatInterface* Combat = Cast<ILostArkCombatInterface>(Owner))
	{
		Combat->Revive();
	}

	// 4) 낙사 대비 아레나 중심으로 복귀 (바닥 높이는 GameState 의 ArenaFloorZ 사용)
	if (bReviveAtArenaCenter)
	{
		if (const ABossRaidGameState* GS = GetWorld()->GetGameState<ABossRaidGameState>())
		{
			FVector ReviveLoc = GS->ArenaCenter;
			if (GS->ArenaFloorZ != 0.f)
			{
				ReviveLoc.Z = GS->ArenaFloorZ;
			}
			if (const ACharacter* Character = Cast<ACharacter>(Owner))
			{
				if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
				{
					ReviveLoc.Z += Capsule->GetScaledCapsuleHalfHeight() + 5.f;
				}
			}
			Owner->TeleportTo(ReviveLoc, Owner->GetActorRotation());
		}
	}

	// HandleDeadTagChanged(태그 제거)가 이미 상태를 내렸겠지만, ASC 미획득 등 예외 경로 방어
	if (bDeadState)
	{
		bDeadState = false;
		OnReviveStateChanged.Broadcast(false);
	}
}

void UBossReviveComponent::OnRep_DeadState()
{
	OnReviveStateChanged.Broadcast(bDeadState);
}
