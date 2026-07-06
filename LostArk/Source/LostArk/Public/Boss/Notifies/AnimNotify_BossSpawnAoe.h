// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Damage/BossPatternActorBase.h"	// FBossAoeCommonOverride
#include "AnimNotify_BossSpawnAoe.generated.h"

class ABossPatternActorBase;

/**
 * 몽타주 특정 프레임에 보스 AOE 장판 액터를 스폰하는 노티파이.
 * (망치 내려찍기 타점, 시전 완료 순간 등 애니메이터가 프레임을 직접 지정)
 *
 * - 서버 권위에서만 스폰 (복제로 클라에 전파)
 * - 시전자 = 몽타주 소유 액터(보스), 타겟 = 보스 타겟팅 컴포넌트의 현재 타겟
 * - 스폰 위치의 세부 정책(캡슐/소켓/타겟/지면)은 AoeClass 의 SpawnOrigin 이 결정.
 *   이 노티파이는 초기 트랜스폼(소켓/오프셋)만 잡아주고 InitAoe 로 런타임 값 주입.
 */
UCLASS(meta = (DisplayName = "Boss Spawn AOE"))
class LOSTARK_API UAnimNotify_BossSpawnAoe : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

protected:
	/**
	 * 스폰 직후(InitAoe 후, BeginPlay 전) 도형별 파라미터를 주입하는 훅.
	 * 도형별 노티파이 서브클래스가 오버라이드해서 각 패턴 값(각도/반지름 등)을 세팅.
	 * (BP_AoeSector 1개를 공유하고, 값은 이 노티파이 인스턴스가 패턴마다 다르게 넣는다)
	 */
	virtual void ConfigureAoe(ABossPatternActorBase* Aoe) const {}

	/** 스폰할 장판 액터 클래스 (BP_Aoe_Circle / _Rect / _Sector ...) */
	UPROPERTY(EditAnywhere, Category = "Aoe")
	TSubclassOf<ABossPatternActorBase> AoeClass;

	/** 공격력계수. GE의 SetByCaller(Data.Damage)로 전달 (InitAoe) */
	UPROPERTY(EditAnywhere, Category = "Aoe")
	float DamageCoefficient = 1.f;

	/** 켜면 아래 공통값으로 액터 BP 기본값을 덮어씀 (패턴별 시전/유지/틱 조정) */
	UPROPERTY(EditAnywhere, Category = "Aoe|Common")
	bool bOverrideCommon = false;

	/** 패턴별 공통값 (bOverrideCommon 켤 때만 적용) */
	UPROPERTY(EditAnywhere, Category = "Aoe|Common", meta = (EditCondition = "bOverrideCommon"))
	FBossAoeCommonOverride CommonOverride;

	/** 초기 스폰 트랜스폼 기준 소켓 (지정 시 이 소켓 위치, 아니면 보스 액터 위치) */
	UPROPERTY(EditAnywhere, Category = "Aoe")
	FName SpawnSocketName = NAME_None;

	/** 스폰 위치 오프셋 (소켓/보스 로컬 기준) */
	UPROPERTY(EditAnywhere, Category = "Aoe")
	FVector LocationOffset = FVector::ZeroVector;
};
