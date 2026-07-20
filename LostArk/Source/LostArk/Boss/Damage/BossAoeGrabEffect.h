// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Boss/Damage/BossAoeEffect.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectTypes.h"
#include "BossAoeGrabEffect.generated.h"

class UGameplayEffect;
class ACharacter;
class ABossPatternActorBase;

/** 다중 잡기 시 소켓 배정 순서 */
UENUM(BlueprintType)
enum class EGrabSocketOrder : uint8
{
	/** 감지 순서 그대로 (정렬 안 함) */
	DetectionOrder		UMETA(DisplayName = "감지 순서"),
	/** 전방 기준 각도 오름차순으로 소켓 배정 */
	Clockwise			UMETA(DisplayName = "시계방향"),
	/** 전방 기준 각도 내림차순으로 소켓 배정 */
	CounterClockwise	UMETA(DisplayName = "반시계방향")
};

/**
 * 잡기 파라미터 오버라이드. 몽타주 노티파이 인스턴스가 스폰 직후 적용해
 * 같은 BP(도형+잡기)를 여러 몽타주에서 서로 다른 소켓/유지시간/방향으로 쓸 수 있게 한다.
 * (BP 클래스 디폴트는 건드리지 않음 - OnHitEffect 는 Instanced 라 스폰마다 복제된 개별 인스턴스)
 */
USTRUCT(BlueprintType)
struct FBossGrabOverride
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Grab")
	TArray<FName> GrabSocketNames;

	UPROPERTY(EditAnywhere, Category = "Grab")
	EGrabSocketOrder SocketOrder = EGrabSocketOrder::DetectionOrder;

	UPROPERTY(EditAnywhere, Category = "Grab")
	bool bAutoRelease = true;

	UPROPERTY(EditAnywhere, Category = "Grab", meta = (ClampMin = "0.0", EditCondition = "bAutoRelease"))
	float GrabHoldTime = 2.f;

	UPROPERTY(EditAnywhere, Category = "Grab", meta = (ClampMin = "1.0", EditCondition = "!bAutoRelease"))
	float FailsafeReleaseTime = 10.f;

	UPROPERTY(EditAnywhere, Category = "Grab")
	float ThrowHorizontalSpeed = 1500.f;

	UPROPERTY(EditAnywhere, Category = "Grab")
	float ThrowVerticalSpeed = 500.f;
};

/** 잡힌 대상 1명의 복구 정보 */
USTRUCT()
struct FGrabbedTargetInfo
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<ACharacter> Character = nullptr;

	/** GrabbedEffect 로 부여했을 때의 핸들 (해제 시 제거) */
	FActiveGameplayEffectHandle GrabbedGEHandle;

	/** GE 미지정으로 루스 태그 폴백을 썼는지 */
	bool bTagFallback = false;

	/** 잡힐 때 전방 기준 각도(도). 소켓 배정 정렬용 */
	float Angle = 0.f;
};

/**
 * 잡기 행동. 판정에 걸린 플레이어를 '전부' 잡아 보스 소켓에 매단다.
 * 도형과 무관하므로 원/사각/부채꼴 어떤 장판 BP 에도 OnHitEffect 로 지정 가능.
 *
 * 흐름(서버):
 *  판정 -> 걸린 전원: Grabbed 상태(GE/태그) + 이동정지 + 캡슐충돌off + 소켓 부착
 *  -> 장판 수명 종료(OnFinish로 파괴 지연) -> GrabHoldTime 유지
 *  -> 해제: 부착 해제 + 상태 복구 + 데미지(Aoe->ApplyDamageAndStatus) + 바깥으로 던지기(넉백) -> 장판 파괴
 *
 * 주의: 예고 후 1회 판정으로 잡으려면 장판 BP 의 Duration=0, bSingleHitPerTarget=true 로 둘 것.
 * 역할 경계: 보스 쪽은 태그/GE + 서버 이동정지/부착까지. 잡힌 동안 입력/카메라/애니는 플레이어 쪽이
 *  State.Player.Grabbed 태그에 바인딩해서 처리.
 */
UCLASS(DisplayName = "잡기 (Grab)")
class LOSTARK_API UBossAoeGrabEffect : public UBossAoeEffect
{
	GENERATED_BODY()

public:
	UBossAoeGrabEffect();

	/** 잡힘 상태 GE (State.Player.Grabbed 부여, Infinite 권장). 미지정 시 복제 루스 태그 폴백 */
	UPROPERTY(EditDefaultsOnly, Category = "Grab")
	TSubclassOf<UGameplayEffect> GrabbedEffect;

	/** 잡힘 태그 (기본 State.Player.Grabbed) */
	UPROPERTY(EditDefaultsOnly, Category = "Grab")
	FGameplayTag GrabbedTag;

	/**
	 * true : 장판 수명 종료 후 GrabHoldTime 지나면 자동 해제(데미지+던지기)
	 * false: 몽타주의 'Boss Grab Release' 노티파이가 해제 타이밍을 제어.
	 *        (원하는 애니메이션 프레임까지 손에 붙잡아뒀다가 노티파이 시점에 던짐.
	 *         노티파이가 안 오는 사고 대비로 FailsafeReleaseTime 후엔 강제 해제)
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Grab")
	bool bAutoRelease = true;

	/** 잡은 뒤 유지 시간(초). 지나면 자동 해제(데미지+던지기). bAutoRelease=true 일 때만 */
	UPROPERTY(EditDefaultsOnly, Category = "Grab", meta = (ClampMin = "0.0", EditCondition = "bAutoRelease"))
	float GrabHoldTime = 2.f;

	/** 노티파이 해제 모드에서 노티파이가 끝내 안 오면 이 시간 후 강제 해제 (잡힌 채 방치 방지) */
	UPROPERTY(EditDefaultsOnly, Category = "Grab", meta = (ClampMin = "1.0", EditCondition = "!bAutoRelease"))
	float FailsafeReleaseTime = 10.f;

	/** 보스 스켈레톤의 잡기 소켓 이름들. SocketOrder 순서대로 배정(부족하면 순환). 비우면 메시 루트에 부착 */
	UPROPERTY(EditDefaultsOnly, Category = "Grab")
	TArray<FName> GrabSocketNames;

	/**
	 * 다중 잡기 시 소켓 배정 순서.
	 * 시계/반시계면 잡힌 대상을 전방 기준 각도로 정렬해 GrabSocketNames 순서대로 붙인다
	 * (스윕 방향과 손-소켓 배치가 맞게). 좌우가 반대로 붙으면 시계<->반시계만 바꾸면 됨.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Grab")
	EGrabSocketOrder SocketOrder = EGrabSocketOrder::DetectionOrder;

	/** 던지기 수평 속도(cm/s). 보스 중심 -> 대상 방향 바깥으로 */
	UPROPERTY(EditDefaultsOnly, Category = "Grab")
	float ThrowHorizontalSpeed = 1500.f;

	/** 던지기 수직(위) 속도(cm/s) */
	UPROPERTY(EditDefaultsOnly, Category = "Grab")
	float ThrowVerticalSpeed = 500.f;

	// ─── UBossAoeEffect ───
	virtual void OnHit(ABossPatternActorBase* Aoe, AActor* Target) override;
	virtual bool OnFinish(ABossPatternActorBase* Aoe) override;
	virtual void OnEndPlay(ABossPatternActorBase* Aoe) override;

	/** 몽타주 노티파이가 스폰 직후 호출: 이 인스턴스만 파라미터 덮어쓰기 (BP 클래스 디폴트는 불변) */
	void ApplyOverride(const FBossGrabOverride& Override);

	/** 잡은 대상이 있는지 (해제 노티파이가 대상 있는 잡기만 골라 해제) */
	FORCEINLINE bool HasGrabbedTargets() const { return GrabbedTargets.Num() > 0; }

	/** 전원 해제(데미지+던지기) 후 장판 파괴. 타이머/'Boss Grab Release' 노티파이가 호출 */
	void ReleaseAll();

private:
	void GrabOne(ABossPatternActorBase* Aoe, ACharacter* Char);
	void RestoreOne(ABossPatternActorBase* Aoe, FGrabbedTargetInfo& Info, bool bWithThrow);

	/** SocketOrder가 시계/반시계면 각도로 정렬 후 소켓 재배정 (모든 잡기 완료 후 OnFinish에서 호출) */
	void AssignSocketsByOrder(ABossPatternActorBase* Aoe);

	UPROPERTY(Transient)
	TArray<FGrabbedTargetInfo> GrabbedTargets;

	/** ReleaseAll 타이머 콜백이 참조할 소유 장판 */
	UPROPERTY(Transient)
	TWeakObjectPtr<ABossPatternActorBase> OwningAoe;

	FTimerHandle ReleaseTimerHandle;
	bool bReleased = false;
};
