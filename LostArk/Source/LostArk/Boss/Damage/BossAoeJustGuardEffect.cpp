// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Damage/BossAoeJustGuardEffect.h"
#include "Boss/Damage/BossPatternActorBase.h"
#include "Boss/Combat/BossJustGuardComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "TimerManager.h"

void UBossAoeJustGuardEffect::OnHit(ABossPatternActorBase* Aoe, AActor* Target)
{
	// T(다 차오른 순간) 히트 = 데미지가 아니라 '가드 판정 후보 확보'. 실제 데미지는 판정 시각 J(ResolveGuards)에서.
	if (!Aoe || !Target)
	{
		return;
	}

	if (HitTime < 0.f)
	{
		UWorld* World = Aoe->GetWorld();
		HitTime = World ? World->GetTimeSeconds() : 0.f;
	}

	PendingTargets.Add(Target);
}

bool UBossAoeJustGuardEffect::OnFinish(ABossPatternActorBase* Aoe)
{
	if (!Aoe || !Aoe->HasAuthority() || bResolved)
	{
		return false;
	}

	OwningAoe = Aoe;

	// 히트 순간 아무도 없었으면(HitTime 미설정) 지금을 기준으로 삼는다
	if (HitTime < 0.f)
	{
		UWorld* World = Aoe->GetWorld();
		HitTime = World ? World->GetTimeSeconds() : 0.f;
	}

	// 판정 시각 J = T + JudgmentDelay 까지 파괴를 미룬다 (성공 윈도우 [J-Duration, J] 는 J 에서 끝나므로
	// J 에 판정하면 충분). OnFinish 는 T 와 (거의) 같은 프레임에 오므로 '지금부터 Delay' 후가 J 다.
	const float ResolveAfter = FMath::Max(JudgmentDelay, KINDA_SMALL_NUMBER);
	if (!Aoe->GetWorldTimerManager().IsTimerActive(ResolveTimerHandle))
	{
		Aoe->GetWorldTimerManager().SetTimer(
			ResolveTimerHandle, this, &UBossAoeJustGuardEffect::ResolveGuards,
			ResolveAfter, false);
	}
	return true;	// 파괴 지연
}

void UBossAoeJustGuardEffect::ResolveGuards()
{
	if (bResolved)
	{
		return;
	}
	bResolved = true;

	ABossPatternActorBase* Aoe = OwningAoe.Get();
	if (!Aoe)
	{
		return;
	}
	Aoe->GetWorldTimerManager().ClearTimer(ResolveTimerHandle);

	UBossJustGuardComponent* JustGuard = nullptr;
	if (AActor* Boss = Aoe->GetCaster())
	{
		JustGuard = Boss->FindComponentByClass<UBossJustGuardComponent>();
	}

	FJustGuardResolveParams Params;
	Params.HitTime = HitTime;
	Params.JudgmentDelay = JudgmentDelay;
	Params.GuardWindowDuration = GuardWindowDuration;
	Params.GuardAngleTolerance = GuardAngleTolerance;
	Params.GuardCenter = Aoe->GetAttackCenter();
	Params.bBypassDirection = bDebugBypassDirection;

	bool bAnySuccess = false;

	for (const TWeakObjectPtr<AActor>& Weak : PendingTargets)
	{
		AActor* Target = Weak.Get();
		if (!Target)
		{
			continue;
		}

		const EJustGuardResult Result = JustGuard
			? JustGuard->ResolveGuard(Target, Params)
			: EJustGuardResult::FailNoInput;

		if (Result == EJustGuardResult::Success)
		{
			bAnySuccess = true;
			// 성공: 데미지 스킵
		}
		else
		{
			// 실패: 일반 장판과 동일하게 데미지+상태이상+넉백 (베이스 로직 재사용.
			// 넉백은 장판 BP 의 Knockback 설정이 None 이면 아무 일도 없음)
			Aoe->ApplyDamageAndStatus(Target);
			Aoe->ApplyKnockback(Target);
		}

		if (bShowDebugResult && GEngine)
		{
			const bool bOK = (Result == EJustGuardResult::Success);
			FString Msg;
			switch (Result)
			{
			case EJustGuardResult::Success:			Msg = TEXT("저스트가드 성공"); break;
			case EJustGuardResult::FailTiming:		Msg = TEXT("저스트가드 실패 (타이밍)"); break;
			case EJustGuardResult::FailDirection:	Msg = TEXT("저스트가드 실패 (방향)"); break;
			default:								Msg = TEXT("저스트가드 실패 (미입력)"); break;
			}
			GEngine->AddOnScreenDebugMessage(-1, 2.f, bOK ? FColor::Green : FColor::Red,
				FString::Printf(TEXT("[%s] %s"), *Target->GetName(), *Msg));
		}
	}

	if (bAnySuccess && JustGuard)
	{
		JustGuard->MarkJustGuardedResult();	// Branch 조건용 (첫 성공 1회)
	}

	PendingTargets.Reset();
	Aoe->DestroyAoeNow();
}

void UBossAoeJustGuardEffect::OnEndPlay(ABossPatternActorBase* Aoe)
{
	// 판정 전에 외부 요인으로 파괴되면(보스 사망 등) 데미지 없이 정리 (패턴 중단으로 간주)
	if (Aoe)
	{
		Aoe->GetWorldTimerManager().ClearTimer(ResolveTimerHandle);
	}
	PendingTargets.Reset();
}
