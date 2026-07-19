// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "BossPatternActorBase.generated.h"

class UGameplayEffect;
class UAbilitySystemComponent;
class UPrimitiveComponent;
class ACharacter;
class UProceduralMeshComponent;
class UBossAoeEffect;
class UNiagaraSystem;
class UNiagaraComponent;
class UParticleSystem;
class UParticleSystemComponent;

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
 * - Homing       : 타겟 위치로 서서히 이동 (유도 장판). Duration>0 이어야 지속 추적처럼 보임
 * - FollowTarget : 매 틱 타겟 '발밑'에 부착되어 따라다님 (전하 변환장판 등)
 * - Spiral       : 타겟을 향해 직선 이동하면서 나선형으로 회전 궤도를 그리며 접근 (유도탄/번개 낙하 등)
 * - Straight     : 타겟 없이 스폰 방향(Forward)으로 등속 직진. 추적하지 않음 (방사형 투사체/번개 등)
 */
UENUM(BlueprintType)
enum class EAoeTargetingMode : uint8
{
	Fixed		UMETA(DisplayName = "고정"),
	Follow		UMETA(DisplayName = "시전자 추적"),
	Homing		UMETA(DisplayName = "타겟 유도"),
	FollowTarget	UMETA(DisplayName = "타겟 부착(발밑)"),
	Spiral		UMETA(DisplayName = "나선형 유도"),
	Straight	UMETA(DisplayName = "직선 발사")
};

/**
 * 피격 리액션(넉백) 종류. 패턴마다 '맞으면 어떻게 되는가'를 구분한다.
 * - None     : 데미지만. 넉백 없음
 * - PushBack : 뒤로 밀려 날아감 (수평 넉백 + 수직 팝). 낙사 허용 여부는 bCanCauseFallDeath 가 결정
 * - LaunchUp : 그 자리에서 공중에 잠깐 위로 솟기만 함 (수평 이동 0 -> 제자리 착지, 낙사 없음)
 */
UENUM(BlueprintType)
enum class EAoeHitReaction : uint8
{
	None		UMETA(DisplayName = "없음 (데미지만)"),
	PushBack	UMETA(DisplayName = "뒤로 밀림"),
	LaunchUp	UMETA(DisplayName = "제자리 띄움")
};

/** PushBack 시 밀려나는 방향 기준 */
UENUM(BlueprintType)
enum class EAoeKnockbackDirection : uint8
{
	/** 장판 중심 -> 대상 방향 (원형/부채꼴 기본. 폭발처럼 바깥으로) */
	FromCenter		UMETA(DisplayName = "장판 중심에서 바깥"),
	/** 장판 전방 (직선/사각 장판, 투사체 진행 방향으로 떠밀기) */
	AlongForward	UMETA(DisplayName = "장판 전방"),
	/** 시전자(보스) 중심 -> 대상 방향 (Follow 회전 장판 등 보스 기준 바깥으로) */
	FromCaster		UMETA(DisplayName = "시전자에서 바깥")
};

/**
 * 패턴별 피격 리액션(넉백) 설정.
 * 장판 BP 디폴트에 두거나, 스폰 노티파이의 KnockbackOverride 로 패턴(몽타주)마다 덮어쓴다.
 *
 * 낙사 규칙:
 *  - LaunchUp 은 수평 이동이 없으므로 낙사가 원천적으로 불가능
 *  - PushBack + bCanCauseFallDeath=true  : 밀린 방향에 파괴 지형/아레나 밖이 있으면 그대로 추락 (낙사 연계 패턴)
 *  - PushBack + bCanCauseFallDeath=false : 착지 경로의 바닥을 검사해 구멍 직전까지만 밀리게 수평 속도를 줄인다
 */
USTRUCT(BlueprintType)
struct FBossAoeKnockbackConfig
{
	GENERATED_BODY()

	/** 피격 리액션 종류 */
	UPROPERTY(EditAnywhere)
	EAoeHitReaction Reaction = EAoeHitReaction::None;

	/** 밀려나는 방향 기준 (PushBack 전용) */
	UPROPERTY(EditAnywhere, meta = (EditCondition = "Reaction == EAoeHitReaction::PushBack"))
	EAoeKnockbackDirection Direction = EAoeKnockbackDirection::FromCenter;

	/** 수평 넉백 속도(cm/s). PushBack 전용 */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0", EditCondition = "Reaction == EAoeHitReaction::PushBack"))
	float HorizontalSpeed = 1200.f;

	/** 수직(위) 속도(cm/s). PushBack 의 팝 높이 / LaunchUp 의 솟는 높이 */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0", EditCondition = "Reaction != EAoeHitReaction::None"))
	float VerticalSpeed = 420.f;

	/**
	 * 이 공격이 낙사를 유발할 수 있는가 (PushBack 전용).
	 * false 면 밀려나는 경로의 바닥을 검사해 파괴된 슬라이스/아레나 밖 구멍 '직전'까지만 밀린다.
	 * true 면 검사 없이 그대로 날려서 파괴 지형 위로 밀리면 낙사한다 (KillVolume 처리).
	 */
	UPROPERTY(EditAnywhere, meta = (EditCondition = "Reaction == EAoeHitReaction::PushBack"))
	bool bCanCauseFallDeath = false;
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

	/** 유지시간 동안 예고 비주얼을 계속 표시 (회전 장판 등 지속 위험지대용) */
	UPROPERTY(EditAnywhere)
	bool bKeepTelegraphWhileActive = false;

	/** 예고가 시전 중앙에서 판정 범위까지 CastTime 동안 점차 차오름 (저스트가드 등 타이밍 읽기 패턴용) */
	UPROPERTY(EditAnywhere)
	bool bTelegraphFill = false;

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

	/**
	 * 직선 투사체 셋업. 방사형(Radial) 등 투사체 스폰 노티파이가 BeginPlay 전에 호출해
	 * BP 클래스 설정과 무관하게 '스폰 트랜스폼 방향으로 등속 직진'을 강제한다.
	 *  - TargetingMode=Straight, SpawnOrigin=SpawnTransform 고정
	 *    (BP가 CasterLocation 등으로 돼 있으면 방사 방향/위치가 보스 것으로 덮여 전부 뭉개지는 사고 방지)
	 *  - 수명 = Range / Speed (사거리 끝까지 날아간 뒤 소멸)
	 * @param Speed       진행 속도(cm/s)
	 * @param Range       사거리(cm). 이만큼 이동하면 소멸
	 * @param HitInterval 비행 중 판정 주기(초)
	 * @param InCastTime  발사 전 대기(초). 0이면 예고 없이 즉시 발사, >0이면 예고 후 발사
	 */
	UFUNCTION(BlueprintCallable, Category = "Aoe")
	void SetupStraightProjectile(float Speed, float Range, float HitInterval, float InCastTime);

	/**
	 * 본체 VFX 를 이 스폰 1회에만 덮어씀 (스폰 노티파이가 BeginPlay 전에 호출).
	 * BP 클래스에 박힌 BodyEffect 대신 노티파이(패턴)마다 다른 몸통 이펙트를 쓸 때 사용.
	 * null 인자는 무시(해당 종류는 BP 기본값 유지). 나이아가라/캐스케이드 각각 지정 가능.
	 */
	UFUNCTION(BlueprintCallable, Category = "Aoe")
	void SetBodyEffectOverride(UNiagaraSystem* InNiagara, UParticleSystem* InCascade);

	/**
	 * 스폰 원점 정책을 코드에서 강제 (BeginPlay 전 호출).
	 * 기믹 타워 등 외부 스포너가 BP 설정과 무관하게 '스폰 트랜스폼 그대로'를 보장할 때 사용.
	 */
	UFUNCTION(BlueprintCallable, Category = "Aoe")
	void SetSpawnOriginPolicy(EAoeSpawnOrigin InOrigin) { SpawnOrigin = InOrigin; }

	// ─── 행동 오브젝트(UBossAoeEffect)가 쓰는 공개 API ───

	/** 시전자(보스) */
	FORCEINLINE AActor* GetCaster() const { return Caster; }

	/** 판정 중심(월드) */
	FORCEINLINE const FVector& GetAttackCenter() const { return AttackCenter; }

	/** 스폰 시 캐싱된 평면 전방(던지기 방향 폴백 등) */
	FORCEINLINE FVector GetShapeForwardVector() const { return ShapeForward; }

	/** 스폰 시 캐싱된 평면 우측(각도 계산용) */
	FORCEINLINE FVector GetShapeRightVector() const { return ShapeRight; }

	/** 기본 데미지+상태이상 GE 적용 (서버). 행동 오브젝트가 데미지가 필요할 때 호출(잡기 해제 시 등) */
	void ApplyDamageAndStatus(AActor* Target);

	/**
	 * 피격 리액션(넉백) 적용 (서버). Knockback.Reaction 이 None 이면 아무것도 안 함.
	 * 기본 데미지 경로는 자동 호출되고, 행동 오브젝트(저스트가드 실패 등)도 데미지 적용 후 직접 호출할 수 있다.
	 * 이동이 잠긴 대상(잡힘 = MOVE_None)은 건드리지 않는다.
	 */
	void ApplyKnockback(AActor* Target);

	/** 패턴별 넉백 설정 주입 (스폰 노티파이가 BeginPlay 전에 호출. BP 클래스 디폴트는 불변) */
	UFUNCTION(BlueprintCallable, Category = "Aoe")
	void SetKnockbackOverride(const FBossAoeKnockbackConfig& InKnockback) { Knockback = InKnockback; }

	/** 실제 파괴(이벤트 방송 + Destroy). 행동 오브젝트가 파괴 타이밍을 제어할 때 호출(잡기 해제 후) */
	void DestroyAoeNow();

	/** 적중 시 행동 오브젝트 (노티파이/외부에서 잡기 해제 등 접근용) */
	FORCEINLINE UBossAoeEffect* GetOnHitEffect() const { return OnHitEffect; }

	/** 시전자(보스) ASC에 루스 태그 부여 (패턴 결과 보고용. 패턴 어빌리티가 종료 시 PatternResult 하위 일괄 제거) */
	void AddCasterLooseTags(const FGameplayTagContainer& InTags);

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
	 * 실제 판정 도형을 디버그 라인으로 그린다. (자식 구현; 베이스는 아무것도 안 그림)
	 * bDrawDebugHitShape 가 켜졌을 때 PerformHitCheck 에서 호출 -> 데칼 크기와 판정 크기 비교용.
	 */
	virtual void DebugDrawShape() const {}

	/**
	 * 예고 비주얼 메시 생성. (자식 구현)
	 * 로컬 좌표(X=Forward, Y=Right) 기준 정점/삼각형을 만들어 CreateTelegraphMesh(Verts, Tris)에 넘긴다.
	 * 메시 자체가 도형이라 머티리얼 마스킹이 필요 없다(밋밋한 반투명 머티리얼이면 충분).
	 * TelegraphEffect 가 지정된 경우 이 함수 대신 BuildTelegraphEffect+ConfigureTelegraphEffect 가 쓰인다.
	 */
	virtual void BuildTelegraph() {}

	/**
	 * VFX 예고(TelegraphEffect)에 도형 파라미터를 주입하는 훅. (자식 구현, 선택)
	 * 나이아가라 시스템에 같은 이름의 User 파라미터(float)를 노출해두면 여기서 세팅한다.
	 * 예: Circle -> "Radius"/"InnerRatio", Sector -> "Radius"/"StartAngleDeg"/"EndAngleDeg".
	 * (VFX는 지오메트리 마스킹 없이 크기만 맞추면 되므로 도형 각각 노출 파라미터명만 맞추면 됨)
	 */
	virtual void ConfigureTelegraphEffect(UNiagaraComponent* NiagaraComp) const {}

	// ═══════════════ 자식이 쓰는 헬퍼 ═══════════════

	/** 스폰 시 캐싱된 평면 전방(정규화). 박스/부채꼴 방향 기준 */
	FORCEINLINE const FVector& GetShapeForward() const { return ShapeForward; }
	FORCEINLINE const FVector& GetShapeRight() const { return ShapeRight; }

	/**
	 * 로컬 좌표 정점/삼각형으로 텔레그래프 프로시저럴 메시를 만들어 WarningComp에 등록.
	 * (NewObject/Register/Attach/바닥스냅/Material 처리를 대신 해줌 -> 자식은 지오메트리만 계산)
	 */
	UProceduralMeshComponent* CreateTelegraphMesh(const TArray<FVector>& Vertices, const TArray<int32>& Triangles);

	/**
	 * 원/도넛/부채꼴 계열 텔레그래프 메시 생성 헬퍼.
	 * [StartAngleDeg, EndAngleDeg] 구간(전방=0°, 우측 +)을 InInnerRadius>0 이면 링 스트립,
	 * 아니면 중심 팬으로 삼각화해 CreateTelegraphMesh 로 등록한다.
	 * (원 = 0~360° 부채꼴이므로 Circle/Sector 도형이 이 하나를 공유)
	 */
	UProceduralMeshComponent* CreateArcTelegraphMesh(float StartAngleDeg, float EndAngleDeg,
		float InInnerRadius, float InOuterRadius, int32 Segments = 48);

	// ═══════════════ 설정 프로퍼티 ═══════════════

	/**
	 * 적중 시 행동. null 이면 아래 Damage/Status GE 를 그대로 적용(기본).
	 * 잡기/전하스왑 등은 여기에 해당 UBossAoeEffect 인스턴스를 지정 -> 어떤 도형에든 붙는다.
	 */
	UPROPERTY(EditDefaultsOnly, Instanced, Category = "Aoe|Behavior")
	TObjectPtr<UBossAoeEffect> OnHitEffect;

	/**
	 * 1명 이상 적중 시 시전자(보스) ASC에 부여할 태그 (첫 적중에 1회).
	 * 스텝 Branch(TagQuery) 조건용 — 예: State.Boss.PatternResult.AoeHit 를 넣으면
	 * '이 장판에 맞은 사람이 있으면 다음 몽타주 진행' 분기를 만들 수 있다.
	 * State.Boss.PatternResult 하위 태그는 패턴 종료 시 자동 제거된다.
	 * (잡기 성공 태그는 잡기 행동이 자동 부여하므로 따로 설정 불필요)
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Aoe|Behavior")
	FGameplayTagContainer CasterTagsOnHit;

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

	/**
	 * 피격 리액션(넉백) 설정. 기본 데미지 경로(OnHitEffect 미지정)에서 데미지 직후 자동 적용.
	 * 패턴(몽타주)마다 다르게 쓰려면 스폰 노티파이의 KnockbackOverride 로 덮어쓴다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Aoe|Knockback")
	FBossAoeKnockbackConfig Knockback;

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

	/**
	 * 켜면 높이(Z) 게이트를 건너뛰고 XY 도형(Radius/각도)만으로 판정한다.
	 * 보스 캡슐/메시 크기에 따라 스폰 높이가 흔들려 판정이 어긋나는 패턴(특히 잡기)에서 사용.
	 * 잡기 행동(UBossAoeGrabEffect)은 이 값과 무관하게 항상 높이를 무시한다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Aoe|Hit")
	bool bIgnoreHeightCheck = false;

	/** 즉발: 예고 장판 비주얼을 표시하지 않음 (판정 타이밍은 CastTime 그대로) */
	UPROPERTY(EditDefaultsOnly, Category = "Aoe|Hit")
	bool bInstant = false;

	/**
	 * 켜면 플레이어 외에 기믹 타워(ABossGimmickTower)도 판정 대상에 포함한다.
	 * 지형파괴 기믹의 '레이저' 장판 BP 에서만 켤 것 — 타워는 이 장판으로만 파괴 가능 규칙.
	 * (타워는 데미지 GE 대신 OnBossLaserHit 호출로 즉시 파괴 처리된다)
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Aoe|Hit")
	bool bCanHitGimmickTargets = false;

	/**
	 * 켜면 CastTime 경과 후에도 예고 비주얼을 파괴하지 않고 유지시간(Duration) 내내 표시한다.
	 * 보스를 따라 도는 회전 장판처럼 '지금 위험한 지대'를 계속 보여줘야 하는 패턴용.
	 * (예고 컴포넌트는 루트에 붙어 있어 Follow 회전/이동을 자동 추종, 액터 소멸 시 함께 정리)
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Aoe|Telegraph")
	bool bKeepTelegraphWhileActive = false;

	/**
	 * 켜면 예고 비주얼이 시전 중앙에서 판정 범위까지 CastTime 동안 점차 차오른다.
	 * 일반 장판은 전체 범위가 한 번에 표시되지만, 저스트가드처럼 '차오른 정도 = 남은 시간'을
	 * 읽고 타이밍을 잡아야 하는 패턴은 이걸 켠다 (docs/09_JUSTGUARD_PATTERN.md).
	 *  - 프로시저럴 메시 예고: XY 스케일 0->1 (중앙에서 바깥으로 확장. 판정 크기는 불변 — T에 전체 도형으로 판정)
	 *  - VFX 예고(TelegraphEffect): "FillRatio"(0~1) User 파라미터로 매 틱 전달 (시스템 쪽에서 소비)
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Aoe|Telegraph")
	bool bTelegraphFill = false;

	/** 수직 판정 허용 오차(cm). |대상.Z - 중심.Z| 가 이 값 이하일 때만 적중 (공중 대상 제외) */
	UPROPERTY(EditDefaultsOnly, Category = "Aoe|Hit", meta = (ClampMin = "0.0"))
	float HeightTolerance = 200.f;

	/** 장판 이동 방식 */
	UPROPERTY(EditDefaultsOnly, Category = "Aoe|Targeting")
	EAoeTargetingMode TargetingMode = EAoeTargetingMode::Fixed;

	/** Homing/Spiral/Straight 진행 속도(cm/s). Homing/Spiral은 타겟을 향한, Straight는 발사 방향으로의 이동 속도. 0이면 정지 */
	UPROPERTY(EditDefaultsOnly, Category = "Aoe|Targeting",
		meta = (EditCondition = "TargetingMode == EAoeTargetingMode::Homing || TargetingMode == EAoeTargetingMode::Spiral || TargetingMode == EAoeTargetingMode::Straight"))
	float HomingSpeed = 400.f;

	/** 나선 궤도 반지름(cm). 진행 경로 기준 옆으로 도는 원의 크기 */
	UPROPERTY(EditDefaultsOnly, Category = "Aoe|Targeting", meta = (EditCondition = "TargetingMode == EAoeTargetingMode::Spiral", ClampMin = "0.0"))
	float SpiralRadius = 200.f;

	/** 나선 회전 각속도(도/초). 값이 클수록 촘촘하게 감아 돈다 */
	UPROPERTY(EditDefaultsOnly, Category = "Aoe|Targeting", meta = (EditCondition = "TargetingMode == EAoeTargetingMode::Spiral"))
	float SpiralAngularSpeed = 360.f;

	/**
	 * 이 거리(cm) 이내로 타겟에 접근하면 나선 반지름이 0으로 수렴(직선으로 빨려들어가며 착탄).
	 * 0이면 반지름이 줄지 않고 끝까지 같은 굵기로 나선 유지.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Aoe|Targeting", meta = (EditCondition = "TargetingMode == EAoeTargetingMode::Spiral", ClampMin = "0.0"))
	float SpiralConvergeDistance = 600.f;

	/** 이 태그를 가진 플레이어는 대상에서 제외 (사망 등). 미설정 시 State.Dead 폴백 */
	UPROPERTY(EditDefaultsOnly, Category = "Aoe|Targeting")
	FGameplayTag DeadTag;

	/** 켜면 판정 도형을 라인으로 그린다(판정 크기 vs 데칼 크기 확인용). 서버에서만 */
	UPROPERTY(EditDefaultsOnly, Category = "Aoe|Debug")
	bool bDrawDebugHitShape = false;

	/** 예고 비주얼 머티리얼 (반투명 경고). 메시가 도형이라 마스킹 불필요 — Two Sided 반투명 빨강 권장. TelegraphEffect 지정 시 무시됨 */
	UPROPERTY(EditDefaultsOnly, Category = "Aoe|Telegraph")
	TObjectPtr<UMaterialInterface> WarningMaterial;

	/**
	 * 지정하면 예고 비주얼을 프로시저럴 메시 대신 이 나이아가라 시스템으로 대체한다.
	 * (예: 번개폭풍 VFX로 위험 표시를 대체). 위치/크기는 자동으로 판정 도형에 맞춰지고
	 * (루트에 부착 -> Homing/Spiral 등 이동도 자동 추적), 도형별 세부 크기는
	 * ConfigureTelegraphEffect 훅에서 나이아가라 User 파라미터로 주입한다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Aoe|Telegraph")
	TObjectPtr<UNiagaraSystem> TelegraphEffect;

	/**
	 * 예고와 별개로, 액터가 살아있는 동안 계속 루트에 붙어 함께 이동하는 '본체' VFX.
	 * (직선 발사 투사체의 번개 몸통 등). BeginPlay에서 스폰되어 소멸 시 함께 정리된다.
	 * TelegraphEffect(예고, 판정 시작 시 제거)와 달리 수명 내내 유지되고 Straight/Homing 이동을 따라간다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Aoe|Body")
	TObjectPtr<UNiagaraSystem> BodyEffect;

	/**
	 * BodyEffect 의 캐스케이드(UParticleSystem) 버전. 토네이도처럼 기존 Cascade 파티클을
	 * 본체로 쓸 때 지정 (Niagara 와 둘 다 지정하면 둘 다 스폰됨).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Aoe|Body")
	TObjectPtr<UParticleSystem> BodyEffectCascade;

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

	/** BodyEffect 로 스폰된 본체 VFX 컴포넌트 (액터 소멸 시 AutoDestroy) */
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> BodyComp;

	/** BodyEffectCascade 로 스폰된 본체 캐스케이드 컴포넌트 */
	UPROPERTY(Transient)
	TObjectPtr<UParticleSystemComponent> BodyCascadeComp;

	// ═══════════════ 내부 로직 ═══════════════

	/** 스폰 원점 정책을 해석해 액터 위치/회전 + AttackCenter + Forward/Right 캐싱 */
	void ResolveOrigin();

	/** 예고 종료 시점(CastTime 경과) 콜백: 예고 제거 -> 첫 판정 -> 유지/소멸 결정 */
	void OnCastFinished();

	/** 액터 파괴 시 행동 오브젝트 정리 훅 호출 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 한 번의 판정: 도형+높이+생존 필터 통과한 플레이어에게 적용 */
	virtual void PerformHitCheck();

	/** 적중 시 행동 디스패치: OnHitEffect 있으면 위임, 없으면 ApplyDamageAndStatus */
	void ApplyEffectsTo(AActor* Target);

	/** 유지 종료 진입점: 행동 오브젝트가 파괴를 가로채면(OnFinish=true) 멈추고, 아니면 DestroyAoeNow */
	virtual void FinishAoe();

	/** 대상 발밑 위치 (Character면 캡슐 절반만큼 내림) */
	static FVector GetFeetLocation(const AActor* Target);

	/** 예고 비주얼 제거 */
	void HideTelegraph();

	/** 차오르는 예고(bTelegraphFill) 갱신: CastTime 진행률만큼 메시 스케일/VFX FillRatio 반영 (매 틱) */
	void UpdateTelegraphFill();

	/** TelegraphEffect 가 지정된 경우 나이아가라 컴포넌트를 루트에 부착 스폰 + ConfigureTelegraphEffect 호출 */
	void BuildTelegraphEffect();

	/** BodyEffect 가 지정된 경우 본체 VFX 를 루트에 부착 스폰 (수명 내내 유지, 이동 추종) */
	void SpawnBodyEffect();

	/** Follow/Homing 이동 처리 */
	void UpdateCenter(float DeltaTime);

	/** 스폰/Follow 시 시전자 기준 위치 (소켓 지정 시 소켓, 아니면 액터 위치) */
	FVector GetCasterOriginLocation() const;

	/**
	 * At 위치의 바닥 Z 를 구한다 (위에서 아래로 트레이스). 성공 시 OutZ 채우고 true.
	 * 오브젝트 타입(WorldStatic/WorldDynamic) 쿼리라 바닥 메시의 Visibility 응답과 무관하게 잡힌다.
	 * (거대 보스 캡슐 높이만큼 위에서 쏘고 충분히 아래까지 내려감)
	 */
	bool TraceGroundZ(const FVector& At, float& OutZ) const;

	/**
	 * At 위치의 바닥 Z 를 항상 반환 (폴백 포함).
	 *  1) TraceGroundZ 성공 시 그 값
	 *  2) 실패 시 시전자(보스) 발밑 Z — 보스는 아레나 바닥에 서 있으므로 바닥 높이와 같다
	 *     (머지 바닥 메시에 콜리전이 없어 트레이스가 아예 안 잡히는 경우 대비)
	 *  3) 시전자도 없으면 At.Z 그대로
	 */
	float ResolveGroundZ(const FVector& At) const;

	/** 예고/본체 메시를 바닥에서 살짝 띄우는 오프셋(cm). Z-파이팅 방지 */
	UPROPERTY(EditDefaultsOnly, Category = "Aoe|Telegraph", meta = (ClampMin = "0.0"))
	float TelegraphZOffset = 5.f;

	/**
	 * 바닥 트레이스 실패 시 폴백 Z(시전자 캡슐 발밑)에 더할 수동 보정(cm).
	 * 보스가 중앙 구덩이에 잠겨 있어 발밑이 실제 아레나 바닥보다 낮으면 그 차이만큼 +로 올린다.
	 * (BP 디폴트에서 맵에 맞게 한 번 맞춰두면 됨. 트레이스가 잡히는 위치에선 사용되지 않음)
	 */
	UPROPERTY(EditAnywhere, Category = "Aoe|Spawn")
	float GroundFallbackZOffset = 0.f;

	/**
	 * 켜면 바닥 트레이스를 아예 건너뛰고 항상 '시전자 발밑 Z + GroundFallbackZOffset' 을 바닥으로 사용.
	 * 트레이스가 엉뚱한 것(투명 볼륨/부착 무기/구덩이 밑바닥 등)을 잡아 높이가 이상할 때
	 * 수동으로 확실하게 제어하는 스위치. (켜면 오프셋이 반드시 적용됨)
	 */
	UPROPERTY(EditAnywhere, Category = "Aoe|Spawn")
	bool bForceGroundFallback = false;

	/**
	 * 켜면 바닥 Z 를 '월드 절대값(AbsoluteGroundZ)' 으로 고정한다. 트레이스/발밑 무시.
	 * 보스가 중앙 구덩이에 잠긴 평평한 아레나처럼 바닥 높이가 일정할 때 가장 확실한 방법.
	 * (로그에서 확인한 실제 바닥 Z 를 그대로 넣으면 됨)
	 */
	UPROPERTY(EditAnywhere, Category = "Aoe|Spawn")
	bool bUseAbsoluteGroundZ = false;

	/** bUseAbsoluteGroundZ 가 켜졌을 때 사용할 바닥의 월드 Z (cm) */
	UPROPERTY(EditAnywhere, Category = "Aoe|Spawn", meta = (EditCondition = "bUseAbsoluteGroundZ"))
	float AbsoluteGroundZ = 0.f;

	/** 대상이 생존 상태인지 (DeadTag 없으면 생존 간주) */
	bool IsAlive(const AActor* Target) const;

	/** PushBack 방향 계산 (Knockback.Direction 기준, 수평 정규화. 퇴화 시 ShapeForward 폴백) */
	FVector ComputeKnockbackDirection(const AActor* Target) const;

	/**
	 * 낙사 금지(bCanCauseFallDeath=false) 넉백의 수평 속도 클램프.
	 * 예상 체공시간으로 수평 이동거리를 추정하고, 밀려나는 경로를 샘플링해
	 * 바닥이 끊기는(파괴 슬라이스/아레나 밖) 지점 '직전'까지만 밀리도록 속도를 줄인다.
	 */
	float ClampPushSpeedToGround(const ACharacter* Char, const FVector& Dir, float HSpeed, float VSpeed) const;

	/** At(발밑 높이) 주변 위 100 ~ 아래 300cm 에 이어진 바닥이 있는지 (구덩이 밑바닥은 바닥으로 안 침) */
	bool HasLandingGroundAt(const FVector& At, const AActor* IgnoreTarget) const;

private:
	/** 현재 액터 회전에서 평면(Z=0) Forward/Right 를 다시 캐싱 (스폰/Follow 회전 시) */
	void CacheShapeAxes();

	FVector ShapeForward = FVector::ForwardVector;	// 스폰 시 캐싱된 평면 전방
	FVector ShapeRight = FVector::RightVector;		// 스폰 시 캐싱된 평면 우측
	FVector LaunchDirection = FVector::ForwardVector;	// Straight 모드 발사 방향(스폰 Forward, 수평)

	FTimerHandle CastTimerHandle;	// 예고->첫 판정
	FTimerHandle TickTimerHandle;	// 유지 중 틱뎀
	FTimerHandle LifeTimerHandle;	// 유지시간 종료

	/** 이번 수명 동안 이미 맞은 대상 (bSingleHitPerTarget 용) */
	UPROPERTY(Transient)
	TSet<TWeakObjectPtr<AActor>> AlreadyHitActors;

	bool bFinished = false;

	/** CastTime 경과(OnCastFinished) 여부. Straight 투사체는 이때부터 전진(예고 중엔 제자리) */
	bool bCastFinished = false;

	/** 시전 시작 시각(월드 초). 차오르는 예고(bTelegraphFill) 진행률 계산용 */
	float CastStartTimeSeconds = 0.f;

	/** CasterTagsOnHit 를 이미 부여했는지 (첫 적중 1회만) */
	bool bCasterHitTagsApplied = false;

	/** 바닥 트레이스 결과를 이번 수명에 1회만 로그 (Follow 매 틱 스팸 방지, 진단용) */
	mutable bool bGroundTraceLogged = false;

	/** Spiral 모드: 나선 궤도의 중심이 되는 직선 진행 위치 (매 틱 타겟 쪽으로 전진) */
	FVector SpiralBasePos = FVector::ZeroVector;

	/** Spiral 모드: 누적 회전각(도) */
	float SpiralAngle = 0.f;
};
