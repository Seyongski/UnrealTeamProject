// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Combat/BossJustGuardComponent.h"
#include "Boss/Combat/BossCombatStatics.h"
#include "Boss/Combat/BossGroggyEffect.h"
#include "Boss/BossGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Engine/Engine.h"	// [임시 디버그] 화면 메시지

UBossJustGuardComponent::UBossJustGuardComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	GroggyEffectClass = UBossGroggyEffect::StaticClass();	// 카운터 성공 경로와 동일한 기본 그로기 GE
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

	// 실패 게이트: 이번 기믹에서 이미 실패 확정(JustGuardFailed)이면 이후 창은 열지 않는다.
	// -> 한 몽타주에 저스트가드가 여러 개여도, 앞에서 실패하면 뒤 창은 자동으로 죽는다.
	// (카운터의 CounterFailed 게이트와 동일 구조)
	if (const UAbilitySystemComponent* ASC = GetASC())
	{
		if (ASC->HasMatchingGameplayTag(LostArkTags::State_Boss_PatternResult_JustGuardFailed.GetTag()))
		{
			return;
		}
	}

	if (bWindowOpen)
	{
		CloseWindow();	// 겹침 방어 (몽타주 전환 타이밍에 End 가 늦는 경우)
	}

	bWindowOpen = true;
	GuardInputs.Reset();
	ReadyPlayers.Reset();

	// 연속 저스트가드(2-3 -> 2-4) 지원: 직전 창의 성공 결과(JustGuarded)를 창마다 리셋한다.
	// JustGuarded 는 패턴 종료까지 남으므로, 리셋하지 않으면 이번 창이 이 스텝의 성공 분기(예: 2-4 성공 -> 그로기)를
	// 입력도 받기 전에 오발시킨다. 실패 게이트(JustGuardFailed)는 그대로 둔다(위에서 이미 이 창을 막았음).
	// (JustGuardable 을 붙이기 전에 지워 태그 변화 감시가 stale 값으로 분기를 평가하지 않게 한다)
	if (UAbilitySystemComponent* ResetASC = GetASC())
	{
		ResetASC->SetLooseGameplayTagCount(LostArkTags::State_Boss_PatternResult_JustGuarded.GetTag(), 0);
	}

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

	// [임시 디버그] 창이 열린 순간을 화면에 표시 (G 를 이때 눌러야 함) — 확인 후 삭제
	if (GEngine)
	{
		const int32 NumReady = ReadyPlayers.Num();
		GEngine->AddOnScreenDebugMessage(/*Key*/1001, 3.f, FColor::Yellow,
			FString::Printf(TEXT("[저스트가드] 창 열림 — G 누르세요 (가드가능 %d명)"), NumReady));
	}
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

	// [임시 디버그] 입력이 기록된 순간 (성공/실패 판정은 장판이 판정 시각에 별도 표시) — 확인 후 삭제
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(/*Key*/1002, 3.f, FColor::Cyan,
			FString::Printf(TEXT("[저스트가드] 입력 기록 t=%.2f (판정 대기)"), Input.PressTime));
	}
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

void UBossJustGuardComponent::MarkJustGuardedResult(bool bApplyGroggy)
{
	UAbilitySystemComponent* ASC = GetASC();
	if (!ASC || ASC->HasMatchingGameplayTag(LostArkTags::State_Boss_PatternResult_JustGuarded.GetTag()))
	{
		return;
	}

	// 최종 저스트가드 성공 -> 그로기: 그로기 GE 를 '먼저' 적용한다. JustGuarded 태그가 붙는 순간 분기가
	// 동기적으로 그로기 스텝을 실행할 수 있는데, 그때 이미 State.Boss.Groggy 가 서 있어야
	// 루프 스텝의 'NOT Groggy' 종료 분기가 첫 평가에서 오발하지 않는다 (카운터 성공 경로와 동일).
	if (bApplyGroggy && GroggyEffectClass)
	{
		UBossCombatStatics::ApplyEffectToSelf(ASC, GroggyEffectClass, this,
			LostArkTags::Data_Duration.GetTag(), FMath::Max(0.1f, GroggyDuration));
	}

	// 분기 트리거는 마지막 (이 줄 아래에 상태를 만지는 코드를 두지 말 것)
	ASC->AddLooseGameplayTag(LostArkTags::State_Boss_PatternResult_JustGuarded.GetTag());
}

void UBossJustGuardComponent::MarkJustGuardFailedResult()
{
	if (UAbilitySystemComponent* ASC = GetASC())
	{
		if (!ASC->HasMatchingGameplayTag(LostArkTags::State_Boss_PatternResult_JustGuardFailed.GetTag()))
		{
			ASC->AddLooseGameplayTag(LostArkTags::State_Boss_PatternResult_JustGuardFailed.GetTag());
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
