// Fill out your copyright notice in the Description page of Project Settings.


#include "Damage/BossAoeGrabEffect.h"
#include "Damage/BossPatternActorBase.h"
#include "Boss/BossGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "TimerManager.h"

UBossAoeGrabEffect::UBossAoeGrabEffect()
{
	GrabbedTag = LostArkTags::State_Player_Grabbed;
}

void UBossAoeGrabEffect::ApplyOverride(const FBossGrabOverride& Override)
{
	GrabSocketNames = Override.GrabSocketNames;
	SocketOrder = Override.SocketOrder;
	bAutoRelease = Override.bAutoRelease;
	GrabHoldTime = Override.GrabHoldTime;
	FailsafeReleaseTime = Override.FailsafeReleaseTime;
	ThrowHorizontalSpeed = Override.ThrowHorizontalSpeed;
	ThrowVerticalSpeed = Override.ThrowVerticalSpeed;
}

void UBossAoeGrabEffect::OnHit(ABossPatternActorBase* Aoe, AActor* Target)
{
	// 적중 = 데미지가 아니라 '잡기'. 데미지+던지기는 해제 시점(RestoreOne)에.
	if (ACharacter* Char = Cast<ACharacter>(Target))
	{
		// 첫 잡기 성공 -> 보스에 패턴 결과 태그 (Branch 조건: '잡힌 사람 있으면 다음 몽타주')
		if (Aoe && GrabbedTargets.Num() == 0)
		{
			Aoe->AddCasterLooseTags(
				FGameplayTagContainer(LostArkTags::State_Boss_PatternResult_Grabbed));
		}
		GrabOne(Aoe, Char);
	}
}

void UBossAoeGrabEffect::GrabOne(ABossPatternActorBase* Aoe, ACharacter* Char)
{
	AActor* Caster = Aoe ? Aoe->GetCaster() : nullptr;

	FGrabbedTargetInfo Info;
	Info.Character = Char;

	// 전방 기준 각도 기록 (소켓 배정 정렬용). 전방=0, 오른쪽 +
	if (Aoe && Caster)
	{
		FVector ToChar = Char->GetActorLocation() - Caster->GetActorLocation();
		ToChar.Z = 0.f;
		const float F = FVector::DotProduct(ToChar, Aoe->GetShapeForwardVector());
		const float R = FVector::DotProduct(ToChar, Aoe->GetShapeRightVector());
		Info.Angle = FMath::RadiansToDegrees(FMath::Atan2(R, F));
	}

	// 1) 잡힘 상태 부여: GE 우선, 없으면 복제 루스 태그 폴백
	//    (플레이어 쪽 입력 차단/연출은 이 태그에 팀원이 바인딩)
	if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Char))
	{
		if (GrabbedEffect)
		{
			FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
			Context.AddSourceObject(this);
			Context.AddInstigator(Caster, Aoe);
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

void UBossAoeGrabEffect::AssignSocketsByOrder(ABossPatternActorBase* Aoe)
{
	if (SocketOrder == EGrabSocketOrder::DetectionOrder || GrabSocketNames.Num() == 0)
	{
		return;	// 감지 순서 그대로 (GrabOne 이 이미 배정)
	}

	const ACharacter* BossChar = Aoe ? Cast<ACharacter>(Aoe->GetCaster()) : nullptr;
	USkeletalMeshComponent* BossMesh = BossChar ? BossChar->GetMesh() : nullptr;
	if (!BossMesh)
	{
		return;
	}

	// 각도 정렬: 시계=오름차순, 반시계=내림차순
	const bool bClockwise = (SocketOrder == EGrabSocketOrder::Clockwise);
	GrabbedTargets.Sort([bClockwise](const FGrabbedTargetInfo& A, const FGrabbedTargetInfo& B)
	{
		return bClockwise ? (A.Angle < B.Angle) : (A.Angle > B.Angle);
	});

	// 정렬 순서대로 소켓 재부착
	for (int32 i = 0; i < GrabbedTargets.Num(); ++i)
	{
		if (ACharacter* C = GrabbedTargets[i].Character)
		{
			const FName Socket = GrabSocketNames[i % GrabSocketNames.Num()];
			C->AttachToComponent(BossMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
		}
	}
}

bool UBossAoeGrabEffect::OnFinish(ABossPatternActorBase* Aoe)
{
	// 잡은 대상이 있으면 해제 시점까지 파괴를 미룬다 (서버)
	if (Aoe && Aoe->HasAuthority() && !bReleased && GrabbedTargets.Num() > 0)
	{
		// 모든 잡기가 끝난 시점 -> 각도 순으로 소켓 재배정(시계/반시계)
		AssignSocketsByOrder(Aoe);

		OwningAoe = Aoe;
		if (!Aoe->GetWorldTimerManager().IsTimerActive(ReleaseTimerHandle))
		{
			// 자동 해제: GrabHoldTime 후. 노티파이 해제 모드: 실패 안전 타이머만
			// (해제 자체는 몽타주의 'Boss Grab Release' 노티파이가 ReleaseAll 호출)
			const float ReleaseAfter = bAutoRelease
				? FMath::Max(GrabHoldTime, KINDA_SMALL_NUMBER)
				: FMath::Max(FailsafeReleaseTime, 1.f);
			Aoe->GetWorldTimerManager().SetTimer(
				ReleaseTimerHandle, this, &UBossAoeGrabEffect::ReleaseAll,
				ReleaseAfter, false);
		}
		return true;	// 파괴 지연
	}
	return false;
}

void UBossAoeGrabEffect::ReleaseAll()
{
	if (bReleased)
	{
		return;
	}
	bReleased = true;

	ABossPatternActorBase* Aoe = OwningAoe.Get();
	if (Aoe)
	{
		Aoe->GetWorldTimerManager().ClearTimer(ReleaseTimerHandle);
	}

	for (FGrabbedTargetInfo& Info : GrabbedTargets)
	{
		RestoreOne(Aoe, Info, /*bWithThrow=*/true);
	}
	GrabbedTargets.Empty();

	if (Aoe)
	{
		Aoe->DestroyAoeNow();
	}
}

void UBossAoeGrabEffect::RestoreOne(ABossPatternActorBase* Aoe, FGrabbedTargetInfo& Info, bool bWithThrow)
{
	ACharacter* Char = Info.Character;
	if (!Char)
	{
		return;
	}

	// 부착 해제
	Char->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	// 던지기/탈출 방향 (보스 중심 -> 대상 바깥, 수평). 캐스터 없으면 전방 폴백.
	const ACharacter* BossChar = Aoe ? Cast<ACharacter>(Aoe->GetCaster()) : nullptr;
	const FVector BossLoc = BossChar ? BossChar->GetActorLocation()
		: (Aoe ? Aoe->GetAttackCenter() : Char->GetActorLocation());
	FVector Dir = Char->GetActorLocation() - BossLoc;
	Dir.Z = 0.f;
	if (!Dir.Normalize())
	{
		Dir = Aoe ? Aoe->GetShapeForwardVector() : FVector::ForwardVector;
	}

	// 충돌 재활성 '전에' 보스 캡슐 밖으로 빼낸다.
	// (거대 보스 캡슐과 겹친 채로 충돌을 켜면 디페너트레이션이 보스를 튕겨 날림)
	if (BossChar)
	{
		if (const UCapsuleComponent* BossCap = BossChar->GetCapsuleComponent())
		{
			float PlayerRadius = 34.f;
			float PlayerHalfH = 88.f;
			if (const UCapsuleComponent* PlayerCap = Char->GetCapsuleComponent())
			{
				PlayerRadius = PlayerCap->GetScaledCapsuleRadius();
				PlayerHalfH = PlayerCap->GetScaledCapsuleHalfHeight();
			}
			const float SafeDist = BossCap->GetScaledCapsuleRadius() + PlayerRadius + 30.f;
			const float FeetZ = BossLoc.Z - BossCap->GetScaledCapsuleHalfHeight();

			FVector SafeLoc = BossLoc + Dir * SafeDist;
			SafeLoc.Z = FeetZ + PlayerHalfH + 5.f;	// 보스 발밑 높이에 세워 겹침 제거
			Char->SetActorLocation(SafeLoc, false, nullptr, ETeleportType::TeleportPhysics);
		}
	}

	// 이제 충돌/이동 복구 (겹침이 없으므로 튕기지 않음)
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

	if (bWithThrow && Aoe)
	{
		// 해제 데미지 + 상태이상 (베이스의 GE 적용 로직 재사용)
		Aoe->ApplyDamageAndStatus(Char);

		// 보스 밖 방향으로 던지기 (파괴 지형으로 날아가면 낙사 연계)
		Char->LaunchCharacter(Dir * ThrowHorizontalSpeed + FVector(0.f, 0.f, ThrowVerticalSpeed), true, true);
	}
}

void UBossAoeGrabEffect::OnEndPlay(ABossPatternActorBase* Aoe)
{
	// 해제 전에 외부 요인으로 파괴되면(보스 사망 등) 데미지/던지기 없이 안전 복구
	if (Aoe && Aoe->HasAuthority() && !bReleased && GrabbedTargets.Num() > 0)
	{
		for (FGrabbedTargetInfo& Info : GrabbedTargets)
		{
			RestoreOne(Aoe, Info, /*bWithThrow=*/false);
		}
		GrabbedTargets.Empty();
	}
}
