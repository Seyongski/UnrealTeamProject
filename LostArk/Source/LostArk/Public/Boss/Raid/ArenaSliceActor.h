// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArenaSliceActor.generated.h"

class UStaticMeshComponent;

/**
 * 피자 조각 바닥 1개. 레벨에 SliceCount 개 배치하고 SliceIndex 를 각각 지정.
 * (관례: 액터 위치 = 아레나 중심, 메시를 조각 방향으로 회전/오프셋)
 *
 * 파괴 상태는 GameState의 복제 마스크가 단일 진실 -> 서버/클라/늦은 참여 모두
 * OnArenaSlicesChanged / 현재 마스크로 같은 상태를 적용한다.
 *
 * 파괴 시:
 *  - 바닥 메시 숨김 + 콜리전 제거 -> 넉백으로 날아온 플레이어는 그대로 낙하 (KillVolume이 처리)
 *  - NavModifier(NavArea_Null) 동적 생성 -> 클릭 이동(내비 경로)이 이 구역을 통과/도착 못 함
 *    (프로젝트 세팅: Navigation Mesh Runtime Generation = Dynamic 필요)
 */
UCLASS()
class LOSTARK_API AArenaSliceActor : public AActor
{
	GENERATED_BODY()

public:
	AArenaSliceActor();

	/** 이 조각의 인덱스 (GameState 마스크의 비트 위치) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Arena")
	int32 SliceIndex = 0;

	/** 파괴 연출용 BP 훅 (파티클/사운드/크랙 메시 교체 등) */
	UFUNCTION(BlueprintImplementableEvent, Category = "Arena")
	void OnSliceDestroyedFX();

protected:
	virtual void BeginPlay() override;

	/** 바닥 메시 (걷는 면. 콜리전 BlockAll) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena")
	TObjectPtr<UStaticMeshComponent> FloorMesh;

private:
	/** GameState가 아직 없으면(클라 초기화 순서) 잠시 후 재시도 */
	void TryBindToGameState();

	UFUNCTION()
	void HandleSlicesChanged();

	void ApplyDestroyedState();

	FTimerHandle BindRetryTimer;
	bool bDestroyedApplied = false;
};
