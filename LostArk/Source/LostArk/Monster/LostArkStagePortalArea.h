#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LostArkStagePortalArea.generated.h"

class UBoxComponent;

UENUM(BlueprintType)
enum class EStagePortalType : uint8
{
	Immediate UMETA(DisplayName = "Immediate (Tutorial)"),
	RequireClear UMETA(DisplayName = "Require Clear (Chaos Dungeon)")
};

UCLASS()
class LOSTARK_API ALostArkStagePortalArea : public AActor
{
	GENERATED_BODY()
	
public:	
	ALostArkStagePortalArea();

	UFUNCTION(BlueprintCallable, Category = "Portal")
	void ActivatePortal();

	UFUNCTION(BlueprintCallable, Category = "Portal")
	bool IsPortalActive() const { return bIsPortalActive; }

protected:
	virtual void BeginPlay() override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UFUNCTION(BlueprintImplementableEvent, Category = "Portal")
	void OnPlayerCountChanged(int32 CurrentCount, int32 RequiredCount, bool bActive);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastUpdatePlayerCount(int32 CurrentCount, int32 RequiredCount);

	UFUNCTION(BlueprintImplementableEvent, Category = "Portal")
	void OnPortalActivated();

	UFUNCTION()
	void OnRep_IsPortalActive();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal Settings")
	EStagePortalType PortalType;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal State", ReplicatedUsing = OnRep_IsPortalActive)
	bool bIsPortalActive;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal State", ReplicatedUsing = OnRep_PlayerCount)
	int32 CurrentPlayerCount;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal State", ReplicatedUsing = OnRep_PlayerCount)
	int32 RequiredPlayerCount;

	UFUNCTION()
	void OnRep_PlayerCount();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal Component")
	UBoxComponent* OverlapComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal Component")
	UStaticMeshComponent* PortalMeshComponent;

private:
	void CheckAllPlayersReady();

	UPROPERTY(VisibleAnywhere, Category = "Portal State")
	TArray<ACharacter*> OverlappingPlayers;

	bool bPortalTriggered;
};
