#include "Abilities/LostArkGameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayEffect.h"
#include "GameplayTagsModule.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/Actor.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Character/LostArkAttributeSet.h"
#include "Character/LostArkCharacter.h"
#include "Monster/LostArkMonster.h"
#include "Boss/BossBase.h"

static bool IsValidDamageTarget(AActor* Instigator, AActor* Target)
{
	if (!Instigator || !Target || Instigator == Target)
	{
		return false;
	}

	// 플레이어끼리는 서로 피격/공격 불가 (PvP / Friendly Fire 방지)
	if (Instigator->IsA<ALostArkCharacter>() && Target->IsA<ALostArkCharacter>())
	{
		return false;
	}

	// 몬스터끼리도 서로 공격 불가
	if (Instigator->IsA<ALostArkMonster>() && Target->IsA<ALostArkMonster>())
	{
		return false;
	}

	return true;
}

ULostArkGameplayAbility::ULostArkGameplayAbility()
{
}

void ULostArkGameplayAbility::ApplyDamageShape(FVector Origin, FRotator Rotation, AActor* InstigatorActor)
{
	if (!DamageEffectClass || !InstigatorActor) return;

	UAbilitySystemComponent* InstigatorASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(InstigatorActor);
	if (!InstigatorASC) return;

	TArray<AActor*> OverlappedActors;
	UWorld* World = InstigatorActor->GetWorld();
	if (!World) return;

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(InstigatorActor);

	FVector ForwardDir = Rotation.Vector();
	ForwardDir.Z = 0.f;
	ForwardDir.Normalize();

	FVector ShapeCenter = Origin + ForwardDir * DamageShapeParams.ForwardOffset;

	if (DamageShapeParams.ShapeType == EDamageShape::Sphere || DamageShapeParams.ShapeType == EDamageShape::Cone)
	{
		FCollisionShape CollisionShape;
		CollisionShape.SetCapsule(DamageShapeParams.Radius, DamageShapeParams.ZTolerance);

		TArray<FOverlapResult> Overlaps;
		World->OverlapMultiByObjectType(Overlaps, ShapeCenter, FQuat::Identity, ObjectQueryParams, CollisionShape, QueryParams);

		for (const FOverlapResult& Overlap : Overlaps)
		{
			AActor* HitActor = Overlap.GetActor();
			if (IsValidDamageTarget(InstigatorActor, HitActor))
			{
				FVector HitLocation = HitActor->GetActorLocation();
				// 보스는 캡슐이 크기 때문에 Z 체크를 건너뜀
				bool bIsBoss = HitActor->IsA<ABossBase>();
				if (!bIsBoss && FMath::Abs(HitLocation.Z - ShapeCenter.Z) > DamageShapeParams.ZTolerance)
				{
					continue;
				}

				if (DamageShapeParams.ShapeType == EDamageShape::Cone)
				{
					FVector2D DirToTarget((HitLocation.X - ShapeCenter.X), (HitLocation.Y - ShapeCenter.Y));
					DirToTarget.Normalize();

					FVector2D ForwardDir2D(ForwardDir.X, ForwardDir.Y);
					ForwardDir2D.Normalize();

					float DotP = FVector2D::DotProduct(ForwardDir2D, DirToTarget);
					float Angle = FMath::RadiansToDegrees(FMath::Acos(DotP));
					if (Angle <= DamageShapeParams.ConeAngleDegrees * 0.5f)
					{
						OverlappedActors.AddUnique(HitActor);
					}
				}
				else
				{
					float DistSq2D = FVector::DistSquaredXY(ShapeCenter, HitLocation);
					if (DistSq2D <= FMath::Square(DamageShapeParams.Radius))
					{
						OverlappedActors.AddUnique(HitActor);
					}
				}
			}
		}

#if ENABLE_DRAW_DEBUG
		if (DamageShapeParams.bShowDebugLines)
		{
			FVector DrawCenter = ShapeCenter; 

			if (DamageShapeParams.ShapeType == EDamageShape::Sphere)
			{
				DrawDebugCircle(World, DrawCenter, DamageShapeParams.Radius, 32, FColor::Red, false, 2.0f, 0, 2.0f, FVector(0,1,0), FVector(1,0,0), false);
			}
			else if (DamageShapeParams.ShapeType == EDamageShape::Cone)
			{
				float HalfAngleRad = FMath::DegreesToRadians(DamageShapeParams.ConeAngleDegrees * 0.5f);
				FVector LeftDir = ForwardDir.RotateAngleAxis(-DamageShapeParams.ConeAngleDegrees * 0.5f, FVector::UpVector);
				FVector RightDir = ForwardDir.RotateAngleAxis(DamageShapeParams.ConeAngleDegrees * 0.5f, FVector::UpVector);
				
				DrawDebugLine(World, DrawCenter, DrawCenter + LeftDir * DamageShapeParams.Radius, FColor::Orange, false, 2.0f, 0, 2.0f);
				DrawDebugLine(World, DrawCenter, DrawCenter + RightDir * DamageShapeParams.Radius, FColor::Orange, false, 2.0f, 0, 2.0f);
				
				const int32 ArcSegments = 16;
				float AngleStep = DamageShapeParams.ConeAngleDegrees / ArcSegments;
				FVector PrevPoint = DrawCenter + LeftDir * DamageShapeParams.Radius;
				for (int32 i = 1; i <= ArcSegments; ++i)
				{
					FVector NextDir = ForwardDir.RotateAngleAxis(-DamageShapeParams.ConeAngleDegrees * 0.5f + (AngleStep * i), FVector::UpVector);
					FVector NextPoint = DrawCenter + NextDir * DamageShapeParams.Radius;
					DrawDebugLine(World, PrevPoint, NextPoint, FColor::Orange, false, 2.0f, 0, 2.0f);
					PrevPoint = NextPoint;
				}
			}
		}
#endif
	}
	else if (DamageShapeParams.ShapeType == EDamageShape::Box)
	{
		FCollisionShape CollisionShape;
		FVector BoxExtent = DamageShapeParams.BoxExtent;
		BoxExtent.Z = DamageShapeParams.ZTolerance;
		CollisionShape.SetBox(FVector3f(BoxExtent));

		FRotator FlatRotation(0.f, Rotation.Yaw, 0.f);

		TArray<FOverlapResult> Overlaps;
		World->OverlapMultiByObjectType(Overlaps, ShapeCenter, FlatRotation.Quaternion(), ObjectQueryParams, CollisionShape, QueryParams);

		for (const FOverlapResult& Overlap : Overlaps)
		{
			AActor* HitActor = Overlap.GetActor();
			if (IsValidDamageTarget(InstigatorActor, HitActor))
			{
				FVector HitLocation = HitActor->GetActorLocation();
				// 보스는 캡슐이 크기 때문에 Z 체크를 건너뜀
				bool bIsBoss = HitActor->IsA<ABossBase>();
				if (bIsBoss || FMath::Abs(HitLocation.Z - ShapeCenter.Z) <= DamageShapeParams.ZTolerance)
				{
					OverlappedActors.AddUnique(HitActor);
				}
			}
		}

#if ENABLE_DRAW_DEBUG
		if (DamageShapeParams.bShowDebugLines)
		{
			FVector FlatBoxExtent = BoxExtent;
			FlatBoxExtent.Z = 1.0f;
			DrawDebugBox(World, ShapeCenter, FlatBoxExtent, FlatRotation.Quaternion(), FColor::Purple, false, 2.0f, 0, 2.0f);
		}
#endif
	}

	FGameplayEffectContextHandle ContextHandle = InstigatorASC->MakeEffectContext();
	ContextHandle.AddInstigator(InstigatorActor, InstigatorActor);

	float CurrentIdentity = InstigatorASC->GetNumericAttribute(ULostArkAttributeSet::GetIdentityGaugeAttribute());
	float MaxIdentity = InstigatorASC->GetNumericAttribute(ULostArkAttributeSet::GetMaxIdentityGaugeAttribute());
	float IdentityPercent = (MaxIdentity > 0.f) ? (CurrentIdentity / MaxIdentity) : 0.f;

	float DamageMultiplier = 1.0f;
	if (InstigatorASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("State.IdentityBurst"), false)))
	{
		DamageMultiplier = 2.0f;
	}
	else if (IdentityPercent >= 0.5f)
	{
		DamageMultiplier = 1.5f;
	}

	int32 HitCount = 0;

	for (AActor* HitActor : OverlappedActors)
	{
		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
		if (!TargetASC) continue;

		FGameplayEffectSpecHandle SpecHandle = InstigatorASC->MakeOutgoingSpec(DamageEffectClass, 1.f, ContextHandle);
		if (!SpecHandle.IsValid()) continue;

		if (bIsCounterSkill)
		{
			SpecHandle.Data->AddDynamicAssetTag(FGameplayTag::RequestGameplayTag(FName("Ability.Type.Counter"), false));
		}

		UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(
			SpecHandle,
			FGameplayTag::RequestGameplayTag(FName("Data.Damage"), false),
			DamageShapeParams.DamageCoefficient * DamageMultiplier
		);

		FActiveGameplayEffectHandle ApplyHandle = InstigatorASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
		if (ApplyHandle.WasSuccessfullyApplied())
		{
			HitCount++;
		}
	}

	if (HitCount > 0 && !InstigatorASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName("State.IdentityBurst"), false)))
	{
		float IdentityGainPerHit = 5.0f;
		InstigatorASC->ApplyModToAttributeUnsafe(ULostArkAttributeSet::GetIdentityGaugeAttribute(), EGameplayModOp::Additive, IdentityGainPerHit * HitCount);
	}
}

void ULostArkGameplayAbility::SetupHitCheckListener()
{
	if (!DamageEffectClass) return;

	UAbilityTask_WaitGameplayEvent* HitCheckTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		FGameplayTag::RequestGameplayTag(FName("Gameplay.Event.HitCheck"), false),
		nullptr,
		false,
		true
	);
	if (HitCheckTask)
	{
		HitCheckTask->EventReceived.AddDynamic(this, &ULostArkGameplayAbility::OnHitCheckReceived);
		HitCheckTask->ReadyForActivation();
	}
}

void ULostArkGameplayAbility::OnHitCheckReceived(FGameplayEventData Payload)
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!AvatarActor) return;

	ApplyDamageShape(
		AvatarActor->GetActorLocation(),
		AvatarActor->GetActorRotation(),
		AvatarActor
	);
}

FVector ULostArkGameplayAbility::ComputeSafeDashDestination(ACharacter* AvatarChar, const FVector& Start, const FVector& DesiredEnd) const
{
	if (!AvatarChar) return DesiredEnd;
	UWorld* World = AvatarChar->GetWorld();
	if (!World) return DesiredEnd;

	const UCapsuleComponent* Capsule = AvatarChar->GetCapsuleComponent();
	const float Radius = Capsule ? Capsule->GetScaledCapsuleRadius() : 34.f;
	const float HalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 88.f;
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, HalfHeight);

	FCollisionQueryParams Params(FName(TEXT("LostArkDashDest")), false, AvatarChar);

	// 1) 벽/보스 앞에서 멈추도록 목적지 클램프.
	//    다른 플레이어/몬스터 폰은 통과(스킵), 벽(월드) 또는 보스(플레이어·몬스터가 아닌 폰)에서만 멈춘다.
	FVector ClampedEnd = DesiredEnd;
	{
		FCollisionObjectQueryParams ObjParams;
		ObjParams.AddObjectTypesToQuery(ECC_WorldStatic);
		ObjParams.AddObjectTypesToQuery(ECC_WorldDynamic);
		ObjParams.AddObjectTypesToQuery(ECC_Pawn);

		TArray<FHitResult> Hits;
		if (World->SweepMultiByObjectType(Hits, Start, DesiredEnd, FQuat::Identity, ObjParams, Shape, Params))
		{
			for (const FHitResult& Hit : Hits)
			{
				AActor* HitActor = Hit.GetActor();
				if (!HitActor) continue;

				// 아군 플레이어/일반 몬스터는 통과시킨다(대시로 지나갈 수 있어야 함).
				if (HitActor->IsA<ALostArkCharacter>() || HitActor->IsA<ALostArkMonster>())
				{
					continue;
				}

				// 벽 또는 보스 -> 여기서 멈춘다. (Hit.Location = 접촉 시 캡슐 중심 위치)
				ClampedEnd = Hit.Location;
				break;
			}
		}
	}

	// 2) 아레나 바닥이 있는 마지막 지점까지만 전진 허용 (플랫폼 밖으로 못 나가게).
	//    프로젝트 표준: WorldStatic/WorldDynamic 오브젝트 타입으로 발밑 바닥 추적.
	FCollisionObjectQueryParams FloorObj;
	FloorObj.AddObjectTypesToQuery(ECC_WorldStatic);
	FloorObj.AddObjectTypesToQuery(ECC_WorldDynamic);

	const int32 Steps = 12;
	FVector SafeEnd = Start;
	for (int32 i = 1; i <= Steps; ++i)
	{
		const float T = static_cast<float>(i) / static_cast<float>(Steps);
		const FVector Sample = FMath::Lerp(Start, ClampedEnd, T);

		// 캡슐 중심에서 발밑 아래로 바닥 탐색 (발밑 300 유닛까지)
		const FVector TraceStart = Sample;
		const FVector TraceEnd = Sample - FVector(0.f, 0.f, HalfHeight + 300.f);

		FHitResult FloorHit;
		if (World->LineTraceSingleByObjectType(FloorHit, TraceStart, TraceEnd, FloorObj, Params))
		{
			SafeEnd = Sample; // 바닥 있음 -> 계속 전진
		}
		else
		{
			break;            // 바닥 없음 = 플랫폼 끝 -> 직전 지점에서 멈춘다
		}
	}

	return SafeEnd;
}




