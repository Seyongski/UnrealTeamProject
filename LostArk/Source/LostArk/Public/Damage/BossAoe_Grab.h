// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Damage/BossAoe_Rect.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectTypes.h"
#include "BossAoe_Grab.generated.h"

class UGameplayEffect;

/** 잡힌 대상 1명의 복구 정보 */
USTRUCT()
struct FGrabbedTargetInfo
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<class ACharacter> Character = nullptr;

	/** GrabbedEffect 로 부여했을 때의 핸들 (해제 시 제거) */
	FActiveGameplayEffectHandle GrabbedGEHandle;

	/** GE 미지정으로 루스 태그 폴백을 썼는지 */
	bool bTagFallback = false;
};

/**
 * 잡기 장판. 경로(사각) 판정에 걸린 플레이어를 '전부' 잡아 보스 소켓에 매단다.
 *
 * 흐름(서버):
 *  예고(CastTime) -> 판정 -> 걸린 전원: Grabbed 상태(GE/태그) + 이동정지 + 캡슐충돌off + 소켓 부착
 *  -> GrabHoldTime 유지 -> 해제: 부착 해제 + 상태 복구 + 데미지 GE + 바깥으로 던지기(넉백)
 *  (파괴된 지형 방향으로 던져지면 낙사 기믹과 연계됨)
 *
 * 역할 경계: 보스 쪽은 태그/GE 부여 + 서버 이동정지/부착까지만.
 *  잡힌 동안의 입력 차단/카메라/애니메이션 반응은 플레이어 쪽(팀원)이
 *  State.Player.Grabbed 태그에 바인딩해서 처리한다.
 */
UCLASS()
class LOSTARK_API ABossAoe_Grab : public ABossAoe_Rect
{
	GENERATED_BODY()

public:
	ABossAoe_Grab();

	/** 잡힘 상태 GE (State.Player.Grabbed 부여, Infinite 권장). 미지정 시 복제 루스 태그 폴백 */
	UPROPERTY(EditDefaultsOnly, Category = "Aoe|Grab")
	TSubclassOf<UGameplayEffect> GrabbedEffect;

	/** 잡힘 태그 (기본 State.Player.Grabbed) */
	UPROPERTY(EditDefaultsOnly, Category = "Aoe|Grab")
	FGameplayTag GrabbedTag;

	/** 잡은 뒤 유지 시간(초). 지나면 자동 해제(데미지+던지기) */
	UPROPERTY(EditDefaultsOnly, Category = "Aoe|Grab", meta = (ClampMin = "0.0"))
	float GrabHoldTime = 2.f;

	/** 보스 스켈레톤의 잡기 소켓 이름들. 잡힌 순서대로 배정(부족하면 순환). 비우면 메시 루트에 부착 */
	UPROPERTY(EditDefaultsOnly, Category = "Aoe|Grab")
	TArray<FName> GrabSocketNames;

	/** 던지기 수평 속도(cm/s). 보스 중심 -> 대상 방향 바깥으로 */
	UPROPERTY(EditDefaultsOnly, Category = "Aoe|Grab")
	float ThrowHorizontalSpeed = 1500.f;

	/** 던지기 수직(위) 속도(cm/s) */
	UPROPERTY(EditDefaultsOnly, Category = "Aoe|Grab")
	float ThrowVerticalSpeed = 500.f;

	/** 조기 해제 (던지기 타이밍을 몽타주 노티파이 등에서 직접 제어할 때 호출) */
	UFUNCTION(BlueprintCallable, Category = "Aoe|Grab")
	void ReleaseAll();

protected:
	/** 데미지 대신 '잡기' 수행 (해제 시점에 Super 로 데미지+상태이상 적용) */
	virtual void ApplyEffectsTo(AActor* Target) override;

	/** 잡은 대상이 있으면 파괴를 유지시간만큼 지연 */
	virtual void FinishAoe() override;

	/** 해제 전에 외부 파괴되면 안전 복구(데미지/던지기 없이) */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void GrabOne(ACharacter* Char);
	void RestoreOne(FGrabbedTargetInfo& Info, bool bWithThrow);

	UPROPERTY(Transient)
	TArray<FGrabbedTargetInfo> GrabbedTargets;

	FTimerHandle ReleaseTimerHandle;
	bool bReleased = false;
};
