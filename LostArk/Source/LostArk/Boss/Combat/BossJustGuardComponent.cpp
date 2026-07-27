// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Combat/BossJustGuardComponent.h"
#include "Boss/Combat/BossCombatStatics.h"
#include "Boss/Combat/BossGroggyEffect.h"
#include "Boss/Targeting/BossTargetingComponent.h"
#include "Boss/BossGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Engine/Engine.h"	// [임시 디버그] 화면 메시지
#include "TimerManager.h"

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

void UBossJustGuardComponent::OpenWindow(float InGuardStateDuration)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	// 이 창에서 G 를 눌렀을 때의 가드 상태 유지시간 (판정 노티파이가 이 값을 사용)
	GuardStateDuration = FMath::Max(0.f, InGuardStateDuration);

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
	// 전 플레이어에 '1회 가드 가능' 부여. 복제 루스로 클라(소유자)까지 전파 -> G 어빌리티 게이트/UI
	// (전용 대상이 지정돼 있고 유효할 때만 1명으로 제한 — 유효하지 않으면 전원 허용으로 폴백)
	TArray<APawn*> PlayerPawns;
	UBossCombatStatics::GetPlayerPawns(GetWorld(), PlayerPawns);

	bool bHasExclusiveTarget = ExclusiveGuardPlayer.IsValid();
	if (bHasExclusiveTarget)
	{
		bool bFoundValidPlayer = false;
		for (APawn* Pawn : PlayerPawns)
		{
			if (Pawn && Pawn == ExclusiveGuardPlayer.Get())
			{
				bFoundValidPlayer = true;
				break;
			}
		}
		if (!bFoundValidPlayer)
		{
			bHasExclusiveTarget = false; // 타깃을 못 찾으면 전원 허용 폴백
		}
	}

	for (APawn* Pawn : PlayerPawns)
	{
		if (Pawn == Owner)
		{
			continue;
		}
		if (bHasExclusiveTarget && Pawn != ExclusiveGuardPlayer.Get())
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

	// [디버그] 창이 열린 순간을 화면과 로그에 명확히 표시
	if (GEngine)
	{
		const int32 NumReady = ReadyPlayers.Num();
		GEngine->AddOnScreenDebugMessage(1001, 3.5f, FColor::Yellow,
			FString::Printf(TEXT("[저스트가드 창 열림!] G를 누르세요 (가드가능: %d명)"), NumReady));
		UE_LOG(LogTemp, Warning, TEXT("[JustGuard] OpenWindow Called - Ready Players: %d"), NumReady);
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

	UWorld* World = GetWorld();

	FJustGuardInput Input;
	Input.PressTime = World ? World->GetTimeSeconds() : 0.f;
	Input.Location = Player->GetActorLocation();
	Input.Facing = Player->GetActorForwardVector();	// 가드 GA 가 커서 방향으로 회전 후 이벤트를 보냄
	Input.Facing.Z = 0.f;
	Input.Facing.Normalize();
	GuardInputs.Add(Player, Input);

	// [디버그] 입력이 기록된 순간 화면에 선명하게 표시 (광클 시 최신 시각 갱신)
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(1002, 3.f, FColor::Cyan,
			FString::Printf(TEXT("[저스트가드 G키 수신] 입력시각: %.2f초 (최신 시각 갱신 완료)"), Input.PressTime));
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

EJustGuardResult UBossJustGuardComponent::JudgeGuardAtAttack(float GuardAngleTolerance,
	bool bBypassDirection, TSubclassOf<UGameplayEffect> DamageEffect,
	float DamageCoefficient, bool bGroggyOnSuccess)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return EJustGuardResult::FailNoInput;
	}

	// 판정 대상: 기믹 전용 가드 플레이어 우선, 없으면 현재 타겟
	AActor* Target = ExclusiveGuardPlayer.Get();
	if (!Target)
	{
		if (const UBossTargetingComponent* Targeting = Owner->FindComponentByClass<UBossTargetingComponent>())
		{
			Target = Targeting->GetCurrentTarget();
		}
	}

	// 판정 시각 J = 지금(노티파이가 부른 순간 = 보스 공격 순간). 성공 창은 [J - GuardWindowDuration, J].
	const UWorld* World = GetWorld();
	FJustGuardResolveParams Params;
	Params.HitTime = World ? World->GetTimeSeconds() : 0.f;
	Params.JudgmentDelay = 0.f;
	Params.GuardWindowDuration = GuardStateDuration;	// 창(Just Guard Window)이 정한 가드 상태 유지시간
	Params.GuardAngleTolerance = GuardAngleTolerance;
	Params.GuardCenter = Owner->GetActorLocation();	// 대상은 보스를 바라봐야 방향 성공
	Params.bBypassDirection = bBypassDirection;

	const EJustGuardResult Result = Target ? ResolveGuard(Target, Params) : EJustGuardResult::FailNoInput;

	if (Result == EJustGuardResult::Success)
	{
		MarkJustGuardedResult(bGroggyOnSuccess);	// 분기용(+옵션 그로기). 무피해
	}
	else
	{
		// 실패: 대상에게 데미지 GE 를 직접 적용 (장판 없음)
		if (Target && DamageEffect)
		{
			if (UAbilitySystemComponent* TargetASC = GetPlayerASC(Target))
			{
				UBossCombatStatics::ApplyEffect(GetASC(), TargetASC, DamageEffect, this, Owner, Owner,
					LostArkTags::Data_Damage.GetTag(), DamageCoefficient);
			}
		}
		MarkJustGuardFailedResult();	// 43(부수기) 분기 + 남은 창 게이트
	}

	// [디버그] 판정 결과 화면 및 로그 구체적 출력
	if (GEngine)
	{
		const bool bOK = (Result == EJustGuardResult::Success);
		FString ReasonMsg;
		const FJustGuardInput* Input = Target ? GuardInputs.Find(Target) : nullptr;
		float PressT = Input ? Input->PressTime : -1.f;
		float JudgmentT = Params.HitTime + FMath::Max(Params.JudgmentDelay, 0.f);
		float WindowStartT = JudgmentT - FMath::Max(Params.GuardWindowDuration, 0.f);

		if (Result == EJustGuardResult::Success)
		{
			ReasonMsg = FString::Printf(TEXT("저스트가드 성공!! (입력시각: %.2fs, 판정시각: %.2fs)"), PressT, JudgmentT);
		}
		else if (Result == EJustGuardResult::FailTiming)
		{
			if (PressT < WindowStartT)
			{
				ReasonMsg = FString::Printf(TEXT("저스트가드 실패 (너무 일찍 누름! 입력: %.2fs, 윈도우시작: %.2fs, %.2fs 조기)"), PressT, WindowStartT, WindowStartT - PressT);
			}
			else
			{
				ReasonMsg = FString::Printf(TEXT("저스트가드 실패 (너무 늦게 누름! 입력: %.2fs, 판정시각: %.2fs, %.2fs 지연)"), PressT, JudgmentT, PressT - JudgmentT);
			}
		}
		else if (Result == EJustGuardResult::FailDirection)
		{
			ReasonMsg = FString::Printf(TEXT("저스트가드 실패 (방향 틀림 — 보스를 바라봐야 함!)"));
		}
		else
		{
			ReasonMsg = FString::Printf(TEXT("저스트가드 실패 (미입력 — G키 누르지 않음)"));
		}

		GEngine->AddOnScreenDebugMessage(1003, 4.f, bOK ? FColor::Green : FColor::Red,
			FString::Printf(TEXT("[저스트가드 판정] %s"), *ReasonMsg));
		UE_LOG(LogTemp, Warning, TEXT("[JustGuard] Result: %s"), *ReasonMsg);
	}

	return Result;
}

void UBossJustGuardComponent::MarkJustGuardedResult(bool bApplyGroggy)
{
	UAbilitySystemComponent* ASC = GetASC();
	if (!ASC || ASC->HasMatchingGameplayTag(LostArkTags::State_Boss_PatternResult_JustGuarded.GetTag()))
	{
		return;
	}

	// 최종 저스트가드 성공 -> 그로기: State.Boss.Groggy 를 '먼저' 세운다. JustGuarded 가 붙는 순간
	// 분기가 동기적으로 그로기 스텝을 실행할 수 있는데, 그때 이미 Groggy 가 서 있어야
	// 루프 스텝의 'NOT Groggy' 종료 분기가 첫 평가에서 오발하지 않는다.
	// (UBossGroggyEffect GE 가 이 프로젝트에서 태그를 안 붙이는 문제 -> 복제 루스 태그+타이머로 직접 부여)
	AActor* Owner = GetOwner();
	if (bApplyGroggy && Owner && !ASC->HasMatchingGameplayTag(LostArkTags::State_Boss_Groggy.GetTag()))
	{
		UBossCombatStatics::AddReplicatedLooseTag(ASC, LostArkTags::State_Boss_Groggy.GetTag());
		FTimerHandle GroggyTimer;	// 로컬 핸들 — fire-and-forget (만료 시 태그 회수)
		Owner->GetWorldTimerManager().SetTimer(GroggyTimer,
			FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				if (UAbilitySystemComponent* A = GetASC())
				{
					UBossCombatStatics::RemoveReplicatedLooseTag(A, LostArkTags::State_Boss_Groggy.GetTag());
				}
			}),
			FMath::Max(0.1f, GroggyDuration), false);
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
