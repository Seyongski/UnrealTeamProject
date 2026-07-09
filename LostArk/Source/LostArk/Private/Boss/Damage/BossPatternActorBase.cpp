// Fill out your copyright notice in the Description page of Project Settings.


#include "Boss/Damage/BossPatternActorBase.h"
#include "Boss/Damage/BossAoeEffect.h"
#include "Boss/BossGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "ProceduralMeshComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "DrawDebugHelpers.h"
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

	// 본체 VFX (지정 시): 수명 내내 루트에 붙어 이동을 따라감 (예고와 무관, 즉발이어도 표시)
	SpawnBodyEffect();

	// 예고 비주얼 (즉발이 아닐 때만, 모든 머신에서 코스메틱으로 표시)
	// TelegraphEffect 지정 시 VFX로 대체, 아니면 기존 프로시저럴 메시
	if (!bInstant)
	{
		if (TelegraphEffect)
		{
			BuildTelegraphEffect();
		}
		else
		{
			BuildTelegraph();
		}
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
			Loc = GetFeetLocation(HomingTarget);	// 장판은 발밑 기준
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

	// AttackCenter.Z 를 바닥으로 스냅. 높이 게이트(|P.Z - AttackCenter.Z| <= HeightTolerance)를
	// 보스 캡슐 크기가 아니라 '바닥' 기준으로 만든다(캡슐을 키워도 판정이 안 흔들리게).
	// XY 도형 판정은 Z 를 무시하므로 영향 없음.
	if (UWorld* World = GetWorld())
	{
		FHitResult GroundHit;
		FCollisionQueryParams GParams(SCENE_QUERY_STAT(AoeCenterGround), false);
		if (Caster) { GParams.AddIgnoredActor(Caster); }
		GParams.AddIgnoredActor(this);
		const FVector Start = AttackCenter + FVector(0, 0, 200.f);
		const FVector End = AttackCenter - FVector(0, 0, 5000.f);
		if (World->LineTraceSingleByChannel(GroundHit, Start, End, ECC_Visibility, GParams))
		{
			AttackCenter.Z = GroundHit.ImpactPoint.Z;
		}
	}

	ShapeForward = GetActorForwardVector();
	ShapeForward.Z = 0.f;
	ShapeForward.Normalize();

	ShapeRight = GetActorRightVector();
	ShapeRight.Z = 0.f;
	ShapeRight.Normalize();

	// Straight 모드: 스폰 시 전방을 발사 방향으로 캐싱 (수평 등속 직진, 추적 없음)
	LaunchDirection = ShapeForward;

	// Spiral 모드: 나선 중심(직선 진행 위치)을 스폰 지점에서 시작
	SpiralBasePos = AttackCenter;
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
	ProcMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 시전자(거대 보스) 스케일이 액터에 실려오면 메시가 그만큼 커진다(판정은 월드 cm라 무관).
	// 스케일 1로 고정해 판정과 크기를 일치시킨다.
	ProcMesh->SetWorldScale3D(FVector::OneVector);

	// 바닥 스냅: 스폰 높이가 허리여도 메시를 바닥 위(+5cm)에 눕힌다. XY 는 판정 중심 그대로.
	FVector MeshCenter = AttackCenter;
	if (UWorld* World = GetWorld())
	{
		FHitResult GroundHit;
		FCollisionQueryParams GParams(SCENE_QUERY_STAT(TelegraphGround), false);
		if (Caster) { GParams.AddIgnoredActor(Caster); }
		GParams.AddIgnoredActor(this);
		const FVector TraceStart = MeshCenter + FVector(0, 0, 200.f);
		const FVector TraceEnd = MeshCenter - FVector(0, 0, 2000.f);
		if (World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility, GParams))
		{
			MeshCenter.Z = GroundHit.ImpactPoint.Z;
		}
	}
	ProcMesh->SetWorldLocation(MeshCenter + FVector(0, 0, 5.f));

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

void ABossPatternActorBase::BuildTelegraphEffect()
{
	if (!TelegraphEffect)
	{
		return;
	}

	// 루트에 부착 -> Follow/Homing/Spiral 등 이동 시 액터와 함께 자동으로 따라옴 (메시 텔레그래프와 동일 원리)
	UNiagaraComponent* NC = UNiagaraFunctionLibrary::SpawnSystemAttached(
		TelegraphEffect, GetRootComponent(), NAME_None,
		FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset,
		/*bAutoDestroy=*/false);

	if (NC)
	{
		ConfigureTelegraphEffect(NC);	// 도형별 크기/각도 파라미터 주입 (자식 구현)
	}

	WarningComp = NC;	// UNiagaraComponent 도 UPrimitiveComponent 라 HideTelegraph 가 그대로 정리
}

void ABossPatternActorBase::SpawnBodyEffect()
{
	if (!BodyEffect)
	{
		return;
	}

	// 루트에 부착 -> Straight/Homing 이동 시 액터와 함께 따라옴. 예고와 달리 수명 내내 유지.
	// bAutoDestroy=true: 액터가 Destroy 되면 나이아가라도 함께 정리(잔여 파티클은 자연 소멸).
	BodyComp = UNiagaraFunctionLibrary::SpawnSystemAttached(
		BodyEffect, GetRootComponent(), NAME_None,
		FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset,
		/*bAutoDestroy=*/true);
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

	case EAoeTargetingMode::FollowTarget:
		if (HomingTarget)
		{
			AttackCenter = GetFeetLocation(HomingTarget);
			SetActorLocation(AttackCenter);
		}
		break;

	case EAoeTargetingMode::Spiral:
		if (HomingTarget)
		{
			// 1) 직선 진행: 나선 중심을 타겟 쪽으로 전진 (Homing과 동일한 전진 로직)
			const FVector Goal = HomingTarget->GetActorLocation();
			SpiralBasePos = (HomingSpeed > 0.f)
				? FMath::VInterpConstantTo(SpiralBasePos, Goal, DeltaTime, HomingSpeed)
				: Goal;

			// 2) 나선 궤도: 진행 방향에 수직인 평면(Right/Up)에서 각도 누적 + 반지름 감쇠
			SpiralAngle += SpiralAngularSpeed * DeltaTime;

			FVector Dir = Goal - SpiralBasePos;
			Dir = Dir.IsNearlyZero() ? ShapeForward : Dir.GetSafeNormal();

			FVector Right = FVector::CrossProduct(FVector::UpVector, Dir);
			Right = Right.IsNearlyZero() ? ShapeRight : Right.GetSafeNormal();
			const FVector Up = FVector::CrossProduct(Dir, Right).GetSafeNormal();

			const float DistLeft = FVector::Dist(SpiralBasePos, Goal);
			const float RadiusNow = (SpiralConvergeDistance > KINDA_SMALL_NUMBER)
				? SpiralRadius * FMath::Clamp(DistLeft / SpiralConvergeDistance, 0.f, 1.f)
				: SpiralRadius;

			const float Rad = FMath::DegreesToRadians(SpiralAngle);
			const FVector OrbitOffset = (Right * FMath::Cos(Rad) + Up * FMath::Sin(Rad)) * RadiusNow;

			AttackCenter = SpiralBasePos + OrbitOffset;
			SetActorLocation(AttackCenter);
		}
		break;

	case EAoeTargetingMode::Straight:
		// 타겟 없이 스폰 방향으로 등속 직진 (수평). 추적하지 않음.
		if (HomingSpeed > 0.f)
		{
			AttackCenter += LaunchDirection * HomingSpeed * DeltaTime;
			SetActorLocation(AttackCenter);
		}
		break;

	case EAoeTargetingMode::Fixed:
	default:
		break;
	}
}

FVector ABossPatternActorBase::GetFeetLocation(const AActor* Target)
{
	if (!Target)
	{
		return FVector::ZeroVector;
	}

	FVector Loc = Target->GetActorLocation();
	if (const ACharacter* C = Cast<ACharacter>(Target))
	{
		if (const UCapsuleComponent* Capsule = C->GetCapsuleComponent())
		{
			Loc.Z -= Capsule->GetScaledCapsuleHalfHeight();
		}
	}
	return Loc;
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

	// 판정 도형을 눈으로 확인 (데칼 크기와 비교용)
	if (bDrawDebugHitShape)
	{
		DebugDrawShape();
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (!Pawn)
		{
			continue;
		}

		// 시전자(보스) 본인은 판정 제외 (자기 장판에 맞거나 잡히면 안 됨)
		if (Pawn == Caster)
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

		// 적중 확정: 첫 적중이면 보스에 결과 태그 부여 (Branch 조건용)
		if (!bCasterHitTagsApplied && !CasterTagsOnHit.IsEmpty())
		{
			bCasterHitTagsApplied = true;
			AddCasterLooseTags(CasterTagsOnHit);
		}

		ApplyEffectsTo(Pawn);
		AlreadyHitActors.Add(Pawn);
		OnAoeHitActor.Broadcast(Pawn);
	}
}

void ABossPatternActorBase::AddCasterLooseTags(const FGameplayTagContainer& InTags)
{
	if (InTags.IsEmpty())
	{
		return;
	}
	if (UAbilitySystemComponent* CasterASC =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Caster))
	{
		CasterASC->AddLooseGameplayTags(InTags);
	}
}

void ABossPatternActorBase::ApplyEffectsTo(AActor* Target)
{
	// 행동 오브젝트가 지정돼 있으면 위임(잡기/전하스왑 등), 없으면 기본 데미지+상태이상
	if (OnHitEffect)
	{
		OnHitEffect->OnHit(this, Target);
		return;
	}
	ApplyDamageAndStatus(Target);
}

void ABossPatternActorBase::ApplyDamageAndStatus(AActor* Target)
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

	// 행동 오브젝트가 파괴를 가로채면(잡기 유지 등) 여기서 멈춘다.
	// 이후 그 오브젝트가 스스로 DestroyAoeNow() 를 호출해 파괴 타이밍을 제어.
	if (OnHitEffect && OnHitEffect->OnFinish(this))
	{
		return;
	}

	DestroyAoeNow();
}

void ABossPatternActorBase::DestroyAoeNow()
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

void ABossPatternActorBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 해제 전에 외부 요인으로 파괴되면 행동 오브젝트가 안전 복구(잡은 대상 부착 해제 등)
	if (OnHitEffect)
	{
		OnHitEffect->OnEndPlay(this);
	}

	Super::EndPlay(EndPlayReason);
}
