// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayAbilitySpec.h"
#include "BossPatternTypes.h"
#include "BossPatternComponent.generated.h"

class UBossPhaseDataAsset;
class UBossPatternAbility;
class UPatternDataAsset;
class UAbilitySystemComponent;

/**
 * 보스 패턴 흐름을 구동하는 브레인.
 * - 페이즈별 패턴 그래프(UBossPhaseDataAsset)를 들고 있다가 체력에 따라 교체
 * - 패턴 실행은 GAS(UBossPatternAbility)에 위임하고, 결과를 받아 다음 노드를 고른다
 * - 체력 전환은 '지연' 처리: 현재 패턴을 끝까지 실행한 뒤 전환 기믹->다음 페이즈
 */
UCLASS(ClassGroup = (Boss), meta = (BlueprintSpawnableComponent))
class LOSTARK_API UBossPatternComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBossPatternComponent();

	/** 페이즈 목록. EnterHealthPercent 내림차순(100,75,50,25)으로 넣을 것 -> index0 = 1페이즈 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Pattern")
	TArray<TObjectPtr<UBossPhaseDataAsset>> Phases;

	/** 데이터 주도 패턴에 쓸 기본 어빌리티 클래스 (현재는 항상 이걸 사용. 패턴별 AbilityOverride 는 향후 확장 지점) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Pattern")
	TSubclassOf<UBossPatternAbility> DefaultPatternAbility;

	/** 전투 시작 -> 1페이즈부터 흐름 시작 */
	UFUNCTION(BlueprintCallable, Category = "Boss|Pattern")
	void StartCombat();

	/** 보스가 GAS 체력 변화 델리게이트에서 호출. Percent = 0~100. 전환은 예약만 하고 지연 실행 */
	UFUNCTION(BlueprintCallable, Category = "Boss|Pattern")
	void NotifyHealthPercent(float HealthPercent);

	/** 현재 실행할 패턴 데이터 (어빌리티가 읽어감) */
	UPatternDataAsset* GetActivePatternData() const { return ActivePatternData; }

	/** 패턴 어빌리티가 종료되면서 결과를 보고 (다음 노드/페이즈 결정) */
	void OnPatternAbilityFinished(EPatternResult Result);

protected:
	virtual void BeginPlay() override;

private:
	UAbilitySystemComponent* GetASC() const;

	void EnterPhase(int32 PhaseIndex);
	void ActivateNode(FName NodeId);
	void RunPatternData(UPatternDataAsset* Data);
	void ActivateAbilityNow();

	UPROPERTY(Transient)
	TObjectPtr<UPatternDataAsset> ActivePatternData;

	int32 CurrentPhaseIndex = INDEX_NONE;
	int32 PendingPhaseIndex = INDEX_NONE;	// 예약된 전환 대상 (현재 패턴 종료 후 반영)
	FName CurrentNodeId;
	bool bRunningTransition = false;		// 지금 실행 중인 게 페이즈 전환 기믹인지

	FGameplayAbilitySpecHandle PatternAbilityHandle;
	FTimerHandle NextActivateTimer;
};
