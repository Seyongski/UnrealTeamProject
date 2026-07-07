#include "LostArkGameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayEffect.h"
#include "GameplayTagsModule.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/Actor.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"

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
	FVector ShapeCenter = Origin + ForwardDir * DamageShapeParams.ForwardOffset;

	if (DamageShapeParams.ShapeType == EDamageShape::Sphere || DamageShapeParams.ShapeType == EDamageShape::Cone)
	{
		FCollisionShape CollisionShape;
		CollisionShape.SetSphere(DamageShapeParams.Radius);

		TArray<FOverlapResult> Overlaps;
		World->OverlapMultiByObjectType(Overlaps, ShapeCenter, FQuat::Identity, ObjectQueryParams, CollisionShape, QueryParams);

		for (const FOverlapResult& Overlap : Overlaps)
		{
			AActor* HitActor = Overlap.GetActor();
			if (HitActor && HitActor != InstigatorActor && HitActor->GetClass() != InstigatorActor->GetClass())
			{
				if (DamageShapeParams.ShapeType == EDamageShape::Cone)
				{
					FVector DirToTarget = (HitActor->GetActorLocation() - ShapeCenter).GetSafeNormal();
					float DotP = FVector::DotProduct(ForwardDir, DirToTarget);
					float Angle = FMath::RadiansToDegrees(FMath::Acos(DotP));
					if (Angle <= DamageShapeParams.ConeAngleDegrees * 0.5f)
					{
						OverlappedActors.AddUnique(HitActor);
					}
				}
				else
				{
					OverlappedActors.AddUnique(HitActor);
				}
			}
		}

#if ENABLE_DRAW_DEBUG
		if (DamageShapeParams.bShowDebugLines)
		{
			if (DamageShapeParams.ShapeType == EDamageShape::Sphere)
		{
			DrawDebugSphere(World, ShapeCenter, DamageShapeParams.Radius, 32, FColor::Red, false, 2.0f, 0, 2.0f);
		}
			else if (DamageShapeParams.ShapeType == EDamageShape::Cone)
			{
				float HalfAngleRad = FMath::DegreesToRadians(DamageShapeParams.ConeAngleDegrees * 0.5f);
				DrawDebugCone(World, ShapeCenter, ForwardDir, DamageShapeParams.Radius, HalfAngleRad, HalfAngleRad, 32, FColor::Orange, false, 2.0f, 0, 2.0f);
			}
		}
#endif
	}
	else if (DamageShapeParams.ShapeType == EDamageShape::Box)
	{
		FCollisionShape CollisionShape;
		CollisionShape.SetBox(FVector3f(DamageShapeParams.BoxExtent));

		TArray<FOverlapResult> Overlaps;
		World->OverlapMultiByObjectType(Overlaps, ShapeCenter, Rotation.Quaternion(), ObjectQueryParams, CollisionShape, QueryParams);

		for (const FOverlapResult& Overlap : Overlaps)
		{
			AActor* HitActor = Overlap.GetActor();
			if (HitActor && HitActor != InstigatorActor && HitActor->GetClass() != InstigatorActor->GetClass())
			{
				OverlappedActors.AddUnique(HitActor);
			}
		}

#if ENABLE_DRAW_DEBUG
		if (DamageShapeParams.bShowDebugLines)
		{
			DrawDebugBox(World, ShapeCenter, DamageShapeParams.BoxExtent, Rotation.Quaternion(), FColor::Purple, false, 2.0f, 0, 2.0f);
		}
#endif
	}

	FGameplayEffectContextHandle ContextHandle = InstigatorASC->MakeEffectContext();
	ContextHandle.AddInstigator(InstigatorActor, InstigatorActor);

	for (AActor* HitActor : OverlappedActors)
	{
		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
		if (!TargetASC) continue;

		FGameplayEffectSpecHandle SpecHandle = InstigatorASC->MakeOutgoingSpec(DamageEffectClass, 1.f, ContextHandle);
		if (!SpecHandle.IsValid()) continue;

		UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(
			SpecHandle,
			FGameplayTag::RequestGameplayTag(FName("Data.Damage"), false),
			DamageShapeParams.DamageCoefficient
		);

		InstigatorASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
	}
}

void ULostArkGameplayAbility::SetupHitCheckListener()
{
	if (!DamageEffectClass) return;

	UAbilityTask_WaitGameplayEvent* HitCheckTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		FGameplayTag::RequestGameplayTag(FName("Gameplay.Event.HitCheck"), false),
		nullptr,
		false, // OnlyTriggerOnce = false (다단히트 허용)
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
