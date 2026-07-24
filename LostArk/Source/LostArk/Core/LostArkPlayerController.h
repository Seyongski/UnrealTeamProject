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

	// ===== [임시 디버그] Q -> 보스 카운터 강제 (헤드존 무시). 카운터/그로기 흐름 확인용. 나중에 전부 삭제 =====
	void DebugForceCounterHit();
	UFUNCTION(Server, Reliable)
	void ServerDebugForceCounterHit();
	// ============================================================================================

	// ===== [임시 디버그] G -> 저스트가드 모션. 창이 없으면 '비활성화', 있으면 '발동'(+성공/실패는 장판이 표시). 나중에 삭제 =====
	void DebugTryJustGuard();
	UFUNCTION(Server, Reliable)
	void ServerDebugTryJustGuard();
	// ============================================================================================

	// ===== [임시 디버그] E -> 보스 무력화 게이지 10 감소 (페이즈 중에만 유효). 나중에 스킬로 대체/삭제 =====
	void DebugStaggerHit();
	UFUNCTION(Server, Reliable)
	void ServerDebugStaggerHit();
	// ============================================================================================

private:
	FVector CachedDestination;
	bool bIsTouch;
	float FollowTime;

	/** 보스 HUD 생성 재시도 타이머/횟수 (GameState/보스 복제 대기용) */
	FTimerHandle BossHUDRetryTimer;
	int32 BossHUDRetryCount = 0;
};




