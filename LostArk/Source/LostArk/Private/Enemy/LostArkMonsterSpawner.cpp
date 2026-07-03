#include "LostArkMonsterSpawner.h"
#include "LostArkMonster.h"
#include "LostArkObjectPoolSubsystem.h"
#include "NavigationSystem.h"
#include "TimerManager.h"
#include "Engine/World.h"

ALostArkMonsterSpawner::ALostArkMonsterSpawner()
{
	PrimaryActorTick.bCanEverTick = false;

	TotalSpawnLimit = 100;
	SpawnCountPerBatch = 5;
	MaxActiveMonsters = 10;
	SpawnRadius = 1000.f;
	SpawnInterval = 2.0f;

	SpawnedCount = 0;
	KilledCount = 0;
}

void ALostArkMonsterSpawner::BeginPlay()
{
	Super::BeginPlay();

	GetWorldTimerManager().SetTimer(SpawnTimerHandle, this, &ALostArkMonsterSpawner::SpawnMonsterBatch, SpawnInterval, true);

	SpawnMonsterBatch();
}

void ALostArkMonsterSpawner::SpawnMonsterBatch()
{
	if (!MonsterClass || SpawnedCount >= TotalSpawnLimit)
	{
		return;
	}

	ULostArkObjectPoolSubsystem* PoolSubsystem = GetWorld()->GetSubsystem<ULostArkObjectPoolSubsystem>();
	if (!PoolSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("[Spawner] Failed to retrieve Object Pool Subsystem!"));
		return;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());

	int32 BatchSpawnCount = 0;

	while (ActiveMonsters.Num() < MaxActiveMonsters && 
		   SpawnedCount < TotalSpawnLimit && 
		   BatchSpawnCount < SpawnCountPerBatch)
	{
		FVector SpawnLocation = GetActorLocation();

		if (NavSys)
		{
			FNavLocation RandomNavLocation;
			if (NavSys->GetRandomReachablePointInRadius(GetActorLocation(), SpawnRadius, RandomNavLocation))
			{
				SpawnLocation = RandomNavLocation.Location;
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[Spawner] Failed to find a valid location on NavMesh in radius. Falling back to actor origin."));
			}
		}

		FRotator SpawnRotation = FRotator(0.f, FMath::RandRange(0.f, 360.f), 0.f);

		AActor* AcquiredActor = PoolSubsystem->AcquireActor(MonsterClass, SpawnLocation, SpawnRotation);
		ALostArkMonster* Monster = Cast<ALostArkMonster>(AcquiredActor);

		if (IsValid(Monster))
		{
			Monster->OnMonsterKilled.AddDynamic(this, &ALostArkMonsterSpawner::OnMonsterKilled);
			
			ActiveMonsters.Add(Monster);
			SpawnedCount++;
			BatchSpawnCount++;

			UE_LOG(LogTemp, Log, TEXT("[Spawner] Spawned monster: %s (Active: %d/%d, Cumulative Spawns: %d/%d)"), 
				*Monster->GetName(), ActiveMonsters.Num(), MaxActiveMonsters, SpawnedCount, TotalSpawnLimit);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[Spawner] Acquired actor from pool is not of type ALostArkMonster!"));
			break;
		}
	}
}

void ALostArkMonsterSpawner::OnMonsterKilled(ALostArkMonster* KilledMonster)
{
	if (!IsValid(KilledMonster))
	{
		return;
	}

	ActiveMonsters.Remove(KilledMonster);
	KilledCount++;

	UE_LOG(LogTemp, Log, TEXT("[Spawner] Monster killed: %s (Kills: %d/%d, Active: %d)"), 
		*KilledMonster->GetName(), KilledCount, TotalSpawnLimit, ActiveMonsters.Num());

	ULostArkObjectPoolSubsystem* PoolSubsystem = GetWorld()->GetSubsystem<ULostArkObjectPoolSubsystem>();
	if (PoolSubsystem)
	{
		PoolSubsystem->ReleaseActor(KilledMonster);
	}

	if (KilledCount >= TotalSpawnLimit)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Spawner] Chaos Dungeon Phase Clear! %d Monsters vanquished. Spawning terminated."), TotalSpawnLimit);
		GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
	}
	else
	{
		SpawnMonsterBatch();
	}
}

