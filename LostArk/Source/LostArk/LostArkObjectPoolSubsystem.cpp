#include "LostArkObjectPoolSubsystem.h"
#include "LostArkPoolableInterface.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

void ULostArkObjectPoolSubsystem::Deinitialize()
{
	for (auto& Pair : PoolMap)
	{
		for (AActor* Actor : Pair.Value.InactiveActors)
		{
			if (IsValid(Actor))
			{
				Actor->Destroy();
			}
		}
	}
	PoolMap.Empty();

	Super::Deinitialize();
}

AActor* ULostArkObjectPoolSubsystem::AcquireActor(TSubclassOf<AActor> ActorClass, const FVector& Location, const FRotator& Rotation)
{
	if (!ActorClass)
	{
		return nullptr;
	}

	AActor* AcquiredActor = nullptr;

	if (PoolMap.Contains(ActorClass) && PoolMap[ActorClass].InactiveActors.Num() > 0)
	{
		AcquiredActor = PoolMap[ActorClass].InactiveActors.Pop();
		
		if (IsValid(AcquiredActor))
		{
			AcquiredActor->SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
			
			AcquiredActor->SetActorHiddenInGame(false);
			AcquiredActor->SetActorEnableCollision(true);
			AcquiredActor->SetActorTickEnabled(true);

			if (AcquiredActor->GetClass()->ImplementsInterface(ULostArkPoolableInterface::StaticClass()))
			{
				ILostArkPoolableInterface::Execute_OnAcquiredFromPool(AcquiredActor);
			}
			
			UE_LOG(LogTemp, Log, TEXT("[ObjectPool] Acquired pooled actor of class: %s"), *ActorClass->GetName());
		}
	}

	if (!AcquiredActor)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		
		AcquiredActor = GetWorld()->SpawnActor<AActor>(ActorClass, Location, Rotation, SpawnParams);
		
		if (IsValid(AcquiredActor))
		{
			if (AcquiredActor->GetClass()->ImplementsInterface(ULostArkPoolableInterface::StaticClass()))
			{
				ILostArkPoolableInterface::Execute_OnAcquiredFromPool(AcquiredActor);
			}
			
			UE_LOG(LogTemp, Log, TEXT("[ObjectPool] Spawned NEW actor of class: %s"), *ActorClass->GetName());
		}
	}

	return AcquiredActor;
}

void ULostArkObjectPoolSubsystem::ReleaseActor(AActor* ActorToRelease)
{
	if (!IsValid(ActorToRelease))
	{
		return;
	}

	TSubclassOf<AActor> ActorClass = ActorToRelease->GetClass();

	ActorToRelease->SetActorHiddenInGame(true);
	ActorToRelease->SetActorEnableCollision(false);
	ActorToRelease->SetActorTickEnabled(false);

	if (ActorClass->ImplementsInterface(ULostArkPoolableInterface::StaticClass()))
	{
		ILostArkPoolableInterface::Execute_OnReleasedToPool(ActorToRelease);
	}

	PoolMap.FindOrAdd(ActorClass).InactiveActors.AddUnique(ActorToRelease);
	
	UE_LOG(LogTemp, Log, TEXT("[ObjectPool] Released actor to pool: %s (Total Inactive: %d)"), 
		*ActorClass->GetName(), PoolMap[ActorClass].InactiveActors.Num());
}

