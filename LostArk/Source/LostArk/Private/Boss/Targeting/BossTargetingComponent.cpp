// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Targeting/BossTargetingComponent.h"
#include "Boss/BossGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/PlayerController.h"
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
}

UAbilitySystemComponent* UBossTargetingComponent::GetOwnerASC() const
{
	return GetOwner() ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()) : nullptr;
}

bool UBossTargetingComponent::IsAlive(const AActor* PlayerActor) const
{
	if (!PlayerActor)
	{
		return false;
	}
	// 사망 태그가 설정 안 됐으면 판정 불가 -> 살아있다고 간주(후보 포함)
	if (!DeadTag.IsValid())
	{
		return true;
	}
	if (const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PlayerActor))
	{
		return !ASC->HasMatchingGameplayTag(DeadTag);
	}
	// ASC 없으면 판정 불가 -> 후보 포함
	return true;
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
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (Pawn && IsAlive(Pawn))
		{
			Candidates.Add(Pawn);
		}
	}

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
	if (!ASC || !TrackTargetTag.IsValid() || !ASC->HasMatchingGameplayTag(TrackTargetTag))
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
	const FRotator NewRot = FMath::RInterpTo(Current, FRotator(0.f, DesiredYaw, 0.f), DeltaTime, RotationInterpSpeed);
	Owner->SetActorRotation(FRotator(0.f, NewRot.Yaw, 0.f));
}
