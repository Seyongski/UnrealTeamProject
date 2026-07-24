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

	UFUNCTION(BlueprintCallable, Category = "UI")
	void InitializeHUDForCharacter(class ALostArkCharacter* InCharacter);

	UFUNCTION(Client, Reliable)
	void ClientShowStageClearUI();

	UFUNCTION(Client, Reliable)
	void ClientShowLoadingScreen();

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	TSubclassOf<class ULostArkHUDWidget> HUDWidgetClass;

	UPROPERTY(BlueprintReadOnly, Category = "UI")
	class ULostArkHUDWidget* HUDWidget;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	TSubclassOf<class UUserWidget> StageClearWidgetClass;

	UPROPERTY(BlueprintReadOnly, Category = "UI")
	class UUserWidget* StageClearWidget;

	/** 화면 상단 보스 체력 HUD (WBP_BossHUD — 이 컨트롤러 BP 에서 지정). 보스 레벨에서만 생성된다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	TSubclassOf<class UBossHUDWidget> BossHUDWidgetClass;

	UPROPERTY(BlueprintReadOnly, Category = "UI")
	class UBossHUDWidget* BossHUDWidget;

	/**
	 * 보스 레벨일 때만 보스 HUD 를 생성/배치한다 (로컬 컨트롤러 전용).
	 * 판별: 월드 GameState 가 ABossRaidGameState 인지. GameState/보스가 아직 복제 전이면
	 * 짧게 재시도한다(튜토/카오스던전에선 GameState 가 달라 곧 중단 -> 안 뜸).
	 */
	void TryCreateBossHUD();

	void OnInputStarted();
	void OnSetDestinationTriggered();
	void OnSetDestinationReleased();
	void OnTouchTriggered();
	void OnTouchReleased();

	/** G키 입력 시 저스트가드 시도 (State.Player.GuardReady 보유 시 발동) */
	void TryJustGuardInput();

private:
	FVector CachedDestination;
	bool bIsTouch;
	float FollowTime;

	/** 보스 HUD 생성 재시도 타이머/횟수 (GameState/보스 복제 대기용) */
	FTimerHandle BossHUDRetryTimer;
	int32 BossHUDRetryCount = 0;
};




