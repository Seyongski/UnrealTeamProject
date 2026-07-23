#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LostArkMonsterSpawner.generated.h"

class ALostArkMonster;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAllMonstersKilledSignature, ALostArkMonsterSpawner*, Spawner);

UCLASS()
class LOSTARK_API ALostArkMonsterSpawner : public AActor
{
	GENERATED_BODY()
	
public:	
	ALostArkMonsterSpawner();

	UPROPERTY(BlueprintAssignable, Category = "Spawner Events")
	FOnAllMonstersKilledSignature OnAllMonstersKilled;

protected:
	virtual void BeginPlay() override;

	void SpawnMonsterBatch();

	UFUNCTION()
	void OnMonsterKilled(ALostArkMonster* KilledMonster);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner Settings")
	TSoftClassPtr<ALostArkMonster> MonsterClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner Settings", meta = (ClampMin = "1"))
	int32 TotalSpawnLimit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner Settings", meta = (ClampMin = "1"))
	int32 SpawnCountPerBatch;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner Settings", meta = (ClampMin = "1"))
	int32 MaxActiveMonsters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner Settings", meta = (ClampMin = "0.0"))
	float SpawnRadius;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner Settings", meta = (ClampMin = "0.1"))
	float SpawnInterval;

private:
	FTimerHandle SpawnTimerHandle;

	UPROPERTY(VisibleAnywhere, Category = "Spawner State")
	int32 SpawnedCount;

	UPROPERTY(VisibleAnywhere, Category = "Spawner State")
	int32 KilledCount;

	UPROPERTY(VisibleAnywhere, Category = "Spawner State")
	TArray<ALostArkMonster*> ActiveMonsters;
};


