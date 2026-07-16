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
			if (HitActor && HitActor != InstigatorActor && HitActor->GetClass() != InstigatorActor->GetClass())
			{
				FVector HitLocation = HitActor->GetActorLocation();
				if (FMath::Abs(HitLocation.Z - ShapeCenter.Z) > DamageShapeParams.ZTolerance)
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
			if (HitActor && HitActor != InstigatorActor && HitActor->GetClass() != InstigatorActor->GetClass())
			{
				FVector HitLocation = HitActor->GetActorLocation();
				if (FMath::Abs(HitLocation.Z - ShapeCenter.Z) <= DamageShapeParams.ZTolerance)
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

		if (InstigatorASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC).WasSuccessfullyApplied())
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




