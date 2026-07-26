#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "LostArkGameplayAbility.generated.h"

UENUM(BlueprintType)
enum class EDamageShape : uint8
{
	Sphere UMETA(DisplayName = "Sphere"),
	Box    UMETA(DisplayName = "Box"),
	Cone   UMETA(DisplayName = "Cone")
};

USTRUCT(BlueprintType)
struct FDamageShapeParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage Shape")
	EDamageShape ShapeType = EDamageShape::Sphere;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage Shape", meta = (EditCondition = "ShapeType == EDamageShape::Sphere || ShapeType == EDamageShape::Cone"))
	float Radius = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage Shape", meta = (EditCondition = "ShapeType == EDamageShape::Box"))
	FVector BoxExtent = FVector(100.f, 50.f, 50.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage Shape", meta = (EditCondition = "ShapeType == EDamageShape::Box || ShapeType == EDamageShape::Cone"))
	float ForwardOffset = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage Shape", meta = (EditCondition = "ShapeType == EDamageShape::Cone"))
	float ConeAngleDegrees = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage Amount")
	float DamageCoefficient = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage Shape|Debug")
	bool bShowDebugLines = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage Shape")
	float ZTolerance = 200.f;
};

UCLASS()
class LOSTARK_API ULostArkGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	ULostArkGameplayAbility();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability|Damage")
	TSubclassOf<class UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability|Damage", meta = (ShowOnlyInnerProperties))
	FDamageShapeParams DamageShapeParams;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability|Combat")
	bool bIsCounterSkill = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ability|Damage", meta = (ClampMin = "0.0"))
	float StaggerAmount = 10.f;

	void ApplyDamageShape(FVector Origin, FRotator Rotation, AActor* InstigatorActor);

	UFUNCTION()
	virtual void OnHitCheckReceived(FGameplayEventData Payload);

	void SetupHitCheckListener();

	/**
	 * 대시/순간이동 목적지 안전 보정 (대시 계열 스킬 공용).
	 * 1) 벽(WorldStatic/Dynamic)과 '보스'(플레이어/몬스터가 아닌 폰) 앞에서 멈춰 보스를 밀지 않게 하고,
	 * 2) 이동 경로 발밑에 아레나 바닥이 있는 마지막 지점까지만 허용해 플랫폼 밖(맵 뚫기)으로 못 나가게 클램프.
	 * 다른 플레이어/몬스터 폰은 통과(게임플레이 유지). 서버/클라 어디서 불러도 동일 판정.
	 */
	FVector ComputeSafeDashDestination(class ACharacter* AvatarChar, const FVector& Start, const FVector& DesiredEnd) const;
};

