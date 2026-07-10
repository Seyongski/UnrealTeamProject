// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Boss/Notifies/AnimNotify_BossSpawnAoe.h"
#include "AnimNotify_BossSpawnRadial.generated.h"

/**
 * 방사형 투사체가 취할 판정 도형. Shape 선택에 따라 에디터에 해당 도형 값만 노출된다.
 * (AoeClass 에는 이 Shape 에 맞는 파생 BP 를 지정해야 파라미터가 실제로 적용된다)
 */
UENUM(BlueprintType)
enum class ERadialShape : uint8
{
	Circle	UMETA(DisplayName = "원형"),
	Rect	UMETA(DisplayName = "직사각형"),
	Sector	UMETA(DisplayName = "부채꼴")
};

/**
 * 한 점에서 N개의 투사체 장판을 방사형으로 동시에 스폰하는 노티파이.
 * (정육각형 방사 = Count 6, 360/6 = 60° 간격으로 여섯 방향 직진)
 *
 * 자급자족 설계: 발사에 필요한 값(속도/사거리/판정주기/시전시간)을 이 노티파이가 전부 갖고
 * 스폰 직후 SetupStraightProjectile 로 강제 주입한다. 따라서
 *  - AoeClass BP 의 SpawnOrigin/TargetingMode 설정이 무엇이든 항상 부채꼴로 발사된다.
 *  - CommonOverride 로 Straight 를 켤 필요가 없다.
 *
 * 도형: Shape 로 Circle / Rect / Sector 중 하나를 고르면 그 도형 값만 에디터에 나타난다.
 *  - AoeClass 에는 고른 Shape 에 맞는 파생 BP(BP_Aoe_Circle / _Rect / _Sector)를 지정할 것.
 *
 * 비주얼: 본체는 AoeClass BP 의 BodyEffect(나이아가라) 또는 BodyEffectCascade(캐스케이드,
 * 토네이도 등)로 지정 — 투사체 이동을 자동으로 따라간다.
 */
UCLASS(meta = (DisplayName = "Boss Spawn Radial"))
class LOSTARK_API UAnimNotify_BossSpawnRadial : public UAnimNotify_BossSpawnAoe
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

protected:
	virtual void ConfigureAoe(ABossPatternActorBase* Aoe) const override;

	// ─── 방사 배치 ───

	/** 방사 개수 (정육각형 = 6) */
	UPROPERTY(EditAnywhere, Category = "Aoe|Radial", meta = (ClampMin = "1"))
	int32 ProjectileCount = 6;

	/** 첫 발사 각(도). 보스 전방=0, 오른쪽 +. 전체 부채를 회전시킬 때 사용 */
	UPROPERTY(EditAnywhere, Category = "Aoe|Radial")
	float StartAngleDeg = 0.f;

	/** 발사 간격(도). 0이면 360/Count 로 자동(정N각형 균등 방사). 부채꼴 방사는 직접 지정 */
	UPROPERTY(EditAnywhere, Category = "Aoe|Radial", meta = (ClampMin = "0.0"))
	float AngleStepDeg = 0.f;

	/** 스폰할 도형. 선택에 따라 아래 도형 값 묶음이 바뀐다 */
	UPROPERTY(EditAnywhere, Category = "Aoe|Radial")
	ERadialShape Shape = ERadialShape::Circle;

	// ─── 발사 (전 도형 공통) ───

	/** 진행 속도(cm/s) */
	UPROPERTY(EditAnywhere, Category = "Aoe|Launch", meta = (ClampMin = "1.0"))
	float Speed = 600.f;

	/** 사거리(cm). 이만큼 날아가면 소멸 (수명 = Range/Speed) */
	UPROPERTY(EditAnywhere, Category = "Aoe|Launch", meta = (ClampMin = "1.0"))
	float Range = 3000.f;

	/** 비행 중 판정 주기(초). 작을수록 촘촘하게 스치는 대상을 잡는다 */
	UPROPERTY(EditAnywhere, Category = "Aoe|Launch", meta = (ClampMin = "0.02"))
	float HitInterval = 0.1f;

	/** 발사 전 대기(초). 0이면 즉시 발사, >0이면 스폰 지점에 예고 표시 후 발사 */
	UPROPERTY(EditAnywhere, Category = "Aoe|Launch", meta = (ClampMin = "0.0"))
	float CastTime = 0.f;

	// ─── 도형: Circle (Shape == Circle 일 때만 표시) ───

	/** 원형 투사체 반지름(cm) */
	UPROPERTY(EditAnywhere, Category = "Aoe|Shape",
		meta = (ClampMin = "0.0", EditCondition = "Shape == ERadialShape::Circle", EditConditionHides))
	float Radius = 150.f;

	/** >0 이면 도넛(안쪽 안전지대) */
	UPROPERTY(EditAnywhere, Category = "Aoe|Shape",
		meta = (ClampMin = "0.0", EditCondition = "Shape == ERadialShape::Circle", EditConditionHides))
	float InnerRadius = 0.f;

	// ─── 도형: Rect (Shape == Rect 일 때만 표시) ───

	/** 전방(진행 방향) 반길이(cm) */
	UPROPERTY(EditAnywhere, Category = "Aoe|Shape",
		meta = (ClampMin = "0.0", EditCondition = "Shape == ERadialShape::Rect", EditConditionHides))
	float RectHalfLength = 300.f;

	/** 측면 반너비(cm) */
	UPROPERTY(EditAnywhere, Category = "Aoe|Shape",
		meta = (ClampMin = "0.0", EditCondition = "Shape == ERadialShape::Rect", EditConditionHides))
	float RectHalfWidth = 100.f;

	/** 중심을 진행 방향으로 미는 오프셋(cm). HalfLength 와 같게 주면 스폰점에서 시작하는 직사각형 */
	UPROPERTY(EditAnywhere, Category = "Aoe|Shape",
		meta = (EditCondition = "Shape == ERadialShape::Rect", EditConditionHides))
	float RectForwardOffset = 0.f;

	// ─── 도형: Sector (Shape == Sector 일 때만 표시) ───

	/** 부채꼴 반지름(cm) */
	UPROPERTY(EditAnywhere, Category = "Aoe|Shape",
		meta = (ClampMin = "0.0", EditCondition = "Shape == ERadialShape::Sector", EditConditionHides))
	float SectorRadius = 300.f;

	/** >0 이면 도넛 조각(안쪽 안전지대) */
	UPROPERTY(EditAnywhere, Category = "Aoe|Shape",
		meta = (ClampMin = "0.0", EditCondition = "Shape == ERadialShape::Sector", EditConditionHides))
	float SectorInnerRadius = 0.f;

	/** 부채꼴 시작각(도). 진행 방향=0, 오른쪽 + */
	UPROPERTY(EditAnywhere, Category = "Aoe|Shape",
		meta = (ClampMin = "-360.0", ClampMax = "360.0", EditCondition = "Shape == ERadialShape::Sector", EditConditionHides))
	float SectorStartAngle = -30.f;

	/** 부채꼴 끝각(도). StartAngle 보다 커야 함 */
	UPROPERTY(EditAnywhere, Category = "Aoe|Shape",
		meta = (ClampMin = "-360.0", ClampMax = "360.0", EditCondition = "Shape == ERadialShape::Sector", EditConditionHides))
	float SectorEndAngle = 30.f;
};
