// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Boss/Notifies/AnimNotify_BossSpawnAoe.h"
#include "AnimNotify_BossSpawnRadial.generated.h"

/**
 * 한 점에서 N개의 투사체를 방사형으로 동시에 스폰하는 노티파이.
 * (정육각형 방사 = Count 6, 360/6 = 60° 간격으로 여섯 꼭짓점 방향으로 직진)
 *
 * - 모든 투사체는 보스(또는 소켓) 한 지점에서 스폰되고, 각각 60°씩 회전된 방향을 바라본다.
 * - AoeClass 는 TargetingMode=Straight(직선 발사) 로 설정된 투사체(BP_Aoe_Circle 등)를 지정할 것.
 *   각 스폰의 회전이 곧 발사 방향(ShapeForward)이 되므로, 투사체 BP 의 SpawnOrigin 은
 *   반드시 'SpawnTransform' 이어야 한다(CasterLocation 등은 회전을 보스 것으로 덮어써 전부 같은 방향이 됨).
 * - 추적하지 않고 쭉 뻗어나가다 Duration 후 소멸, 오버랩 대상에 데미지+감전(StatusEffects) 적용.
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

	/** 방사 개수 (정육각형 = 6) */
	UPROPERTY(EditAnywhere, Category = "Aoe|Radial", meta = (ClampMin = "1"))
	int32 ProjectileCount = 6;

	/** 첫 발사 각(도). 보스 전방=0, 오른쪽 +. 전체 부채를 회전시킬 때 사용 */
	UPROPERTY(EditAnywhere, Category = "Aoe|Radial")
	float StartAngleDeg = 0.f;

	/** 발사 간격(도). 0이면 360/Count 로 자동(정N각형 균등 방사). 부채꼴 방사는 직접 지정 */
	UPROPERTY(EditAnywhere, Category = "Aoe|Radial", meta = (ClampMin = "0.0"))
	float AngleStepDeg = 0.f;

	/** 투사체(원형 본체) 반지름(cm). AoeClass 가 Circle 파생일 때 판정 반지름으로 주입 */
	UPROPERTY(EditAnywhere, Category = "Aoe|Radial", meta = (ClampMin = "0.0"))
	float Radius = 150.f;
};
