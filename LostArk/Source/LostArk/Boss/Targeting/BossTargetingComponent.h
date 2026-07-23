// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "BossTargetingComponent.generated.h"

class UAbilitySystemComponent;

/** 타겟 선정 정책 */
UENUM(BlueprintType)
enum class ETargetSelectPolicy : uint8
{
	Nearest,	// 최근접
	Farthest,	// 최원거리
	Random		// 무작위
};

/**
 * 보스 타겟팅 컴포넌트.
 * - 생존 플레이어(사망 태그 없는) 중 정책에 따라 타겟 1명 선정 (서버 전용)
 * - CurrentTarget 복제 (코스메틱/기타 시스템이 클라에서도 참조 가능)
 * - TrackTarget 태그가 보스에 있는 동안만 서버에서 액터를 타겟 방향으로 회전 (캡슐+메시 함께)
 *   -> 회전은 액터 회전 복제로 클라에 전파되므로 클라는 계산 안 함
 */
UCLASS(ClassGroup = (Boss), meta = (BlueprintSpawnableComponent))
class LOSTARK_API UBossTargetingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBossTargetingComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 생존 플레이어 중 정책에 따라 타겟 선정 (서버). 패턴 시작 시 브레인이 호출 */
	UFUNCTION(BlueprintCallable, Category = "Boss|Targeting")
	AActor* SelectTarget(ETargetSelectPolicy Policy = ETargetSelectPolicy::Nearest);

	UFUNCTION(BlueprintPure, Category = "Boss|Targeting")
	AActor* GetCurrentTarget() const { return CurrentTarget; }

	/**
	 * 직전 추적 회전과 같은 방향으로 초당 TurnRate(도)만큼 회전 시작 (서버).
	 * 방향 부호는 TrackTarget 구간에서 캡처된 LastTrackTurnSign 을 재사용
	 * -> 추적이 시계방향이었으면 이 회전도 시계방향.
	 * @param TurnRateDegPerSec 초당 회전량(도). 부호 없이 크기만
	 * @param FallbackSign      추적 이력이 없어 방향 미정일 때 쓸 부호 (+1/-1)
	 */
	UFUNCTION(BlueprintCallable, Category = "Boss|Targeting")
	void BeginScriptedTurn(float TurnRateDegPerSec, float FallbackSign = 1.f);

	/** 스크립트 회전 종료 */
	UFUNCTION(BlueprintCallable, Category = "Boss|Targeting")
	void EndScriptedTurn();

	/** 마지막 추적 회전 방향 부호 (+1/-1, 0=미정). 추적 세션 시작 시 리셋 후 첫 유효 프레임에 확정 */
	UFUNCTION(BlueprintPure, Category = "Boss|Targeting")
	float GetLastTrackTurnSign() const { return LastTrackTurnSign; }

	/** 사망 판정 태그. 이 태그를 가진 플레이어는 후보에서 제외 (미설정 시 BeginPlay에서 State.Dead 로 폴백) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Targeting")
	FGameplayTag DeadTag;

	/** 추적 회전 스위치 태그. 보스 ASC에 이 태그가 있을 때만 회전 (NotifyState가 토글) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Targeting")
	FGameplayTag TrackTargetTag;

	/** 회전 보간 속도 (클수록 빠르게 돎) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Targeting")
	float RotationInterpSpeed = 6.f;

protected:
	virtual void BeginPlay() override;

private:
	/** 사망 태그가 없으면 생존으로 간주 (판정 불가 시 후보 포함) */
	bool IsAlive(const AActor* PlayerActor) const;

	/** 서버에서 TrackTarget 태그가 있을 때 액터를 타겟 방향으로 보간 회전 */
	void UpdateFacing(float DeltaTime);

	/** 스크립트 회전: 캡처된 부호 x TurnRate 로 등속 회전 (추적 중이면 추적이 우선) */
	void UpdateScriptedTurn(float DeltaTime);

	UAbilitySystemComponent* GetOwnerASC() const;

	UPROPERTY(Replicated, Transient)
	TObjectPtr<AActor> CurrentTarget;

	/** 마지막 추적 회전 방향 (+1/-1, 0=미정). 추적 세션 첫 유효 프레임에 확정 */
	float LastTrackTurnSign = 0.f;

	/** 직전 틱에 추적 중이었는지 (추적 시작 엣지 감지 -> 방향 부호 리셋용) */
	bool bWasTracking = false;

	/** 스크립트 회전 활성 여부 (노티파이가 Begin/End 로 토글) */
	bool bScriptedTurnActive = false;

	/** 스크립트 회전 속도(도/초, 크기만). 부호는 LastTrackTurnSign */
	float ScriptedTurnRate = 0.f;
};
