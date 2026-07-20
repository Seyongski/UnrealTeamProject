#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/LostArkPoolableInterface.h"
#include "LostArkDamageTextActor.generated.h"

class UWidgetComponent;

/**
 * Damage Text Actor to display damage amount in the world
 */
UCLASS()
class LOSTARK_API ALostArkDamageTextActor : public AActor, public ILostArkPoolableInterface
{
	GENERATED_BODY()
	
public:	
	ALostArkDamageTextActor();

	virtual void OnAcquiredFromPool_Implementation() override;
	virtual void OnReleasedToPool_Implementation() override;

	// Setup Damage amount and trigger blueprint animations
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Damage Text")
	void SetupDamageText(float DamageAmount);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UWidgetComponent* DamageTextWidgetComponent;

	// Return to pool when animation ends
	UFUNCTION(BlueprintCallable, Category = "Damage Text")
	void ReturnToPool();
};

