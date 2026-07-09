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

	void ApplyDamageShape(FVector Origin, FRotator Rotation, AActor* InstigatorActor);

	UFUNCTION()
	virtual void OnHitCheckReceived(FGameplayEventData Payload);

	void SetupHitCheckListener();
};
