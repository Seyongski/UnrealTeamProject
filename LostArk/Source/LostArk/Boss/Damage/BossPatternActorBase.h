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
class UBossAoeEffect;
class UNiagaraSystem;
class UNiagaraComponent;

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

	/** TelegraphEffect 가 지정된 경우 나이아가라 컴포넌트를 루트에 부착 스폰 + ConfigureTelegraphEffect 호출 */
	void BuildTelegraphEffect();

	/** BodyEffect 가 지정된 경우 본체 VFX 를 루트에 부착 스폰 (수명 내내 유지, 이동 추종) */
	void SpawnBodyEffect();

	/** Follow/Homing 이동 처리 */
	void UpdateCenter(float DeltaTime);

	/** 스폰/Follow 시 시전자 기준 위치 (소켓 지정 시 소켓, 아니면 액터 위치) */
	FVector GetCasterOriginLocation() const;

	/** 대상이 생존 상태인지 (DeadTag 없으면 생존 간주) */
	bool IsAlive(const AActor* Target) const;

private:
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

	/** CasterTagsOnHit 를 이미 부여했는지 (첫 적중 1회만) */
	bool bCasterHitTagsApplied = false;

	/** Spiral 모드: 나선 궤도의 중심이 되는 직선 진행 위치 (매 틱 타겟 쪽으로 전진) */
	FVector SpiralBasePos = FVector::ZeroVector;

	/** Spiral 모드: 누적 회전각(도) */
	float SpiralAngle = 0.f;
};
