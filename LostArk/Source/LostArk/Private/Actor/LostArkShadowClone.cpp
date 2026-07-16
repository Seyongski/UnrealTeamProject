#include "Actor/LostArkShadowClone.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayEffect.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Kismet/KismetSystemLibrary.h"
#include "DrawDebugHelpers.h"

ALostArkShadowClone::ALostArkShadowClone()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	MeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("MeshComponent"));
	RootComponent = MeshComponent;
	
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetCollisionProfileName(TEXT("NoCollision"));
	MeshComponent->SetCastShadow(true);

	WeaponMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMeshComponent"));
	WeaponMeshComponent->SetupAttachment(MeshComponent, TEXT("b_wp_1"));
	WeaponMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMeshComponent->SetCollisionProfileName(TEXT("NoCollision"));
	WeaponMeshComponent->SetCastShadow(true);

	ShadowMaterial = nullptr;
	CurrentDashSpeed = 0.f;
	CurrentDashDuration = 0.f;
	ElapsedDashTime = 0.f;
	DashDirection = FVector::ZeroVector;
	DashDirection = FVector::ZeroVector;
}

void ALostArkShadowClone::BeginPlay()
{
	Super::BeginPlay();
}

void ALostArkShadowClone::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (ElapsedDashTime < CurrentDashDuration)
	{
		ElapsedDashTime += DeltaTime;

		FVector CurrentLoc = GetActorLocation();
		FVector NewLoc = CurrentLoc + DashDirection * (CurrentDashSpeed * DeltaTime);

		FVector TraceStart = CurrentLoc + FVector(0.f, 0.f, 50.f);
		FVector TraceEnd = NewLoc + FVector(0.f, 0.f, 50.f);

		FHitResult WallHit;
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(this);
		if (GetInstigator())
		{
			Params.AddIgnoredActor(GetInstigator());
		}

		bool bHitWall = GetWorld()->LineTraceSingleByChannel(WallHit, TraceStart, TraceEnd, ECC_WorldStatic, Params);
		if (!bHitWall)
		{
			SetActorLocation(NewLoc);
		}
		else
		{
			FVector HitLoc = WallHit.ImpactPoint;
			HitLoc.Z = CurrentLoc.Z;
			SetActorLocation(HitLoc);
			ElapsedDashTime = CurrentDashDuration;
		}
	}
	else
	{
		SetActorTickEnabled(false);
	}
}

void ALostArkShadowClone::InitShadow(
	USkeletalMeshComponent* SourceMeshComp,
	USkeletalMeshComponent* SourceWeaponMeshComp,
	UAnimMontage* MontageToPlay,
	float Rate,
	float DashSpeed,
	float DashDuration,
	UAbilitySystemComponent* InInstigatorASC,
	TSubclassOf<UGameplayEffect> InDamageEffectClass,
	FDamageShapeParams InDamageShape)
{
	if (!SourceMeshComp) return;

	InstigatorASC = InInstigatorASC;
	DamageEffectClass = InDamageEffectClass;
	DamageShape = InDamageShape;

	MeshComponent->SetSkeletalMesh(SourceMeshComp->GetSkeletalMeshAsset());
	MeshComponent->SetAnimClass(SourceMeshComp->GetAnimClass());
	
	if (SourceWeaponMeshComp && WeaponMeshComponent)
	{
		WeaponMeshComponent->SetSkeletalMesh(SourceWeaponMeshComp->GetSkeletalMeshAsset());
	}

	if (ShadowMaterial)
	{
		const int32 NumMaterials = MeshComponent->GetNumMaterials();
		for (int32 i = 0; i < NumMaterials; ++i)
		{
			MeshComponent->SetMaterial(i, ShadowMaterial);
		}

		if (WeaponMeshComponent)
		{
			const int32 NumWeaponMaterials = WeaponMeshComponent->GetNumMaterials();
			for (int32 i = 0; i < NumWeaponMaterials; ++i)
			{
				WeaponMeshComponent->SetMaterial(i, ShadowMaterial);
			}
		}
	}

	if (DashSpeed > 0.f && DashDuration > 0.f)
	{
		CurrentDashSpeed = DashSpeed;
		CurrentDashDuration = DashDuration;
		ElapsedDashTime = 0.f;
		DashDirection = GetActorForwardVector();
		SetActorTickEnabled(true);
	}


	UAnimInstance* AnimInstance = MeshComponent->GetAnimInstance();
	if (AnimInstance && MontageToPlay)
	{
		AnimInstance->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);

		float Length = AnimInstance->Montage_Play(MontageToPlay, Rate);
		if (Length > 0.f)
		{
			FOnMontageEnded EndedDelegate;
			EndedDelegate.BindUObject(this, &ALostArkShadowClone::OnMontageEnded);
			AnimInstance->Montage_SetEndDelegate(EndedDelegate, MontageToPlay);

			GetWorldTimerManager().SetTimer(
				SelfDestructTimerHandle,
				this,
				&ALostArkShadowClone::SelfDestruct,
				Length,
				false
			);
		}
		else
		{
			Destroy();
		}
	}
	else
	{
		Destroy();
	}
}

void ALostArkShadowClone::ApplyCloneDamage(FVector Origin, FRotator Rotation)
{
	if (!InstigatorASC.IsValid() || !DamageEffectClass) return;

	AActor* InstigatorActor = InstigatorASC->GetOwner();
	if (!InstigatorActor) return;

	UWorld* World = GetWorld();
	if (!World) return;

	TArray<AActor*> OverlappedActors;
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(InstigatorActor);

	FVector ForwardDir = Rotation.Vector();
	ForwardDir.Z = 0.f;
	ForwardDir.Normalize();

	FVector ShapeCenter = Origin + ForwardDir * DamageShape.ForwardOffset;

	if (DamageShape.ShapeType == EDamageShape::Sphere || DamageShape.ShapeType == EDamageShape::Cone)
	{
		FCollisionShape CollisionShape;
		CollisionShape.SetCapsule(DamageShape.Radius, DamageShape.ZTolerance);

		TArray<FOverlapResult> Overlaps;
		World->OverlapMultiByObjectType(Overlaps, ShapeCenter, FQuat::Identity, ObjectQueryParams, CollisionShape, QueryParams);

		for (const FOverlapResult& Overlap : Overlaps)
		{
			AActor* HitActor = Overlap.GetActor();
			if (HitActor && HitActor != InstigatorActor && HitActor->GetClass() != InstigatorActor->GetClass())
			{
				FVector HitLocation = HitActor->GetActorLocation();
				if (FMath::Abs(HitLocation.Z - ShapeCenter.Z) > DamageShape.ZTolerance)
				{
					continue;
				}

				if (DamageShape.ShapeType == EDamageShape::Cone)
				{
					FVector2D DirToTarget((HitLocation.X - ShapeCenter.X), (HitLocation.Y - ShapeCenter.Y));
					DirToTarget.Normalize();
					
					FVector2D ForwardDir2D(ForwardDir.X, ForwardDir.Y);
					ForwardDir2D.Normalize();

					float DotP = FVector2D::DotProduct(ForwardDir2D, DirToTarget);
					float Angle = FMath::RadiansToDegrees(FMath::Acos(DotP));
					if (Angle <= DamageShape.ConeAngleDegrees * 0.5f)
					{
						OverlappedActors.AddUnique(HitActor);
					}
				}
				else
				{
					float DistSq2D = FVector::DistSquaredXY(ShapeCenter, HitLocation);
					if (DistSq2D <= FMath::Square(DamageShape.Radius))
					{
						OverlappedActors.AddUnique(HitActor);
					}
				}
			}
		}

#if ENABLE_DRAW_DEBUG
		if (DamageShape.bShowDebugLines)
		{
			FVector DrawCenter = ShapeCenter; 

			if (DamageShape.ShapeType == EDamageShape::Sphere)
			{
				DrawDebugCircle(World, DrawCenter, DamageShape.Radius, 32, FColor::Blue, false, 2.0f, 0, 2.0f, FVector(0,1,0), FVector(1,0,0), false);
			}
			else if (DamageShape.ShapeType == EDamageShape::Cone)
			{
				float HalfAngleRad = FMath::DegreesToRadians(DamageShape.ConeAngleDegrees * 0.5f);
				FVector LeftDir = ForwardDir.RotateAngleAxis(-DamageShape.ConeAngleDegrees * 0.5f, FVector::UpVector);
				FVector RightDir = ForwardDir.RotateAngleAxis(DamageShape.ConeAngleDegrees * 0.5f, FVector::UpVector);
				
				DrawDebugLine(World, DrawCenter, DrawCenter + LeftDir * DamageShape.Radius, FColor::Cyan, false, 2.0f, 0, 2.0f);
				DrawDebugLine(World, DrawCenter, DrawCenter + RightDir * DamageShape.Radius, FColor::Cyan, false, 2.0f, 0, 2.0f);
				
				const int32 ArcSegments = 16;
				float AngleStep = DamageShape.ConeAngleDegrees / ArcSegments;
				FVector PrevPoint = DrawCenter + LeftDir * DamageShape.Radius;
				for (int32 i = 1; i <= ArcSegments; ++i)
				{
					FVector NextDir = ForwardDir.RotateAngleAxis(-DamageShape.ConeAngleDegrees * 0.5f + (AngleStep * i), FVector::UpVector);
					FVector NextPoint = DrawCenter + NextDir * DamageShape.Radius;
					DrawDebugLine(World, PrevPoint, NextPoint, FColor::Cyan, false, 2.0f, 0, 2.0f);
					PrevPoint = NextPoint;
				}
			}
		}
#endif
	}
	else if (DamageShape.ShapeType == EDamageShape::Box)
	{
		FCollisionShape CollisionShape;
		FVector BoxExtent = DamageShape.BoxExtent;
		BoxExtent.Z = DamageShape.ZTolerance;
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
				if (FMath::Abs(HitLocation.Z - ShapeCenter.Z) <= DamageShape.ZTolerance)
				{
					OverlappedActors.AddUnique(HitActor);
				}
			}
		}

#if ENABLE_DRAW_DEBUG
		if (DamageShape.bShowDebugLines)
		{
			FVector FlatBoxExtent = BoxExtent;
			FlatBoxExtent.Z = 1.0f;
			DrawDebugBox(World, ShapeCenter, FlatBoxExtent, FlatRotation.Quaternion(), FColor::Purple, false, 2.0f, 0, 2.0f);
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
			DamageShape.DamageCoefficient
		);

		InstigatorASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
	}
}

void ALostArkShadowClone::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	SelfDestruct();
}

void ALostArkShadowClone::SelfDestruct()
{
	GetWorldTimerManager().ClearTimer(SelfDestructTimerHandle);
	Destroy();
}




