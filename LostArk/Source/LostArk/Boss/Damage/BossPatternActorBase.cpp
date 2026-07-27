// Fill out your copyright notice in the Description page of Project Settings.


#include "Boss/Damage/BossPatternActorBase.h"
#include "Boss/Damage/BossAoeEffect.h"
#include "Boss/BossGameplayTags.h"
#include "Boss/Combat/BossCombatStatics.h"
#include "Boss/Gimmick/BossGimmickTower.h"
#include "Boss/Raid/BossRaidGameState.h"
#include "EngineUtils.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "ProceduralMeshComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "Net/UnrealNetwork.h"

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

void ABossPatternActorBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 스폰 노티파이/오버라이드가 서버에서 런타임으로 주입하는 값들 -> 클라 예고 비주얼이
	// 서버와 같게 그려지도록 복제 (미복제 시 클라는 클래스 기본값으로 그려 모양/타이밍이 어긋남).
	//
	// [규칙] 새 도형/예고 파생 클래스를 만들 때, '스폰 시 노티파이/Setup 이 런타임으로 대입하는'
	//        비주얼/타이밍 파라미터는 반드시 이렇게 Replicated + DOREPLIFETIME 해야 한다.
	//        (도형별 Radius/각도 등은 각 파생 클래스에서 replicate — Sector/Rect/Circle 참고.
	//         저스트가드 등은 이 도형들 + bTelegraphFill 을 쓰므로 여기서 이미 커버됨.)
	//        순수 EditDefaultsOnly(런타임에 안 바뀜)면 CDO 로 동일하니 복제 불필요.
	DOREPLIFETIME(ABossPatternActorBase, CastTime);
	DOREPLIFETIME(ABossPatternActorBase, Duration);
	DOREPLIFETIME(ABossPatternActorBase, bInstant);
	DOREPLIFETIME(ABossPatternActorBase, bKeepTelegraphWhileActive);
	DOREPLIFETIME(ABossPatternActorBase, bTelegraphFill);
	DOREPLIFETIME(ABossPatternActorBase, TargetingMode);
	DOREPLIFETIME(ABossPatternActorBase, HomingSpeed);	// Straight/Homing/Spiral 이동속도 (SetupStraightProjectile 런타임 주입)
	DOREPLIFETIME(ABossPatternActorBase, ShapeOffset);	// 중심 오프셋 (스폰 노티파이 런타임 주입, Follow 모드는 클라도 매 틱 재적용)
}

ABossPatternActorBase* ABossPatternActorBase::SpawnAoeDeferred(UWorld* World,
	TSubclassOf<ABossPatternActorBase> AoeClass, const FTransform& SpawnTM,
	AActor* SpawnOwner, AActor* Caster, AActor* Target, float DamageCoefficient,
	TFunctionRef<void(ABossPatternActorBase&)> Configure)
{
	if (!World || !AoeClass)
	{
		return nullptr;
	}

	ABossPatternActorBase* Aoe = World->SpawnActorDeferred<ABossPatternActorBase>(
		AoeClass, SpawnTM, SpawnOwner, Cast<APawn>(Caster),
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Aoe)
	{
		return nullptr;
	}

	Aoe->InitAoe(Caster, Target, DamageCoefficient);
	Configure(*Aoe);
	Aoe->FinishSpawning(SpawnTM);
	return Aoe;
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
	bKeepTelegraphWhileActive = Override.bKeepTelegraphWhileActive;
	bTelegraphFill = Override.bTelegraphFill;
	TargetingMode = Override.TargetingMode;
}

void ABossPatternActorBase::SetupStraightProjectile(float Speed, float Range, float HitInterval, float InCastTime)
{
	// BP 클래스 설정(SpawnOrigin/TargetingMode)이 무엇이든 '스폰 트랜스폼 방향 직진'을 보장.
	// SpawnOrigin 이 CasterLocation 등이면 ResolveOrigin 이 위치/회전을 보스 것으로 덮어써
	// 방사형 부채가 전부 같은 방향으로 뭉개지므로 반드시 SpawnTransform 으로 고정한다.
	TargetingMode = EAoeTargetingMode::Straight;
	SpawnOrigin = EAoeSpawnOrigin::SpawnTransform;

	HomingSpeed = FMath::Max(Speed, 1.f);
	Duration = FMath::Max(Range, 1.f) / HomingSpeed;	// 사거리 소진 = 수명 종료
	TickInterval = FMath::Max(HitInterval, 0.02f);		// 비행 중 판정 주기
	CastTime = FMath::Max(InCastTime, 0.f);
	bInstant = (CastTime <= KINDA_SMALL_NUMBER);		// 대기 없으면 예고 스킵하고 즉시 발사
}

void ABossPatternActorBase::SetBodyEffectOverride(UNiagaraSystem* InNiagara, UParticleSystem* InCascade)
{
	// BeginPlay(SpawnBodyEffect) 전에 호출되어야 반영됨. null 인자는 BP 기본값 유지.
	if (InNiagara)
	{
		BodyEffect = InNiagara;
	}
	if (InCascade)
	{
		BodyEffectCascade = InCascade;
	}
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

	// 차오르는 예고: 시작 시각 기록 + 첫 프레임부터 0 근처에서 시작 (이후 Tick이 진행률 갱신)
	CastStartTimeSeconds = GetWorld()->GetTimeSeconds();
	UpdateTelegraphFill();

	OnAoeBegin.Broadcast();

	// 시전시간 후 첫 판정. CastTime 0이면 다음 프레임에 즉시.
	GetWorldTimerManager().SetTimer(
		CastTimerHandle, this, &ABossPatternActorBase::OnCastFinished,
		FMath::Max(CastTime, KINDA_SMALL_NUMBER), false);
}

void ABossPatternActorBase::ResolveOrigin()
{
	// 클라: 서버가 스폰 직후 BeginPlay 에서 원점+바닥Z 스냅까지 끝낸 위치가 이미 복제돼 있다.
	// 여기서 바닥 Z 를 재계산하면(플레이어 발밑 등 클라마다 다를 수 있는 폴백) 서버와 어긋나
	// 데칼/예고가 딴 데 뜬다 -> 판정은 서버 전용이므로 클라는 복제된 트랜스폼을 그대로 채택만 한다.
	if (!HasAuthority())
	{
		AttackCenter = GetActorLocation();
		CacheShapeAxes();
		LaunchDirection = ShapeForward;
		SpiralBasePos = AttackCenter;
		UE_LOG(LogTemp, Warning, TEXT("[Aoe] %s ResolveOrigin(클라): 위치=(%.1f,%.1f,%.1f) Yaw=%.1f Fwd=(%.2f,%.2f)"),
			*GetName(), AttackCenter.X, AttackCenter.Y, AttackCenter.Z,
			GetActorRotation().Yaw, ShapeForward.X, ShapeForward.Y);
		return;
	}

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
		// 타겟 위치 기준. 바닥 Z 는 아래 공통 스냅(ResolveGroundZ)이 처리하므로 별도 트레이스 불필요
		if (HomingTarget)
		{
			Loc = HomingTarget->GetActorLocation();
		}
		break;

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
	AttackCenter.Z = ResolveGroundZ(AttackCenter);
	SetActorLocation(AttackCenter);	// 액터/부착 메시(예고·본체)도 바닥으로 내려 함께 스냅

	CacheShapeAxes();

	// 원점 해석이 끝난 '뒤에' 중심 오프셋 적용 -> SpawnOrigin 이 무엇이든(CasterLocation 등으로
	// 위치를 덮어써도) 좌/우·전후 배치가 살아남는다. 아래 LaunchDirection/SpiralBasePos 가
	// 옮겨진 중심을 기준으로 잡히도록 이 순서를 지킬 것.
	ApplyShapeOffset();

	// Straight 모드: 스폰 시 전방을 발사 방향으로 캐싱 (수평 등속 직진, 추적 없음)
	LaunchDirection = ShapeForward;

	// Spiral 모드: 나선 중심(직선 진행 위치)을 스폰 지점에서 시작
	SpiralBasePos = AttackCenter;

	UE_LOG(LogTemp, Warning, TEXT("[Aoe] %s ResolveOrigin(서버): 위치=(%.1f,%.1f,%.1f) Yaw=%.1f Fwd=(%.2f,%.2f)"),
		*GetName(), AttackCenter.X, AttackCenter.Y, AttackCenter.Z,
		GetActorRotation().Yaw, ShapeForward.X, ShapeForward.Y);
}

bool ABossPatternActorBase::TraceGroundZ(const FVector& At, float& OutZ) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// bTraceComplex=true: 머지된 바닥 메시(SM_MERGED_*)는 심플 콜리전이 없고 복합(트라이앵글)
	// 콜리전만 있는 경우가 많다. 기본 라인트레이스는 심플만 검사하므로 이걸 켜야 실제 바닥을 맞춘다.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AoeGroundTrace), /*bTraceComplex=*/true);
	if (Caster) { Params.AddIgnoredActor(Caster); }
	Params.AddIgnoredActor(this);

	// 채널 응답이 아니라 '오브젝트 타입'으로 질의 -> 바닥이 Visibility 를 Block 안 해도 잡힘.
	// 걸을 수 있는 바닥은 대부분 WorldStatic, 이동 플랫폼 등은 WorldDynamic.
	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	// 위에서 아래로 훑는다. 시작 높이를 GroundTraceStartHeight 로 넉넉히 잡아, 보스 캡슐이
	// 지형 아래로 잠겨 기준 Z(At) 가 바닥보다 낮은 맵에서도 시작점이 바닥 위가 되게 한다.
	// (시작점이 바닥 밑이면 아래로 쏴봐야 바닥을 못 맞춰 데칼이 맵 아래에 찍힌다)
	const FVector Start = At + FVector(0.f, 0.f, GroundTraceStartHeight);
	const FVector End = At - FVector(0.f, 0.f, 5000.f);

	FHitResult Hit;
	if (World->LineTraceSingleByObjectType(Hit, Start, End, ObjParams, Params))
	{
		// 뭘 잡았는지 1회 로그 -> 엉뚱한 지오메트리(투명 볼륨/부착 액터 등)를 잡는 문제 진단용
		if (!bGroundTraceLogged)
		{
			bGroundTraceLogged = true;
			UE_LOG(LogTemp, Warning, TEXT("[Aoe] %s 바닥 트레이스 적중: 액터=%s 컴포넌트=%s ImpactZ=%.1f (기준 Z=%.1f)"),
				*GetName(), *GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()),
				Hit.ImpactPoint.Z, At.Z);
		}
		OutZ = Hit.ImpactPoint.Z;
		return true;
	}

	return false;	// 실패 시 로그는 ResolveGroundZ 가 폴백 수치와 함께 남긴다
}

float ABossPatternActorBase::ResolveGroundZ(const FVector& At) const
{
	// 절대 Z 고정: 트레이스/발밑 무시하고 지정한 월드 Z 를 그대로 바닥으로 사용 (가장 확실)
	if (bUseAbsoluteGroundZ)
	{
		if (!bGroundTraceLogged)
		{
			bGroundTraceLogged = true;
			UE_LOG(LogTemp, Warning, TEXT("[Aoe] %s 절대 바닥 Z 사용: %.1f"), *GetName(), AbsoluteGroundZ);
		}
		return AbsoluteGroundZ;
	}

	// 강제 폴백: 트레이스가 이상한 걸 잡을 때 수동 제어 (오프셋이 반드시 적용됨)
	float TracedZ = 0.f;
	if (!bForceGroundFallback && TraceGroundZ(At, TracedZ))
	{
		return TracedZ;
	}

	// 폴백 1순위: 아레나 바닥 높이의 단일 진실 = GameState.ArenaFloorZ (디자이너가 GameState BP에 지정).
	// 0 이면 미설정으로 보고 다음 폴백으로 넘어간다.
	// (각 BP 에 절대 Z 를 박지 않아도 이 한 값만 맞추면 모든 장판이 따라옴)
	if (const ABossRaidGameState* GS = GetWorld() ? GetWorld()->GetGameState<ABossRaidGameState>() : nullptr)
	{
		if (!FMath::IsNearlyZero(GS->ArenaFloorZ))
		{
			const float ArenaZ = GS->ArenaFloorZ + GroundFallbackZOffset;
			if (!bGroundTraceLogged)
			{
				bGroundTraceLogged = true;
				UE_LOG(LogTemp, Warning, TEXT("[Aoe] %s 폴백 Z(아레나): ArenaFloorZ=%.1f + 오프셋=%.1f => %.1f"),
					*GetName(), GS->ArenaFloorZ, GroundFallbackZOffset, ArenaZ);
			}
			return ArenaZ;
		}
	}

	// 폴백 2순위: 생존 플레이어 발밑 Z. 플레이어는 실제 아레나 바닥에 서 있으므로,
	// 보스가 구덩이에 잠겨 시전자 발밑/트레이스가 바닥을 못 줄 때 이게 진짜 바닥이다.
	{
		float PlayerZ = 0.f;
		if (TryGetPlayerGroundZ(At, PlayerZ))
		{
			const float FinalZ = PlayerZ + GroundFallbackZOffset;
			if (!bGroundTraceLogged)
			{
				bGroundTraceLogged = true;
				UE_LOG(LogTemp, Warning, TEXT("[Aoe] %s 폴백 Z(플레이어 발밑): 발밑=%.1f + 오프셋=%.1f => %.1f"),
					*GetName(), PlayerZ, GroundFallbackZOffset, FinalZ);
			}
			return FinalZ;
		}
	}

	// 폴백 3순위: 시전자(보스) 발밑 Z + 수동 보정 (플레이어도 못 찾을 때)
	if (Caster)
	{
		const float FeetZ = GetFeetLocation(Caster).Z;
		const float FinalZ = FeetZ + GroundFallbackZOffset;
		if (!bGroundTraceLogged)
		{
			bGroundTraceLogged = true;
			UE_LOG(LogTemp, Warning, TEXT("[Aoe] %s 폴백 Z(시전자 발밑): 발밑=%.1f + 오프셋=%.1f => %.1f"),
				*GetName(), FeetZ, GroundFallbackZOffset, FinalZ);
		}
		return FinalZ;
	}

	return At.Z;
}

bool ABossPatternActorBase::TryGetPlayerGroundZ(const FVector& At, float& OutZ) const
{
	TArray<APawn*> PlayerPawns;
	UBossCombatStatics::GetPlayerPawns(GetWorld(), PlayerPawns);

	float BestDistSq = TNumericLimits<float>::Max();
	bool bFound = false;
	for (const APawn* Pawn : PlayerPawns)
	{
		if (!Pawn || !IsAlive(Pawn))
		{
			continue;
		}
		// XY 로 가장 가까운 생존 플레이어 (경사/단차 있어도 이 장판 근처 바닥을 대표)
		const FVector Feet = GetFeetLocation(Pawn);
		const float DistSq = FVector::DistSquaredXY(At, Pawn->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			OutZ = Feet.Z;
			bFound = true;
		}
	}
	return bFound;
}

void ABossPatternActorBase::ApplyShapeOffset()
{
	if (ShapeOffset.IsNearlyZero())
	{
		return;
	}

	// 도형 로컬축(평면 Forward/Right) 기준으로 밀기. Z 는 이미 바닥으로 스냅돼 있으므로 유지.
	AttackCenter += ShapeForward * ShapeOffset.X + ShapeRight * ShapeOffset.Y;
	SetActorLocation(AttackCenter);	// 부착된 예고 메시/본체 VFX 도 함께 이동
}

void ABossPatternActorBase::CacheShapeAxes()
{
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
	ProcMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 시전자(거대 보스) 스케일이 액터에 실려오면 메시가 그만큼 커진다(판정은 월드 cm라 무관).
	// 스케일 1로 고정해 판정과 크기를 일치시킨다.
	ProcMesh->SetWorldScale3D(FVector::OneVector);

	// 바닥 스냅: AttackCenter 는 ResolveOrigin 에서 이미 바닥으로 스냅됐으므로 그 Z 를 그대로 쓴다.
	// (예고 표시 유지 패턴에서 액터가 Follow 로 이동하면 부착된 메시가 자동으로 함께 따라감)
	FVector MeshCenter = AttackCenter;
	MeshCenter.Z = AttackCenter.Z + TelegraphZOffset;
	ProcMesh->SetWorldLocation(MeshCenter);

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

UProceduralMeshComponent* ABossPatternActorBase::CreateArcTelegraphMesh(float StartAngleDeg, float EndAngleDeg,
	float InInnerRadius, float InOuterRadius, int32 Segments)
{
	// 로컬 좌표(X=Forward, Y=Right). 각도 A 방향 = (cos A, sin A). 메시 자체가 도형이라
	// 머티리얼 마스킹이 필요 없다. 원은 0~360°, 부채꼴은 Start~End 구간.
	TArray<FVector> Vertices;
	TArray<int32> Triangles;

	if (InInnerRadius > KINDA_SMALL_NUMBER)
	{
		// 도넛/환형 부채꼴: 안쪽 호 + 바깥 호를 스트립으로 연결
		for (int32 i = 0; i <= Segments; ++i)
		{
			const float Rad = FMath::DegreesToRadians(
				FMath::Lerp(StartAngleDeg, EndAngleDeg, (float)i / Segments));
			const float Cos = FMath::Cos(Rad);
			const float Sin = FMath::Sin(Rad);
			Vertices.Add(FVector(Cos * InInnerRadius, Sin * InInnerRadius, 0.f));
			Vertices.Add(FVector(Cos * InOuterRadius, Sin * InOuterRadius, 0.f));
		}
		for (int32 i = 0; i < Segments; ++i)
		{
			const int32 i0 = i * 2, i1 = i * 2 + 1, i2 = i * 2 + 2, i3 = i * 2 + 3;
			Triangles.Add(i0); Triangles.Add(i1); Triangles.Add(i3);
			Triangles.Add(i0); Triangles.Add(i3); Triangles.Add(i2);
		}
	}
	else
	{
		// 꽉 찬 원/부채꼴: 중심 팬
		Vertices.Add(FVector::ZeroVector);
		for (int32 i = 0; i <= Segments; ++i)
		{
			const float Rad = FMath::DegreesToRadians(
				FMath::Lerp(StartAngleDeg, EndAngleDeg, (float)i / Segments));
			Vertices.Add(FVector(FMath::Cos(Rad) * InOuterRadius, FMath::Sin(Rad) * InOuterRadius, 0.f));
		}
		for (int32 i = 0; i < Segments; ++i)
		{
			Triangles.Add(0); Triangles.Add(i + 1); Triangles.Add(i + 2);
		}
	}

	return CreateTelegraphMesh(Vertices, Triangles);
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
	// 루트에 부착 -> Straight/Homing 이동 시 액터와 함께 따라옴. 예고와 달리 수명 내내 유지.
	// bAutoDestroy=true: 액터가 Destroy 되면 파티클도 함께 정리(잔여 파티클은 자연 소멸).
	if (BodyEffect)
	{
		BodyComp = UNiagaraFunctionLibrary::SpawnSystemAttached(
			BodyEffect, GetRootComponent(), NAME_None,
			FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset,
			/*bAutoDestroy=*/true);

		if (BodyComp)
		{
			// 본체 VFX 가 스스로 연출 타이밍을 전환할 수 있게 공통 타이밍 주입.
			// 예: 전기 장벽 — 구체 2개는 즉시, 그 사이 전기 빔은 CastTime 경과(폭발) 후 Duration 동안.
			BodyComp->SetFloatParameter(TEXT("CastTime"), CastTime);
			BodyComp->SetFloatParameter(TEXT("Duration"), Duration);

			// 도형 파라미터(Rect 의 HalfLength/ForwardOffset 등)도 예고와 동일 훅으로 주입
			// -> 구체를 장판 양 끝(ForwardOffset±HalfLength)에 배치하는 식으로 크기를 판정과 일치시킨다.
			ConfigureTelegraphEffect(BodyComp);
		}
	}

	// 캐스케이드 본체 (토네이도 등 기존 UParticleSystem 에셋용)
	if (BodyEffectCascade)
	{
		BodyCascadeComp = UGameplayStatics::SpawnEmitterAttached(
			BodyEffectCascade, GetRootComponent(), NAME_None,
			FVector::ZeroVector, FRotator::ZeroRotator, FVector::OneVector,
			EAttachLocation::KeepRelativeOffset,
			/*bAutoDestroy=*/true);
	}
}

void ABossPatternActorBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateCenter(DeltaTime);
	UpdateTelegraphFill();
}

void ABossPatternActorBase::UpdateTelegraphFill()
{
	if (!bTelegraphFill || !WarningComp)
	{
		return;
	}

	// CastTime 진행률. 시전이 끝났는데 예고가 남아있으면(유지 표시) 가득 찬 상태로 고정
	float Ratio = 1.f;
	if (!bCastFinished && CastTime > KINDA_SMALL_NUMBER)
	{
		const float Elapsed = GetWorld()->GetTimeSeconds() - CastStartTimeSeconds;
		Ratio = FMath::Clamp(Elapsed / CastTime, 0.f, 1.f);
	}

	if (UNiagaraComponent* NC = Cast<UNiagaraComponent>(WarningComp))
	{
		// VFX 예고는 지오메트리 스케일 대신 파라미터로 전달 (시스템이 반경/알파 등에 소비)
		NC->SetFloatParameter(TEXT("FillRatio"), Ratio);
	}
	else
	{
		// 프로시저럴 메시: 중앙(액터 원점 = 시전 중앙)에서 바깥으로 XY 확장.
		// 판정 크기는 불변 — 도형 판정은 T에 원래 크기로 수행되고, 이건 순수 비주얼.
		// 스케일 0은 트랜스폼이 퇴화하므로 바닥값을 둔다.
		const float S = FMath::Max(Ratio, 0.02f);
		WarningComp->SetWorldScale3D(FVector(S, S, 1.f));
	}
}

void ABossPatternActorBase::UpdateCenter(float DeltaTime)
{
	switch (TargetingMode)
	{
	case EAoeTargetingMode::Follow:
		if (Caster)
		{
			AttackCenter = GetCasterOriginLocation();
			// 시전자(보스) XY·회전은 따라가되 Z 는 바닥으로 스냅 -> 회전 장판이 허공에 안 뜬다
			AttackCenter.Z = ResolveGroundZ(AttackCenter);
			SetActorLocationAndRotation(AttackCenter, Caster->GetActorRotation());
			CacheShapeAxes();
			// 중심을 시전자에서 새로 잡았으므로 오프셋도 다시 얹는다 (누적이 아니라 매 틱 재계산이라 안전).
			// 보스가 회전하면 축도 함께 돌아 좌/우 배치가 정면 기준을 유지한다.
			ApplyShapeOffset();
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
			ApplyShapeOffset();	// 타겟 위치에서 새로 잡은 중심에 오프셋 재적용 (Follow 와 동일)
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
		// 예고(CastTime) 중에는 제자리 대기, 시전이 끝나야 발사된다.
		if (bCastFinished && HomingSpeed > 0.f)
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
	bCastFinished = true;	// Straight 투사체는 이때부터 전진 시작

	// 유지 표시 패턴(회전 장판 등)은 예고를 남겨 위험지대를 계속 보여준다.
	// (유지시간이 없으면 남길 이유가 없으므로 기존대로 제거. 액터 소멸 시 컴포넌트도 함께 정리됨)
	const bool bKeepTelegraph = bKeepTelegraphWhileActive && Duration > KINDA_SMALL_NUMBER;
	if (!bKeepTelegraph)
	{
		HideTelegraph();
	}

	// 첫 판정
	PerformHitCheck();

	// 유지형이면 수명 타이머 + (틱 주기 지정 시) 틱뎀 반복, 아니면 즉시 종료.
	// 주의: 틱 주기 미지정이어도 Duration 이 있으면 살아있어야 한다
	// (이전엔 Duration>0 && TickInterval==0 이면 즉시 소멸해 투사체가 생성 직후 사라지는 버그).
	if (Duration > KINDA_SMALL_NUMBER)
	{
		if (TickInterval > KINDA_SMALL_NUMBER)
		{
			GetWorldTimerManager().SetTimer(
				TickTimerHandle, this, &ABossPatternActorBase::PerformHitCheck,
				TickInterval, true);
		}

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

	TArray<APawn*> PlayerPawns;
	UBossCombatStatics::GetPlayerPawns(World, PlayerPawns);
	for (APawn* Pawn : PlayerPawns)
	{
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

		// 높이(Z) 체크. 단, bIgnoreHeightCheck 이거나 행동(잡기 등)이 높이 무시를 요구하면 스킵.
		// (보스 캡슐/메시 크기가 바뀌면 스폰 높이가 흔들려 XY 범위 안이어도 걸러지는 문제 방지)
		const bool bSkipHeight = bIgnoreHeightCheck || (OnHitEffect && OnHitEffect->IgnoresAoeHeight());
		if (!bSkipHeight && FMath::Abs(P.Z - AttackCenter.Z) > HeightTolerance)
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

	// 기믹 타워 판정 (지형파괴 기믹의 레이저 장판만 — BP 에서 bCanHitGimmickTargets 켠 경우).
	// 타워는 GE 데미지 경로 대신 OnBossLaserHit 로 즉시 파괴 처리 -> '이 장판으로만 파괴 가능' 규칙
	if (bCanHitGimmickTargets)
	{
		for (TActorIterator<ABossGimmickTower> It(World); It; ++It)
		{
			ABossGimmickTower* Tower = *It;
			if (!IsValid(Tower) || Tower->IsDying())
			{
				continue;
			}
			if (bSingleHitPerTarget && AlreadyHitActors.Contains(Tower))
			{
				continue;
			}

			const FVector P = Tower->GetActorLocation();
			if (!bIgnoreHeightCheck && FMath::Abs(P.Z - AttackCenter.Z) > HeightTolerance)
			{
				continue;
			}
			if (!IsInsideShape(P))
			{
				continue;
			}

			AlreadyHitActors.Add(Tower);
			OnAoeHitActor.Broadcast(Tower);
			Tower->OnBossLaserHit();
		}
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
	// 행동 오브젝트가 지정돼 있으면 위임(잡기/전하스왑 등), 없으면 기본 데미지+상태이상+넉백
	if (OnHitEffect)
	{
		OnHitEffect->OnHit(this, Target);
		return;
	}
	ApplyDamageAndStatus(Target);
	ApplyKnockback(Target);
}

void ABossPatternActorBase::ApplyKnockback(AActor* Target)
{
	if (Knockback.Reaction == EAoeHitReaction::None || !HasAuthority())
	{
		return;
	}

	ACharacter* Char = Cast<ACharacter>(Target);
	if (!Char)
	{
		return;
	}

	// 이동이 잠긴 대상(잡힘 = MOVE_None)은 밀지 않는다 (보스 소켓 부착이 찢어지는 사고 방지)
	const UCharacterMovementComponent* Move = Char->GetCharacterMovement();
	if (!Move || Move->MovementMode == MOVE_None)
	{
		return;
	}

	const float VSpeed = FMath::Max(Knockback.VerticalSpeed, 0.f);

	// 제자리 띄움: 수평 속도를 0으로 덮어써 서 있던 자리에 그대로 착지 -> 낙사가 원천적으로 불가능
	if (Knockback.Reaction == EAoeHitReaction::LaunchUp)
	{
		Char->LaunchCharacter(FVector(0.f, 0.f, VSpeed), true, true);
		return;
	}

	// 뒤로 밀림
	const FVector Dir = ComputeKnockbackDirection(Char);
	float HSpeed = FMath::Max(Knockback.HorizontalSpeed, 0.f);
	if (!Knockback.bCanCauseFallDeath)
	{
		HSpeed = ClampPushSpeedToGround(Char, Dir, HSpeed, VSpeed);
	}
	Char->LaunchCharacter(Dir * HSpeed + FVector(0.f, 0.f, VSpeed), true, true);
}

FVector ABossPatternActorBase::ComputeKnockbackDirection(const AActor* Target) const
{
	FVector Dir;
	switch (Knockback.Direction)
	{
	case EAoeKnockbackDirection::AlongForward:
		Dir = ShapeForward;
		break;

	case EAoeKnockbackDirection::FromCaster:
		Dir = Target->GetActorLocation() - (Caster ? Caster->GetActorLocation() : AttackCenter);
		break;

	case EAoeKnockbackDirection::FromCenter:
	default:
		Dir = Target->GetActorLocation() - AttackCenter;
		break;
	}

	Dir.Z = 0.f;
	if (!Dir.Normalize())
	{
		// 중심과 정확히 겹친 경우 등 퇴화 -> 장판 전방으로 폴백
		Dir = ShapeForward;
	}
	return Dir;
}

float ABossPatternActorBase::ClampPushSpeedToGround(const ACharacter* Char, const FVector& Dir,
	float HSpeed, float VSpeed) const
{
	if (HSpeed <= 0.f)
	{
		return 0.f;
	}

	// 예상 체공시간(포물선 왕복)으로 수평 이동거리 추정.
	// 수직 팝이 낮아도 착지 후 미끄러지는 거리가 있으므로 최소 체공은 가정한다.
	const UCharacterMovementComponent* Move = Char->GetCharacterMovement();
	const float Gravity = Move ? FMath::Abs(Move->GetGravityZ()) : 980.f;
	const float AirTime = FMath::Max(2.f * VSpeed / FMath::Max(Gravity, 1.f), 0.35f);
	const float FullDist = HSpeed * AirTime;

	float CapsuleRadius = 34.f;
	if (const UCapsuleComponent* Capsule = Char->GetCapsuleComponent())
	{
		CapsuleRadius = Capsule->GetScaledCapsuleRadius();
	}

	// 밀려나는 경로를 일정 간격으로 샘플링해 바닥이 끊기는 첫 지점을 찾는다.
	// (파괴된 슬라이스는 콜리전이 제거되고, 아레나 밖은 바닥이 없어 트레이스가 실패한다)
	const FVector Feet = GetFeetLocation(Char);
	const float Step = FMath::Max(CapsuleRadius, 50.f);
	float SafeDist = FullDist;
	bool bHitHole = false;
	for (float D = Step; D <= FullDist + KINDA_SMALL_NUMBER; D += Step)
	{
		const float SampleDist = FMath::Min(D, FullDist);
		if (!HasLandingGroundAt(Feet + Dir * SampleDist, Char))
		{
			// 구멍 직전 샘플까지만. 캡슐 반지름만큼 더 물러나 가장자리에 걸치지 않게 한다
			SafeDist = FMath::Max(SampleDist - Step - CapsuleRadius, 0.f);
			bHitHole = true;
			break;
		}
	}

	if (!bHitHole)
	{
		return HSpeed;	// 경로 전체에 바닥이 이어짐 -> 그대로 밀기
	}
	return HSpeed * (SafeDist / FullDist);
}

bool ABossPatternActorBase::HasLandingGroundAt(const FVector& At, const AActor* IgnoreTarget) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return true;
	}

	// 발밑 기준 위 100 ~ 아래 300cm 범위에 바닥이 있어야 '이어진 지형'으로 본다.
	// (TraceGroundZ 처럼 수천 cm 아래까지 보면 구덩이 밑바닥을 바닥으로 오인해 낙사 방지가 뚫린다)
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AoeKnockbackGroundCheck), /*bTraceComplex=*/true);
	if (Caster)
	{
		Params.AddIgnoredActor(Caster);
	}
	if (IgnoreTarget)
	{
		Params.AddIgnoredActor(IgnoreTarget);
	}
	Params.AddIgnoredActor(this);

	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	const FVector Start = At + FVector(0.f, 0.f, 100.f);
	const FVector End = At - FVector(0.f, 0.f, 300.f);

	FHitResult Hit;
	return World->LineTraceSingleByObjectType(Hit, Start, End, ObjParams, Params);
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

	// 데미지 GE (시전자 ASC 필수 — 데미지 귀속/계산이 소스 기준)
	if (SourceASC)
	{
		UBossCombatStatics::ApplyEffect(SourceASC, TargetASC, DamageEffect, this,
			/*Instigator=*/Caster, /*EffectCauser=*/this,
			DamageSetByCallerTag, DamageCoefficient);
	}

	// 상태이상 GE들 (소스 ASC 없으면 헬퍼가 타겟 자신을 소스로 폴백)
	for (const TSubclassOf<UGameplayEffect>& StatusGE : StatusEffects)
	{
		UBossCombatStatics::ApplyEffect(SourceASC, TargetASC, StatusGE, this);
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
	const FGameplayTag Tag = DeadTag.IsValid() ? DeadTag : LostArkTags::State_Dead;
	return UBossCombatStatics::IsAliveActor(Target, Tag);
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
