// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Combat/BossJustGuardComponent.h"
#include "Boss/Combat/BossCombatStatics.h"
#include "Boss/BossGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"

UBossJustGuardComponent::UBossJustGuardComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UBossJustGuardComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	// 플레이어 가드 입력 이벤트 구독 (판정은 전부 이쪽에서)
	if (UAbilitySystemComponent* ASC = GetASC())
	{
		ASC->GenericGameplayEventCallbacks.FindOrAdd(LostArkTags::Event_Boss_JustGuardInput.GetTag())
			.AddUObject(this, &UBossJustGuardComponent::HandleGuardEvent);
	}
}

UAbilitySystemComponent* UBossJustGuardComponent::GetASC() const
{
	return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
}

UAbilitySystemComponent* UBossJustGuardComponent::GetPlayerASC(AActor* Player)
{
	return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Player);
}

void UBossJustGuardComponent::OpenWindow()
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	if (bWindowOpen)
	{
		CloseWindow();	// 겹침 방어 (몽타주 전환 타이밍에 End 가 늦는 경우)
	}

	bWindowOpen = true;
	GuardInputs.Reset();
	ReadyPlayers.Reset();

	// 전 플레이어에 '1회 가드 가능' 부여. 복제 루스로 클라(소유자)까지 전파 -> G 어빌리티 게이트/UI
	// (전용 대상이 지정돼 있으면 그 1명에게만 — 기믹 대상 전용 저스트가드)
	TArray<APawn*> PlayerPawns;
	UBossCombatStatics::GetPlayerPawns(GetWorld(), PlayerPawns);
	for (APawn* Pawn : PlayerPawns)
	{
		if (Pawn == Owner)
		{
			continue;
		}
		if (ExclusiveGuardPlayer.IsValid() && Pawn != ExclusiveGuardPlayer.Get())
		{
			continue;
		}
		if (UAbilitySystemComponent* PlayerASC = GetPlayerASC(Pawn))
		{
			UBossCombatStatics::AddReplicatedLooseTag(PlayerASC, LostArkTags::State_Player_GuardReady.GetTag());
			ReadyPlayers.Add(Pawn);
		}
	}

	// 보스 글로우 큐 (BP 연출은 이 태그 또는 델리게이트에 바인딩)
	if (UAbilitySystemComponent* ASC = GetASC())
	{
		ASC->AddLooseGameplayTag(LostArkTags::State_Boss_JustGuardable.GetTag());
	}

	OnJustGuardWindowChanged.Broadcast(true);
}

void UBossJustGuardComponent::CloseWindow()
{
	if (!bWindowOpen)
	{
		return;
	}
	bWindowOpen = false;

	// 남은 GuardReady 회수 (이미 소모한 플레이어는 셋에서 빠져 있음)
	for (const TWeakObjectPtr<AActor>& Weak : ReadyPlayers)
	{
		if (AActor* Player = Weak.Get())
		{
			UBossCombatStatics::RemoveReplicatedLooseTag(
				GetPlayerASC(Player), LostArkTags::State_Player_GuardReady.GetTag());
		}
	}
	ReadyPlayers.Reset();

	if (UAbilitySystemComponent* ASC = GetASC())
	{
		ASC->RemoveLooseGameplayTag(LostArkTags::State_Boss_JustGuardable.GetTag());
	}

	OnJustGuardWindowChanged.Broadcast(false);
}

void UBossJustGuardComponent::NotifyGuardInput(AActor* Player)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || !bWindowOpen || !Player)
	{
		return;
	}

	// GuardReady 를 아직 가진 플레이어만 유효 (패턴당 1회 게이트)
	if (!HasGuardReady(Player))
	{
		return;
	}

	UWorld* World = GetWorld();

	FJustGuardInput Input;
	Input.PressTime = World ? World->GetTimeSeconds() : 0.f;
	Input.Location = Player->GetActorLocation();
	Input.Facing = Player->GetActorForwardVector();	// 가드 GA 가 커서 방향으로 회전 후 이벤트를 보냄
	Input.Facing.Z = 0.f;
	Input.Facing.Normalize();
	GuardInputs.Add(Player, Input);

	// 가드 소모: GuardReady 제거 -> 이 패턴에서 다시 못 누른다
	UBossCombatStatics::RemoveReplicatedLooseTag(
		GetPlayerASC(Player), LostArkTags::State_Player_GuardReady.GetTag());
	ReadyPlayers.Remove(Player);
}

EJustGuardResult UBossJustGuardComponent::ResolveGuard(AActor* Player, const FJustGuardResolveParams& Params) const
{
	const FJustGuardInput* Input = GuardInputs.Find(Player);
	if (!Input || Input->PressTime < 0.f)
	{
		return EJustGuardResult::FailNoInput;
	}

	// 타이밍: 판정 시각 J = T + Delay. 성공 윈도우는 J 에서 끝나고 앞으로 Duration 만큼 = [J - Duration, J].
	// (Delay=0 이면 [T - Duration, T] -> 다 차기 직전~다 차는 순간에 눌러야 성공.
	//  Delay>0 이면 윈도우가 통째로 뒤로 밀려 '장판이 사라진 뒤 엇박에 눌러야' 성공)
	const float JudgmentTime = Params.HitTime + FMath::Max(Params.JudgmentDelay, 0.f);
	const float WindowStart = JudgmentTime - FMath::Max(Params.GuardWindowDuration, 0.f);
	if (Input->PressTime < WindowStart || Input->PressTime > JudgmentTime)
	{
		return EJustGuardResult::FailTiming;
	}

	// 방향: 플레이어 정면 vs (플레이어->장판중심)
	if (!Params.bBypassDirection)
	{
		FVector ToCenter = Params.GuardCenter - Input->Location;
		ToCenter.Z = 0.f;
		if (ToCenter.Normalize())
		{
			const float Dot = FVector::DotProduct(Input->Facing, ToCenter);
			const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Dot, -1.f, 1.f)));
			if (AngleDeg > FMath::Max(Params.GuardAngleTolerance, 0.f))
			{
				return EJustGuardResult::FailDirection;
			}
		}
	}

	return EJustGuardResult::Success;
}

void UBossJustGuardComponent::MarkJustGuardedResult()
{
	if (UAbilitySystemComponent* ASC = GetASC())
	{
		if (!ASC->HasMatchingGameplayTag(LostArkTags::State_Boss_PatternResult_JustGuarded.GetTag()))
		{
			ASC->AddLooseGameplayTag(LostArkTags::State_Boss_PatternResult_JustGuarded.GetTag());
		}
	}
}

bool UBossJustGuardComponent::HasGuardReady(AActor* Player) const
{
	if (const UAbilitySystemComponent* PlayerASC = GetPlayerASC(Player))
	{
		return PlayerASC->HasMatchingGameplayTag(LostArkTags::State_Player_GuardReady.GetTag());
	}
	return false;
}

void UBossJustGuardComponent::HandleGuardEvent(const FGameplayEventData* Payload)
{
	AActor* Player = Payload ? const_cast<AActor*>(Payload->Instigator.Get()) : nullptr;
	NotifyGuardInput(Player);
}
