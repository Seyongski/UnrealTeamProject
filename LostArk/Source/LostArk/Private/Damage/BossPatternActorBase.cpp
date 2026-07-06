// Fill out your copyright notice in the Description page of Project Settings.


#include "Damage/BossPatternActorBase.h"
#include "Boss/BossGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "ProceduralMeshComponent.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"

ABossPatternActorBase::ABossPatternActorBase()
{
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// 서버 권위: 액터는 복제하되 판정/GE는 서버에서만
	bReplicates = true;
	SetReplicateMovement(true);	// Follow/Homing 이동을 클라에 전파(예고 비주얼 위치 동기화)

	// 공격력계수 기본 전달 태그 (에디터에서 변경 가능)
	DamageSetByCallerTag = LostArkTags::Data_Damage;
}

void ABossPatternActorBase::InitAoe(AActor* InCaster, AActor* InTarget, float InDamageCoefficient)
{
	Caster = InCaster;
	HomingTarget = InTarget;
	DamageCoefficient = InDamageCoefficient;
	SetInstigator(Cast<APawn>(InCaster));
}

void ABossPatternActorBase::ApplyCommonOverride(const FBossAoeCommonOverride& Override)
{
	CastTime = Override.CastTime;
	Duration = Override.Duration;
	TickInterval = Override.TickInterval;
	HeightTolerance = Override.HeightTolerance;
	bSingleHitPerTarget = Override.bSingleHitPerTarget;
	bInstant = Override.bInstant;
	TargetingMode = Override.TargetingMode;
}

void ABossPatternActorBase::BeginPlay()
{
	Super::BeginPlay();

	// 스폰 원점 해석 (위치/회전 확정 + Forward/Right 캐싱)
	ResolveOrigin();

	// 예고 비주얼 (즉발이 아닐 때만, 모든 머신에서 코스메틱으로 표시)
	if (!bInstant)
	{
		BuildTelegraph();
	}

	OnAoeBegin.Broadcast();

	// 시전시간 후 첫 판정. CastTime 0이면 다음 프레임에 즉시.
	GetWorldTimerManager().SetTimer(
		CastTimerHandle, this, &ABossPatternActorBase::OnCastFinished,
		FMath::Max(CastTime, KINDA_SMALL_NUMBER), false);
}

void ABossPatternActorBase::ResolveOrigin()
{
	FVector Loc = GetActorLocation();
	FRotator Rot = GetActorRotation();

	switch (SpawnOrigin)
	{
	case EAoeSpawnOrigin::CasterLocation:
		if (Caster)
		{
			Loc = Caster->GetActorLocation();
			Rot = Caster->GetActorRotation();
		}
		break;

	case EAoeSpawnOrigin::CasterSocket:
		Loc = GetCasterOriginLocation();
		if (Caster)
		{
			Rot = Caster->GetActorRotation();
		}
		break;

	case EAoeSpawnOrigin::TargetLocation:
		if (HomingTarget)
		{
			Loc = HomingTarget->GetActorLocation();
		}
		break;

	case EAoeSpawnOrigin::GroundUnderTarget:
	{
		const FVector Base = HomingTarget ? HomingTarget->GetActorLocation() : Loc;
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(AoeGroundTrace), false);
		if (Caster) { Params.AddIgnoredActor(Caster); }
		if (HomingTarget) { Params.AddIgnoredActor(HomingTarget); }
		Params.AddIgnoredActor(this);
		const FVector Start = Base + FVector(0, 0, 200.f);
		const FVector End = Base - FVector(0, 0, 2000.f);
		if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
		{
			Loc = Hit.ImpactPoint;
		}
		else
		{
			Loc = Base;
		}
		break;
	}

	case EAoeSpawnOrigin::SpawnTransform:
	default:
		// 스폰 시 넘겨준 트랜스폼 유지
		break;
	}

	SetActorLocationAndRotation(Loc, Rot);
	AttackCenter = Loc;

	ShapeForward = GetActorForwardVector();
	ShapeForward.Z = 0.f;
	ShapeForward.Normalize();

	ShapeRight = GetActorRightVector();
	ShapeRight.Z = 0.f;
	ShapeRight.Normalize();
}

FVector ABossPatternActorBase::GetCasterOriginLocation() const
{
	if (const ACharacter* C = Cast<ACharacter>(Caster))
	{
		if (const USkeletalMeshComponent* Mesh = C->GetMesh())
		{
			if (SpawnSocketName != NAME_None && Mesh->DoesSocketExist(SpawnSocketName))
			{
				return Mesh->GetSocketLocation(SpawnSocketName);
			}
		}
	}
	return Caster ? Caster->GetActorLocation() : AttackCenter;
}

UProceduralMeshComponent* ABossPatternActorBase::CreateTelegraphMesh(
	const TArray<FVector>& Vertices, const TArray<int32>& Triangles)
{
	UProceduralMeshComponent* ProcMesh = NewObject<UProceduralMeshComponent>(this);
	ProcMesh->RegisterComponent();
	ProcMesh->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	ProcMesh->SetRelativeLocation(FVector(0.f, 0.f, 5.f));
	ProcMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FColor> Colors;
	TArray<FProcMeshTangent> Tangents;
	Normals.Init(FVector::UpVector, Vertices.Num());

	ProcMesh->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, Colors, Tangents, false);

	if (WarningMaterial)
	{
		ProcMesh->SetMaterial(0, WarningMaterial);
	}

	WarningComp = ProcMesh;
	return ProcMesh;
}

void ABossPatternActorBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateCenter(DeltaTime);
}

void ABossPatternActorBase::UpdateCenter(float DeltaTime)
{
	switch (TargetingMode)
	{
	case EAoeTargetingMode::Follow:
		if (Caster)
		{
			AttackCenter = GetCasterOriginLocation();
			SetActorLocationAndRotation(AttackCenter, Caster->GetActorRotation());
			ShapeForward = GetActorForwardVector(); ShapeForward.Z = 0.f; ShapeForward.Normalize();
			ShapeRight = GetActorRightVector();     ShapeRight.Z = 0.f;   ShapeRight.Normalize();
		}
		break;

	case EAoeTargetingMode::Homing:
		if (HomingTarget)
		{
			const FVector Goal = HomingTarget->GetActorLocation();
			AttackCenter = (HomingSpeed > 0.f)
				? FMath::VInterpConstantTo(AttackCenter, Goal, DeltaTime, HomingSpeed)
				: Goal;
			SetActorLocation(AttackCenter);
		}
		break;

	case EAoeTargetingMode::Fixed:
	default:
		break;
	}
}

void ABossPatternActorBase::OnCastFinished()
{
	HideTelegraph();

	// 첫 판정
	PerformHitCheck();

	// 유지형이면 틱뎀 반복 + 수명 타이머, 아니면 즉시 종료
	if (Duration > KINDA_SMALL_NUMBER && TickInterval > KINDA_SMALL_NUMBER)
	{
		GetWorldTimerManager().SetTimer(
			TickTimerHandle, this, &ABossPatternActorBase::PerformHitCheck,
			TickInterval, true);

		GetWorldTimerManager().SetTimer(
			LifeTimerHandle, this, &ABossPatternActorBase::FinishAoe,
			Duration, false);
	}
	else
	{
		FinishAoe();
	}
}

void ABossPatternActorBase::PerformHitCheck()
{
	// 판정/데미지는 서버 권위에서만
	if (!HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (!Pawn)
		{
			continue;
		}

		// 한번만타격: 이미 맞은 대상 스킵
		if (bSingleHitPerTarget && AlreadyHitActors.Contains(Pawn))
		{
			continue;
		}

		if (!IsAlive(Pawn))
		{
			continue;
		}

		const FVector P = Pawn->GetActorLocation();

		// 높이(Z) 체크
		if (FMath::Abs(P.Z - AttackCenter.Z) > HeightTolerance)
		{
			continue;
		}

		// 도형 판정 (자식 구현)
		if (!IsInsideShape(P))
		{
			continue;
		}

		ApplyEffectsTo(Pawn);
		AlreadyHitActors.Add(Pawn);
		OnAoeHitActor.Broadcast(Pawn);
	}
}

void ABossPatternActorBase::ApplyEffectsTo(AActor* Target)
{
	UAbilitySystemComponent* TargetASC =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
	if (!TargetASC)
	{
		return;
	}

	UAbilitySystemComponent* SourceASC =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Caster);

	// 데미지 GE
	if (DamageEffect && SourceASC)
	{
		FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
		Context.AddSourceObject(this);
		Context.AddInstigator(Caster, this);

		FGameplayEffectSpecHandle Spec =
			SourceASC->MakeOutgoingSpec(DamageEffect, 1.f, Context);
		if (Spec.IsValid())
		{
			if (DamageSetByCallerTag.IsValid())
			{
				Spec.Data->SetSetByCallerMagnitude(DamageSetByCallerTag, DamageCoefficient);
			}
			SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data, TargetASC);
		}
	}

	// 상태이상 GE들 (소스 ASC 없으면 타겟 자기 자신을 소스로)
	UAbilitySystemComponent* StatusSource = SourceASC ? SourceASC : TargetASC;
	for (const TSubclassOf<UGameplayEffect>& StatusGE : StatusEffects)
	{
		if (!StatusGE)
		{
			continue;
		}
		FGameplayEffectContextHandle Context = StatusSource->MakeEffectContext();
		Context.AddSourceObject(this);
		FGameplayEffectSpecHandle Spec =
			StatusSource->MakeOutgoingSpec(StatusGE, 1.f, Context);
		if (Spec.IsValid())
		{
			StatusSource->ApplyGameplayEffectSpecToTarget(*Spec.Data, TargetASC);
		}
	}
}

void ABossPatternActorBase::HideTelegraph()
{
	if (WarningComp)
	{
		WarningComp->SetVisibility(false);
		WarningComp->DestroyComponent();
		WarningComp = nullptr;
	}
}

bool ABossPatternActorBase::IsAlive(const AActor* Target) const
{
	const UAbilitySystemComponent* ASC =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(const_cast<AActor*>(Target));
	if (!ASC)
	{
		return true; // 판정 불가 시 대상 포함
	}

	const FGameplayTag Tag = DeadTag.IsValid() ? DeadTag : LostArkTags::State_Dead;
	return !ASC->HasMatchingGameplayTag(Tag);
}

void ABossPatternActorBase::FinishAoe()
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;

	GetWorldTimerManager().ClearTimer(TickTimerHandle);
	GetWorldTimerManager().ClearTimer(LifeTimerHandle);

	OnAoeEnd.Broadcast();
	Destroy();
}
