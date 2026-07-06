// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "BossPatternActorBase.generated.h"

class UGameplayEffect;
class UAbilitySystemComponent;
class UPrimitiveComponent;
class UProceduralMeshComponent;

/**
 * 장판을 '어디에' 생성할지. (스폰 원점 정책 — Base가 BeginPlay에서 해석)
 * - SpawnTransform    : 스폰 시 넘겨준 트랜스폼 그대로 (노티파이가 이미 위치 계산한 경우)
 * - CasterLocation    : 시전자(보스) 발밑. 캡슐 중심 장판
 * - CasterSocket      : 시전자 메시 소켓 위치. 망치 머리 등 무기 타점
 * - TargetLocation    : 선정된 타겟 위치
 * - GroundUnderTarget : 타겟 아래로 라인트레이스한 지면 (경사/단차 대응)
 */
UENUM(BlueprintType)
enum class EAoeSpawnOrigin : uint8
{
	SpawnTransform		UMETA(DisplayName = "스폰 트랜스폼"),
	CasterLocation		UMETA(DisplayName = "시전자 위치"),
	CasterSocket		UMETA(DisplayName = "시전자 소켓"),
	TargetLocation		UMETA(DisplayName = "타겟 위치"),
	GroundUnderTarget	UMETA(DisplayName = "타겟 아래 지면")
};

/**
 * 장판이 어떻게 움직이는가.
 * - Fixed        : 스폰 시점 위치에 고정 (일반 예고 장판)
 * - Follow       : 매 틱 시전자(또는 소켓) 위치를 따라감 (몸통/무기 주변 장판)
 * - Homing       : 타겟 위치로 서서히 이동 (유도 장판)
 * - FollowTarget : 매 틱 타겟 '발밑'에 부착되어 따라다님 (전하 변환장판 등)
 */
UENUM(BlueprintType)
enum class EAoeTargetingMode : uint8
{
	Fixed		UMETA(DisplayName = "고정"),
	Follow		UMETA(DisplayName = "시전자 추적"),
	Homing		UMETA(DisplayName = "타겟 유도"),
	FollowTarget	UMETA(DisplayName = "타겟 부착(발밑)")
};

/**
 * 패턴별로 덮어쓰는 공통 파라미터. (스폰 노티파이가 들고 있다가 스폰 시 주입)
 * BP_Aoe* 는 하나만 두고, 시전/유지/틱 같은 값은 패턴(몽타주)마다 여기서 바꾼다.
 */
USTRUCT(BlueprintType)
struct FBossAoeCommonOverride
{
	GENERATED_BODY()

	/** 시전시간 = 예고 후 첫 판정까지 대기(초) */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float CastTime = 1.f;

	/** 유지시간(초). 0이면 1회 판정 후 소멸 */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float Duration = 0.f;

	/** 틱뎀 주기(초). Duration>0 일 때만 */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float TickInterval = 0.5f;

	/** 수직 판정 허용 오차(cm) */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float HeightTolerance = 200.f;

	/** 대상당 한 번만 타격 */
	UPROPERTY(EditAnywhere)
	bool bSingleHitPerTarget = true;

	/** 즉발(예고 비주얼 스킵) */
	UPROPERTY(EditAnywhere)
	bool bInstant = false;

	/** 장판 이동 방식 */
	UPROPERTY(EditAnywhere)
	EAoeTargetingMode TargetingMode = EAoeTargetingMode::Fixed;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAoeBegin);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAoeHitActor, AActor*, HitActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAoeEnd);

/**
 * 보스 패턴이 스폰하는 장판/판정 액터의 베이스.
 *
 * 책임 분리:
 *  - 이 베이스 = 스폰 원점 해석, 수명주기(예고->판정->유지->소멸), 타겟팅 이동,
 *    GAS 적용, 이벤트, 네트워크
 *  - 판정 '도형'은 자식이 IsInsideShape()/BuildTelegraph() 두 개만 구현하면 확장 끝.
 *    방향(Forward/Right)과 텔레그래프 메시 생성 헬퍼는 베이스가 제공한다.
 *
 * 네트워크: 서버 권위.
 *  - 서버에서만 히트 판정 + GameplayEffect 적용
 *  - 액터 자체는 복제되어 클라에도 스폰되지만, 예고 비주얼은 코스메틱(클라 로컬)
 */
UCLASS(Abstract)
class LOSTARK_API ABossPatternActorBase : public AActor
{
	GENERATED_BODY()

public:
	ABossPatternActorBase();

	virtual void Tick(float DeltaTime) override;

	/**
	 * 스폰 직후 브레인/노티파이가 호출해 런타임 값 주입.
	 * @param InCaster            패턴 시전자(보스). 데미지 Instigator + 스폰원점/Follow 기준
	 * @param InTarget            타겟. Homing/TargetLocation/GroundUnderTarget 기준
	 * @param InDamageCoefficient 공격력계수 (GE에 SetByCaller로 전달)
	 */
	UFUNCTION(BlueprintCallable, Category = "Aoe")
	void InitAoe(AActor* InCaster, AActor* InTarget, float InDamageCoefficient);

	/** 패턴별 공통값 오버라이드 주입 (스폰 노티파이가 BeginPlay 전에 호출) */
	UFUNCTION(BlueprintCallable, Category = "Aoe")
	void ApplyCommonOverride(const FBossAoeCommonOverride& Override);

	// ─── 이벤트 (콤보/연계/이펙트 트리거용) ───
	UPROPERTY(BlueprintAssignable, Category = "Aoe|Events")
	FOnAoeBegin OnAoeBegin;

	UPROPERTY(BlueprintAssignable, Category = "Aoe|Events")
	FOnAoeHitActor OnAoeHitActor;

	UPROPERTY(BlueprintAssignable, Category = "Aoe|Events")
	FOnAoeEnd OnAoeEnd;

protected:
	virtual void BeginPlay() override;

	// ═══════════════ 자식이 오버라이드하는 도형 훅 ═══════════════

	/**
	 * 월드 좌표가 이 장판의 판정 도형 안에 있는지. (자식 구현; 베이스는 항상 false)
	 * 방향이 필요한 도형(박스/부채꼴)은 GetShapeForward()/GetShapeRight() 사용.
	 */
	virtual bool IsInsideShape(const FVector& WorldPoint) const { return false; }

	/**
	 * 예고 비주얼 메시 생성. (자식 구현)
	 * 로컬 좌표(X=Forward, Y=Right) 기준 정점/삼각형만 만들어
	 * CreateTelegraphMesh(Verts, Tris) 에 넘기면 끝.
	 */
	virtual void BuildTelegraph() {}

	// ═══════════════ 자식이 쓰는 헬퍼 ═══════════════

	/** 스폰 시 캐싱된 평면 전방(정규화). 박스/부채꼴 방향 기준 */
	FORCEINLINE const FVector& GetShapeForward() const { return ShapeForward; }
	FORCEINLINE const FVector& GetShapeRight() const { return ShapeRight; }

	/**
	 * 로컬 좌표 정점/삼각형으로 텔레그래프 프로시저럴 메시를 만들어 WarningComp에 등록.
	 * (NewObject/Register/Attach/Material 처리를 대신 해줌 -> 자식은 지오메트리만 계산)
	 */
	UProceduralMeshComponent* CreateTelegraphMesh(const TArray<FVector>& Vertices, const TArray<int32>& Triangles);

	// ═══════════════ 설정 프로퍼티 ═══════════════

	/** 스폰 원점 정책 */
	UPROPERTY(EditDefaultsOnly, Category = "Aoe|Spawn")
	EAoeSpawnOrigin SpawnOrigin = EAoeSpawnOrigin::SpawnTransform;

	/** CasterSocket/Follow(소켓) 일 때 사용할 시전자 메시 소켓 이름 (망치 머리 등) */
	UPROPERTY(EditDefaultsOnly, Category = "Aoe|Spawn", meta = (EditCondition = "SpawnOrigin == EAoeSpawnOrigin::CasterSocket"))
	FName SpawnSocketName = NAME_None;

	/** 공격력계수. GE에 SetByCaller(DamageSetByCallerTag)로 전달. InitAoe로 덮어쓸 수 있음 */
	UPROPERTY(EditDefaultsOnly, Category = "Aoe|Damage")
	float DamageCoefficient = 1.f;

	/** 적중 시 적용할 데미지 GE (실제 수치는 GE의 SetByCaller/Calculation이 결정) */
	UPROPERTY(EditDefaultsOnly, Category = "Aoe|Damage")
	TSubclassOf<UGameplayEffect> DamageEffect;

	/** 공격력계수를 실을 SetByCaller 태그 (예: Data.Damage). 미설정 시 계수 전달 생략 */
	UPROPERTY(EditDefaultsOnly, Category = "Aoe|Damage")
	FGameplayTag DamageSetByCallerTag;

	/** 적중 시 함께 적용할 상태이상 GE들 (감전/출혈/스턴 등) */
	UPROPERTY(EditDefaultsOnly, Category = "Aoe|Status")
	TArray<TSubclassOf<UGameplayEffect>> StatusEffects;

	/** 시전시간 = 예고 표시 후 첫 판정까지의 대기(초). 즉발이면 예고만 스킵되고 이 대기는 유지 */
	UPROPERTY(EditDefaultsOnly, Category = "Aoe|Timing", meta = (ClampMin = "0.0"))
	float CastTime = 1.f;

	/** 유지시간(초). 0이면 첫 판정 1회 후 즉시 소멸. >0이면 장판이 남아 틱뎀 */
	UPROPERTY(EditDefaultsOnly, Category = "Aoe|Timing", meta = (ClampMin = "0.0"))
	float Duration = 0.f;

	/** 틱뎀 주기(초). Duration>0 일 때만 사용. 0이면 첫 판정 1회만 */
	UPROPERTY(EditDefaultsOnly, Category = "Aoe|Timing", meta = (ClampMin = "0.0"))
	float TickInterval = 0.5f;

	/** 대상당 한 번만 타격. 유지형이어도 처음 들어온 대상은 1회만 (지속형 vs 즉발형 구분) */
	UPROPERTY(EditDefaultsOnly, Category = "Aoe|Hit")
	bool bSingleHitPerTarget = true;

	/** 즉발: 예고 장판 비주얼을 표시하지 않음 (판정 타이밍은 CastTime 그대로) */
	UPROPERTY(EditDefaultsOnly, Category = "Aoe|Hit")
	bool bInstant = false;

	/** 수직 판정 허용 오차(cm). |대상.Z - 중심.Z| 가 이 값 이하일 때만 적중 (공중 대상 제외) */
	UPROPERTY(EditDefaultsOnly, Category = "Aoe|Hit", meta = (ClampMin = "0.0"))
	float HeightTolerance = 200.f;

	/** 장판 이동 방식 */
	UPROPERTY(EditDefaultsOnly, Category = "Aoe|Targeting")
	EAoeTargetingMode TargetingMode = EAoeTargetingMode::Fixed;

	/** Homing 이동 속도(cm/s). 0이면 순간 이동 */
	UPROPERTY(EditDefaultsOnly, Category = "Aoe|Targeting", meta = (EditCondition = "TargetingMode == EAoeTargetingMode::Homing"))
	float HomingSpeed = 400.f;

	/** 이 태그를 가진 플레이어는 대상에서 제외 (사망 등). 미설정 시 State.Dead 폴백 */
	UPROPERTY(EditDefaultsOnly, Category = "Aoe|Targeting")
	FGameplayTag DeadTag;

	/** 예고 비주얼 머티리얼 (반투명 경고). CreateTelegraphMesh 가 자동 적용 */
	UPROPERTY(EditDefaultsOnly, Category = "Aoe|Telegraph")
	TObjectPtr<UMaterialInterface> WarningMaterial;

	// ═══════════════ 런타임 상태 ═══════════════

	/** 시전자(보스). 데미지 Instigator, 스폰원점/Follow 기준 */
	UPROPERTY(Transient)
	TObjectPtr<AActor> Caster;

	/** 타겟 (Homing/Target 원점 기준) */
	UPROPERTY(Transient)
	TObjectPtr<AActor> HomingTarget;

	/** 판정 중심(월드). Fixed면 고정, Follow/Homing이면 매 틱 갱신 */
	FVector AttackCenter = FVector::ZeroVector;

	/** 자식이 예고 비주얼을 저장하는 컴포넌트 (HideTelegraph 시 파괴) */
	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> WarningComp;

	// ═══════════════ 내부 로직 ═══════════════

	/** 스폰 원점 정책을 해석해 액터 위치/회전 + AttackCenter + Forward/Right 캐싱 */
	void ResolveOrigin();

	/** 예고 종료 시점(CastTime 경과) 콜백: 예고 제거 -> 첫 판정 -> 유지/소멸 결정 */
	void OnCastFinished();

	/** 한 번의 판정: 도형+높이+생존 필터 통과한 플레이어에게 GE 적용 */
	virtual void PerformHitCheck();

	/** 대상 1명에게 데미지+상태이상 GE 적용 (서버). 자식이 잡기/전하스왑 등으로 대체 가능 */
	virtual void ApplyEffectsTo(AActor* Target);

	/** 유지 종료 -> 이벤트 방송 + 파괴. 자식이 파괴 지연(잡기 유지 등)에 오버라이드 가능 */
	virtual void FinishAoe();

	/** 대상 발밑 위치 (Character면 캡슐 절반만큼 내림) */
	static FVector GetFeetLocation(const AActor* Target);

	/** 예고 비주얼 제거 */
	void HideTelegraph();

	/** Follow/Homing 이동 처리 */
	void UpdateCenter(float DeltaTime);

	/** 스폰/Follow 시 시전자 기준 위치 (소켓 지정 시 소켓, 아니면 액터 위치) */
	FVector GetCasterOriginLocation() const;

	/** 대상이 생존 상태인지 (DeadTag 없으면 생존 간주) */
	bool IsAlive(const AActor* Target) const;

private:
	FVector ShapeForward = FVector::ForwardVector;	// 스폰 시 캐싱된 평면 전방
	FVector ShapeRight = FVector::RightVector;		// 스폰 시 캐싱된 평면 우측

	FTimerHandle CastTimerHandle;	// 예고->첫 판정
	FTimerHandle TickTimerHandle;	// 유지 중 틱뎀
	FTimerHandle LifeTimerHandle;	// 유지시간 종료

	/** 이번 수명 동안 이미 맞은 대상 (bSingleHitPerTarget 용) */
	UPROPERTY(Transient)
	TSet<TWeakObjectPtr<AActor>> AlreadyHitActors;

	bool bFinished = false;
};
