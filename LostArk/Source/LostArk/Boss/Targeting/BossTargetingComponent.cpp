// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Targeting/BossTargetingComponent.h"
#include "Boss/BossGameplayTags.h"
#include "Boss/Combat/BossCombatStatics.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

UBossTargetingComponent::UBossTargetingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);

	// 네이티브 태그로 기본값 지정 (에디터에서 오버라이드 가능)
	DeadTag = LostArkTags::State_Dead.GetTag();
	TrackTargetTag = LostArkTags::State_Boss_TrackTarget.GetTag();
	MarkTag = LostArkTags::State_Player_Marked.GetTag();
}

void UBossTargetingComponent::BeginPlay()
{
	Super::BeginPlay();

	// 회전은 서버 권한에서만 계산 (클라는 액터 회전 복제로 따라옴)
	if (const AActor* Owner = GetOwner())
	{
		SetComponentTickEnabled(Owner->HasAuthority());
	}

	// 태그 미지정 시 기본값 폴백
	if (!DeadTag.IsValid())
	{
		DeadTag = FGameplayTag::RequestGameplayTag(FName("State.Dead"), /*ErrorIfNotFound=*/false);
	}
	if (!TrackTargetTag.IsValid())
	{
		TrackTargetTag = FGameplayTag::RequestGameplayTag(FName("State.Boss.TrackTarget"), /*ErrorIfNotFound=*/false);
	}
	if (!MarkTag.IsValid())
	{
		MarkTag = FGameplayTag::RequestGameplayTag(FName("State.Player.Marked"), /*ErrorIfNotFound=*/false);
	}
}

void UBossTargetingComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UBossTargetingComponent, CurrentTarget);
}

void UBossTargetingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateFacing(DeltaTime);
	UpdateScriptedTurn(DeltaTime);
}

UAbilitySystemComponent* UBossTargetingComponent::GetOwnerASC() const
{
	return GetOwner() ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()) : nullptr;
}

bool UBossTargetingComponent::IsAlive(const AActor* PlayerActor) const
{
	// 태그 미지정/ASC 없음 = 판정 불가 -> 후보 포함 (규약은 공용 헬퍼 참고)
	return UBossCombatStatics::IsAliveActor(PlayerActor, DeadTag);
}

AActor* UBossTargetingComponent::SelectTarget(ETargetSelectPolicy Policy)
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();

	// 선정은 서버 전용
	if (!Owner || !Owner->HasAuthority() || !World)
	{
		return CurrentTarget;
	}

	// 생존 플레이어 폰 수집
	TArray<APawn*> Candidates;
	UBossCombatStatics::GetPlayerPawns(World, Candidates);
	Candidates.RemoveAll([this](const APawn* Pawn) { return !IsAlive(Pawn); });

	if (Candidates.Num() == 0)
	{
		CurrentTarget = nullptr;
		return nullptr;
	}

	AActor* Chosen = nullptr;
	if (Policy == ETargetSelectPolicy::Random)
	{
		Chosen = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
	}
	else
	{
		const FVector Origin = Owner->GetActorLocation();
		const bool bFarthest = (Policy == ETargetSelectPolicy::Farthest);
		float Best = bFarthest ? -1.f : TNumericLimits<float>::Max();
		for (APawn* P : Candidates)
		{
			const float DistSq = FVector::DistSquared(Origin, P->GetActorLocation());
			if (bFarthest ? (DistSq > Best) : (DistSq < Best))
			{
				Best = DistSq;
				Chosen = P;
			}
		}
	}

	CurrentTarget = Chosen;
	return Chosen;
}

void UBossTargetingComponent::MarkCurrentTarget()
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	AActor* NewMarked = CurrentTarget;
	if (MarkedTarget.Get() == NewMarked)
	{
		return;	// 이미 같은 대상이 표식 중 (중복 부여 방지)
	}

	// 이전 표식 대상에서 회수 (항상 최대 1명)
	if (AActor* Prev = MarkedTarget.Get())
	{
		if (UAbilitySystemComponent* PrevASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Prev))
		{
			UBossCombatStatics::RemoveReplicatedLooseTag(PrevASC, MarkTag);
		}
	}

	MarkedTarget = NewMarked;

	if (UAbilitySystemComponent* ASC = NewMarked
		? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(NewMarked) : nullptr)
	{
		UBossCombatStatics::AddReplicatedLooseTag(ASC, MarkTag);
	}
}

void UBossTargetingComponent::ClearMark()
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	if (AActor* Prev = MarkedTarget.Get())
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Prev))
		{
			UBossCombatStatics::RemoveReplicatedLooseTag(ASC, MarkTag);
		}
	}
	MarkedTarget = nullptr;
}

void UBossTargetingComponent::UpdateFacing(float DeltaTime)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	// 죽은 타겟이면 비움 (아래에서 재선정 시도)
	if (CurrentTarget && !IsAlive(CurrentTarget))
	{
		CurrentTarget = nullptr;
	}

	// TrackTarget 태그가 있을 때만 회전 (NotifyState가 켜고 끔)
	UAbilitySystemComponent* ASC = GetOwnerASC();
	const bool bTracking = ASC && TrackTargetTag.IsValid() && ASC->HasMatchingGameplayTag(TrackTargetTag);

	// 추적 시작 엣지: 이번 세션의 회전 방향을 새로 캡처하도록 리셋
	if (bTracking && !bWasTracking)
	{
		LastTrackTurnSign = 0.f;
	}
	bWasTracking = bTracking;

	if (!bTracking)
	{
		return;
	}

	// 추적을 요구받았는데 타겟이 없으면 자동 선정 (직접 몽타주 재생/타겟 사망 대비)
	if (!CurrentTarget)
	{
		SelectTarget(ETargetSelectPolicy::Nearest);
		if (!CurrentTarget)
		{
			return;	// 후보 없음(생존 플레이어 없음)
		}
	}

	// 평지 기준 yaw 만 추적 (캡슐+메시 함께 회전)
	FVector ToTarget = CurrentTarget->GetActorLocation() - Owner->GetActorLocation();
	ToTarget.Z = 0.f;
	if (ToTarget.IsNearlyZero())
	{
		return;
	}

	const float DesiredYaw = ToTarget.Rotation().Yaw;
	const FRotator Current = Owner->GetActorRotation();

	// 이번 추적 세션에서 아직 방향 미정이면, 유효한 각도 차가 생긴 첫 프레임에 부호 확정.
	// (RInterpTo 는 최단경로 회전이므로 시작 시점의 델타 부호 = 실제 도는 방향)
	if (LastTrackTurnSign == 0.f)
	{
		const float Delta = FMath::FindDeltaAngleDegrees(Current.Yaw, DesiredYaw);
		if (FMath::Abs(Delta) > 2.f)	// 미세 노이즈로 방향이 확정되는 것 방지
		{
			LastTrackTurnSign = FMath::Sign(Delta);
		}
	}

	const FRotator NewRot = FMath::RInterpTo(Current, FRotator(0.f, DesiredYaw, 0.f), DeltaTime, RotationInterpSpeed);
	Owner->SetActorRotation(FRotator(0.f, NewRot.Yaw, 0.f));
}

void UBossTargetingComponent::BeginScriptedTurn(float TurnRateDegPerSec, float FallbackSign)
{
	ScriptedTurnRate = FMath::Max(TurnRateDegPerSec, 0.f);

	// 추적 이력이 없으면(패턴이 추적 없이 바로 회전) 폴백 부호로 확정
	if (LastTrackTurnSign == 0.f)
	{
		LastTrackTurnSign = (FallbackSign < 0.f) ? -1.f : 1.f;
	}

	bScriptedTurnActive = true;
}

void UBossTargetingComponent::EndScriptedTurn()
{
	bScriptedTurnActive = false;
}

void UBossTargetingComponent::UpdateScriptedTurn(float DeltaTime)
{
	AActor* Owner = GetOwner();
	if (!bScriptedTurnActive || !Owner || !Owner->HasAuthority())
	{
		return;
	}

	// 추적 구간과 겹치면 추적(UpdateFacing)이 우선 -> 이중 회전 방지
	if (bWasTracking)
	{
		return;
	}

	const float DeltaYaw = LastTrackTurnSign * ScriptedTurnRate * DeltaTime;
	Owner->AddActorWorldRotation(FRotator(0.f, DeltaYaw, 0.f));
}
