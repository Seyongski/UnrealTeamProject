// Fill out your copyright notice in the Description page of Project Settings.


#include "Damage/BossAoe_Grab.h"
#include "Boss/BossGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "TimerManager.h"

ABossAoe_Grab::ABossAoe_Grab()
{
	// 경로 1회 판정 -> 잡기 -> 유지 후 해제. (유지시간은 GrabHoldTime 이 담당하므로 Duration=0)
	Duration = 0.f;
	bSingleHitPerTarget = true;

	GrabbedTag = LostArkTags::State_Player_Grabbed;
}

void ABossAoe_Grab::ApplyEffectsTo(AActor* Target)
{
	// 적중 = 데미지가 아니라 '잡기'. 데미지+던지기는 해제 시점(RestoreOne)에.
	if (ACharacter* Char = Cast<ACharacter>(Target))
	{
		GrabOne(Char);
	}
}

void ABossAoe_Grab::GrabOne(ACharacter* Char)
{
	FGrabbedTargetInfo Info;
	Info.Character = Char;

	// 1) 잡힘 상태 부여: GE 우선, 없으면 복제 루스 태그 폴백
	//    (플레이어 쪽 입력 차단/연출은 이 태그에 팀원이 바인딩)
	if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Char))
	{
		if (GrabbedEffect)
		{
			FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
			Context.AddSourceObject(this);
			Context.AddInstigator(Caster, this);
			FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(GrabbedEffect, 1.f, Context);
			if (Spec.IsValid())
			{
				Info.GrabbedGEHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
			}
		}
		else
		{
			ASC->AddLooseGameplayTag(GrabbedTag);
			ASC->AddReplicatedLooseGameplayTag(GrabbedTag);	// 클라 UI/입력 차단용 복제
			Info.bTagFallback = true;
		}
	}

	// 2) 서버 권위로 이동 정지 + 캡슐 충돌 차단 (CMC가 부착 위치와 싸우지 않게)
	if (UCharacterMovementComponent* Move = Char->GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();	// MOVE_None
	}
	if (UCapsuleComponent* Capsule = Char->GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// 3) 보스 메시 소켓에 부착 (잡힌 순서대로 소켓 배정)
	USkeletalMeshComponent* BossMesh = nullptr;
	if (const ACharacter* BossChar = Cast<ACharacter>(Caster))
	{
		BossMesh = BossChar->GetMesh();
	}

	const int32 Index = GrabbedTargets.Num();
	const FName Socket = GrabSocketNames.Num() > 0
		? GrabSocketNames[Index % GrabSocketNames.Num()]
		: NAME_None;

	if (BossMesh)
	{
		Char->AttachToComponent(BossMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
	}
	else if (Caster)
	{
		Char->AttachToActor(Caster, FAttachmentTransformRules::KeepWorldTransform);
	}

	GrabbedTargets.Add(MoveTemp(Info));
}

void ABossAoe_Grab::FinishAoe()
{
	// 잡은 대상이 있으면 유지시간 동안 파괴를 미룬다 (서버)
	if (HasAuthority() && !bReleased && GrabbedTargets.Num() > 0)
	{
		if (!GetWorldTimerManager().IsTimerActive(ReleaseTimerHandle))
		{
			GetWorldTimerManager().SetTimer(
				ReleaseTimerHandle, this, &ABossAoe_Grab::ReleaseAll,
				FMath::Max(GrabHoldTime, KINDA_SMALL_NUMBER), false);
		}
		return;
	}

	Super::FinishAoe();
}

void ABossAoe_Grab::ReleaseAll()
{
	if (bReleased)
	{
		return;
	}
	bReleased = true;
	GetWorldTimerManager().ClearTimer(ReleaseTimerHandle);

	for (FGrabbedTargetInfo& Info : GrabbedTargets)
	{
		RestoreOne(Info, /*bWithThrow=*/true);
	}
	GrabbedTargets.Empty();

	Super::FinishAoe();
}

void ABossAoe_Grab::RestoreOne(FGrabbedTargetInfo& Info, bool bWithThrow)
{
	ACharacter* Char = Info.Character;
	if (!Char)
	{
		return;
	}

	// 부착 해제 + 상태 복구
	Char->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	if (UCapsuleComponent* Capsule = Char->GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
	if (UCharacterMovementComponent* Move = Char->GetCharacterMovement())
	{
		Move->SetMovementMode(MOVE_Falling);	// 공중 해제 -> 착지하면 자동 Walking
	}

	if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Char))
	{
		if (Info.GrabbedGEHandle.IsValid())
		{
			ASC->RemoveActiveGameplayEffect(Info.GrabbedGEHandle);
		}
		else if (Info.bTagFallback)
		{
			ASC->RemoveLooseGameplayTag(GrabbedTag);
			ASC->RemoveReplicatedLooseGameplayTag(GrabbedTag);
		}
	}

	if (bWithThrow)
	{
		// 해제 데미지 + 상태이상 (베이스의 GE 적용 로직 재사용)
		Super::ApplyEffectsTo(Char);

		// 보스 중심 -> 대상 방향 바깥으로 던지기 (파괴 지형으로 날아가면 낙사 연계)
		FVector Dir = Char->GetActorLocation() - (Caster ? Caster->GetActorLocation() : AttackCenter);
		Dir.Z = 0.f;
		if (!Dir.Normalize())
		{
			Dir = GetShapeForward();
		}
		Char->LaunchCharacter(Dir * ThrowHorizontalSpeed + FVector(0.f, 0.f, ThrowVerticalSpeed), true, true);
	}
}

void ABossAoe_Grab::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 해제 전에 외부 요인으로 파괴되면(보스 사망 등) 데미지/던지기 없이 안전 복구
	if (HasAuthority() && !bReleased && GrabbedTargets.Num() > 0)
	{
		for (FGrabbedTargetInfo& Info : GrabbedTargets)
		{
			RestoreOne(Info, /*bWithThrow=*/false);
		}
		GrabbedTargets.Empty();
	}

	Super::EndPlay(EndPlayReason);
}
