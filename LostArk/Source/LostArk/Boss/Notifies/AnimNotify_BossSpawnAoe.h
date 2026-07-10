// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Boss/Damage/BossPatternActorBase.h"	// FBossAoeCommonOverride
#include "Boss/Damage/BossAoeGrabEffect.h"		// FBossGrabOverride
#include "AnimNotify_BossSpawnAoe.generated.h"

class ABossPatternActorBase;
class UNiagaraSystem;
class UParticleSystem;

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

	/**
	 * 스폰 공통 준비: 서버 권위/월드/타겟/베이스 스폰 트랜스폼(소켓·오프셋·스케일 반영)을 계산.
	 * 방사형 등 여러 개를 스폰하는 서브클래스가 재사용. 스폰 불가(클라/월드 없음 등)면 false.
	 */
	bool PrepareSpawn(USkeletalMeshComponent* MeshComp, class UWorld*& OutWorld,
		AActor*& OutBoss, AActor*& OutTarget, FTransform& OutBaseTM) const;

	/**
	 * 주어진 트랜스폼으로 AoeClass 액터 1개를 Deferred 스폰 + InitAoe/오버라이드/ConfigureAoe/FinishSpawning.
	 * 방사형 노티파이가 방향만 바꿔 여러 번 호출한다.
	 */
	ABossPatternActorBase* SpawnAoeActor(class UWorld* World, AActor* Boss,
		AActor* Target, const FTransform& SpawnTM) const;

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

	/**
	 * 켜면 아래 값으로 잡기 행동(OnHitEffect가 UBossAoeGrabEffect 인 경우)의 파라미터를 이 스폰 1회에만 덮어씀.
	 * 같은 도형+잡기 BP를 몽타주마다 다른 소켓/유지시간/방향으로 재사용할 때 사용
	 * (BP 클래스 디폴트는 그대로 유지됨). OnHitEffect 가 잡기가 아니면 무시된다.
	 */
	UPROPERTY(EditAnywhere, Category = "Aoe|Grab")
	bool bOverrideGrab = false;

	/** 잡기 파라미터 오버라이드 (bOverrideGrab 켤 때만 적용) */
	UPROPERTY(EditAnywhere, Category = "Aoe|Grab", meta = (EditCondition = "bOverrideGrab"))
	FBossGrabOverride GrabOverride;

	/** 초기 스폰 트랜스폼 기준 소켓 (지정 시 이 소켓 위치, 아니면 보스 액터 위치) */
	UPROPERTY(EditAnywhere, Category = "Aoe")
	FName SpawnSocketName = NAME_None;

	/** 스폰 위치 오프셋 (소켓/보스 로컬 기준) */
	UPROPERTY(EditAnywhere, Category = "Aoe")
	FVector LocationOffset = FVector::ZeroVector;

	/**
	 * 본체 VFX(나이아가라)를 이 노티파이에서 지정. 켜면 AoeClass BP 의 BodyEffect 를 덮어쓴다.
	 * 같은 장판 BP 를 패턴마다 다른 몸통 이펙트로 쓰고 싶을 때 사용(예: 뻗어나가는 원장판을
	 * 빨간 경고 대신 토네이도 등 이펙트로). 비우면 BP 기본값 유지.
	 */
	UPROPERTY(EditAnywhere, Category = "Aoe|Body")
	TObjectPtr<UNiagaraSystem> BodyEffectOverride = nullptr;

	/** 본체 VFX(캐스케이드 UParticleSystem)를 이 노티파이에서 지정. 토네이도 등 기존 캐스케이드 에셋용. 비우면 BP 기본값 유지 */
	UPROPERTY(EditAnywhere, Category = "Aoe|Body")
	TObjectPtr<UParticleSystem> BodyEffectCascadeOverride = nullptr;
};
