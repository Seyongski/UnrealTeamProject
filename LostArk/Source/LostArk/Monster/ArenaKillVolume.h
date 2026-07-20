// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArenaKillVolume.generated.h"

class UBoxComponent;
class UGameplayEffect;

/**
 * 낙사 판정 볼륨. 파괴된 지형 아래(아레나 밑 전체)에 크게 배치.
 * 서버에서 폰이 들어오면 FallDeathEffect(사망 GE)를 적용한다.
 * GE 미지정 시 State.Dead 루스 태그 폴백 (사망 처리 자체는 플레이어 쪽 담당).
 */
UCLASS()
class LOSTARK_API AArenaKillVolume : public AActor
{
	GENERATED_BODY()

public:
	AArenaKillVolume();

	/** 낙사 시 적용할 GE (체력 0 / State.Dead 부여 등 — 플레이어 팀과 합의한 애셋) */
	UPROPERTY(EditAnywhere, Category = "Arena")
	TSubclassOf<UGameplayEffect> FallDeathEffect;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Arena")
	TObjectPtr<UBoxComponent> Box;

private:
	UFUNCTION()
	void OnBoxBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};
