// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Raid/BossReviveComponent.h"
#include "Boss/BossGameplayTags.h"
#include "Boss/Combat/BossCombatStatics.h"
#include "Boss/Raid/BossRaidGameState.h"
#include "Boss/BossBase.h"
#include "Core/LostArkCombatInterface.h"
#include "Character/LostArkAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameStateBase.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "GameFramework/PlayerStart.h"
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

void UBossReviveComponent::HandleDeadTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	const bool bNowDead = (NewCount > 0);
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

	// ★ 1단계: 콜리전이나 부활 처리를 하기 전에, 우선 안전한 땅 위로 텔레포트(피신)부터 수행합니다!
	// (낙사 바닥에서 먼저 Revive를 켜면 낙사 존에 즉시 또 닿아 체력 0이 되고 다시 낙사하기 때문입니다)
	if (bReviveAtArenaCenter)
	{
		FVector ReviveLoc = FVector::ZeroVector;
		bool bFoundValidSlice = false;

		// 보스 찾기 (거리 계산 및 폴드백용)
		ABossBase* Boss = nullptr;
		for (TActorIterator<ABossBase> It(GetWorld()); It; ++It)
		{
			Boss = *It;
			break;
		}

		if (const ABossRaidGameState* GS = GetWorld()->GetGameState<ABossRaidGameState>())
		{
			ReviveLoc = GS->ArenaCenter;
			if (GS->ArenaFloorZ != 0.f)
			{
				ReviveLoc.Z = GS->ArenaFloorZ;
			}

			// 살아있는 (파괴되지 않은) 유효 슬라이스 쿼리
			TArray<int32> SafeSlices;
			for (int32 i = 0; i < GS->SliceCount; ++i)
			{
				if (!GS->IsSliceDestroyed(i))
				{
					SafeSlices.Add(i);
				}
			}

			if (SafeSlices.Num() > 0)
			{
				int32 BestSlice = SafeSlices[0];
				if (Boss)
				{
					float MaxDistSq = -1.f;
					const FVector BossLoc = Boss->GetActorLocation();
					for (int32 SliceIdx : SafeSlices)
					{
						FVector SliceCenter = GS->GetSliceCenterLocation(SliceIdx, 500.f);
						float DistSq = FVector::DistSquared2D(SliceCenter, BossLoc);
						if (DistSq > MaxDistSq)
						{
							MaxDistSq = DistSq;
							BestSlice = SliceIdx;
						}
					}
				}
				ReviveLoc = GS->GetSliceCenterLocation(BestSlice, 500.f);
				bFoundValidSlice = true;
			}
		}

		// 바닥 충돌 센싱 (LineTrace로 실제 대지 높이 탐지하여 바닥 뚫림 방지)
		FHitResult HitResult;
		const FVector TraceStart = ReviveLoc + FVector(0.f, 0.f, 3000.f);
		const FVector TraceEnd = ReviveLoc - FVector(0.f, 0.f, 3000.f);
		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(Owner);

		bool bHitGround = GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_WorldStatic, QueryParams);

		// ★ 만약 바닥 탐지에 실패했거나 슬라이스가 없다면 (낙사 구역, 구덩이 또는 허공), 무조건 안전한 장소로 대체!
		if (!bHitGround || !bFoundValidSlice)
		{
			// 1순위: 보스가 서 있는 안전한 바닥 근처
			if (Boss)
			{
				ReviveLoc = Boss->GetActorLocation() + FVector(200.f, 0.f, 150.f);
				bHitGround = GetWorld()->LineTraceSingleByChannel(HitResult, ReviveLoc + FVector(0.f,0.f,1000.f), ReviveLoc - FVector(0.f,0.f,2000.f), ECC_WorldStatic, QueryParams);
			}
			// 2순위: 맵에 설정된 PlayerStart (최초 플레이어 스폰 지역)
			if (!bHitGround)
			{
				for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
				{
					ReviveLoc = (*It)->GetActorLocation() + FVector(0.f, 0.f, 100.f);
					bHitGround = GetWorld()->LineTraceSingleByChannel(HitResult, ReviveLoc + FVector(0.f,0.f,1000.f), ReviveLoc - FVector(0.f,0.f,2000.f), ECC_WorldStatic, QueryParams);
					break;
				}
			}
		}

		if (bHitGround)
		{
			ReviveLoc.Z = HitResult.ImpactPoint.Z;
		}

		float CapsuleHalfHeight = 88.f;
		if (const ACharacter* Character = Cast<ACharacter>(Owner))
		{
			if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
			{
				CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
			}
		}

		// 안전하게 바닥 위 150cm 상공에 스폰
		ReviveLoc.Z += CapsuleHalfHeight + 150.f;

		Owner->TeleportTo(ReviveLoc, Owner->GetActorRotation());
	}

	// ★ 2단계: 사망 태그를 떼고 무적 태그를 부여 (외부 트리거/낙사 판정으로부터 완벽 보호)
	UAbilitySystemComponent* ASC = GetOwnerASC();
	if (ASC)
	{
		UBossCombatStatics::RemoveReplicatedLooseTag(ASC, LostArkTags::State_Dead);

		FGameplayTag InvincibleTag = FGameplayTag::RequestGameplayTag(FName("State.Invincible"), false);
		if (InvincibleTag.IsValid())
		{
			UBossCombatStatics::AddReplicatedLooseTag(ASC, InvincibleTag);
			if (ReviveInvincibleDuration > 0.f)
			{
				GetWorld()->GetTimerManager().SetTimer(
					ReviveInvincibleTimer, this, &UBossReviveComponent::RemoveReviveInvincibility, ReviveInvincibleDuration, false);
			}
		}

		// ★ 3단계: 체력 회복 (ApplyModToAttribute 및 AttributeSet 직접 변경으로 HUD 즉시 방송)
		const float MaxHealth = ASC->GetNumericAttribute(ULostArkAttributeSet::GetMaxHealthAttribute());
		if (MaxHealth > 0.f)
		{
			const float TargetHealth = MaxHealth * ReviveHealthPercent;
			ASC->SetNumericAttributeBase(ULostArkAttributeSet::GetHealthAttribute(), TargetHealth);
			ASC->ApplyModToAttribute(ULostArkAttributeSet::GetHealthAttribute(), EGameplayModOp::Override, TargetHealth);
			if (ULostArkAttributeSet* AttributeSet = const_cast<ULostArkAttributeSet*>(ASC->GetSet<ULostArkAttributeSet>()))
			{
				AttributeSet->SetHealth(TargetHealth);
			}
		}
	}

	// ★ 4단계: 가장 마지막에 콜리전을 활성화(Revive)하고 MOVE_Falling으로 공중에서 안전하게 발을 딛게 함
	if (ILostArkCombatInterface* Combat = Cast<ILostArkCombatInterface>(Owner))
	{
		Combat->Revive();
	}

	// HandleDeadTagChanged(태그 제거)가 이미 상태를 내렸겠지만, ASC 미획득 등 예외 경로 방어
	if (bDeadState)
	{
		bDeadState = false;
		OnReviveStateChanged.Broadcast(false);
	}
}

void UBossReviveComponent::RemoveReviveInvincibility()
{
	if (UAbilitySystemComponent* ASC = GetOwnerASC())
	{
		FGameplayTag InvincibleTag = FGameplayTag::RequestGameplayTag(FName("State.Invincible"), false);
		if (InvincibleTag.IsValid())
		{
			UBossCombatStatics::RemoveReplicatedLooseTag(ASC, InvincibleTag);
		}
	}
}

void UBossReviveComponent::OnRep_DeadState()
{
	OnReviveStateChanged.Broadcast(bDeadState);
}
