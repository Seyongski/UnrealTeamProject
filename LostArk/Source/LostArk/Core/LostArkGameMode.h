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
