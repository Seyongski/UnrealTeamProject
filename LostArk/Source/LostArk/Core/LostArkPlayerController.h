#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "GameFramework/PlayerController.h"
#include "LostArkPlayerController.generated.h"

class UNiagaraSystem;
class UInputMappingContext;
class UInputAction;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

UCLASS()
class ALostArkPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ALostArkPlayerController();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
	float ShortPressThreshold;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
	TSoftObjectPtr<UNiagaraSystem> FXCursor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputMappingContext* DefaultMappingContext;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* SetDestinationClickAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputAction* SetDestinationTouchAction;

protected:
	uint32 bMoveToMouseCursor : 1;

	virtual void SetupInputComponent() override;
	
	virtual void BeginPlay() override;

	void OnInputStarted();
	void OnSetDestinationTriggered();
	void OnSetDestinationReleased();
	void OnTouchTriggered();
	void OnTouchReleased();

	// ===== [임시 디버그] Q -> 보스 카운터 강제 (헤드존 무시). 카운터/그로기 흐름 확인용. 나중에 전부 삭제 =====
	void DebugForceCounterHit();
	UFUNCTION(Server, Reliable)
	void ServerDebugForceCounterHit();
	// ============================================================================================

private:
	FVector CachedDestination;
	bool bIsTouch;
	float FollowTime;
};




