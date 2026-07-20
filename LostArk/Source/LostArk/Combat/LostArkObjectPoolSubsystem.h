#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LostArkObjectPoolSubsystem.generated.h"

USTRUCT(BlueprintType)
struct FPooledActorArray
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<AActor*> InactiveActors;
};

UCLASS()
class LOSTARK_API ULostArkObjectPoolSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Object Pool")
	AActor* AcquireActor(TSubclassOf<AActor> ActorClass, const FVector& Location, const FRotator& Rotation);

	UFUNCTION(BlueprintCallable, Category = "Object Pool")
	void ReleaseActor(AActor* ActorToRelease);

private:
	UPROPERTY()
	TMap<TSubclassOf<AActor>, FPooledActorArray> PoolMap;
};


