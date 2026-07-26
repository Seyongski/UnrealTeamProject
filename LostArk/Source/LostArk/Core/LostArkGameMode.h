// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "LostArkGameMode.generated.h"

class ALostArkMonsterSpawner;
class ALostArkStagePortalArea;

UCLASS(minimalapi)
class ALostArkGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ALostArkGameMode();

	void OnStagePortalAreaReady();

	UFUNCTION()
	void ExecuteServerTravel();

	/**
	 * 클라이언트가 '이 레벨 로드 완료'를 보고했을 때 호출 (ALostArkPlayerController 경유, 서버 전용).
	 * 기본 규칙은 '기다리지 않는다' — 보고한 클라에게 즉시 진행을 알린다.
	 * 전원이 로드될 때까지 시작을 막아야 하는 레벨(보스 레이드)만 오버라이드한다.
	 */
	virtual void NotifyPlayerLevelLoaded(APlayerController* PC);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnAllMonstersKilled(ALostArkMonsterSpawner* Spawner);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level Travel Settings")
	FString NextLevelURL;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level Travel Settings")
	TSubclassOf<ALostArkStagePortalArea> StagePortalAreaClass;

	UPROPERTY(Transient)
	ALostArkStagePortalArea* SpawnedPortalArea;
};
