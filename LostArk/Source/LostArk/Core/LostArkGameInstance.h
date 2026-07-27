#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "LostArkGameInstance.generated.h"

class UUserWidget;

/**
 * Custom GameInstance for handling Map Loading Screens and global state.
 */
UCLASS()
class LOSTARK_API ULostArkGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void Shutdown() override;

protected:
	/** Loading screen widget blueprint class to use */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loading Screen")
	TSubclassOf<UUserWidget> LoadingScreenWidgetClass;

public:
	UFUNCTION(BlueprintCallable, Category = "Loading Screen")
	void ShowLoadingScreen();

	UFUNCTION()
	virtual void BeginLoadingScreen(const FString& MapName);

	UFUNCTION()
	virtual void EndLoadingScreen(UWorld* InLoadedWorld);

	/** 로딩/대기 화면 내리기. 맵 로드 완료와 '전원 로드 대기' 해제 양쪽에서 쓴다 */
	UFUNCTION(BlueprintCallable, Category = "Loading Screen")
	void HideLoadingScreen();

	UPROPERTY()
	UUserWidget* ActiveLoadingWidget;

	/**
	 * 레벨 이동 직전 서버가 저장해 두는 파티 인원수.
	 * 비-seamless ServerTravel 이라 클라는 각자 재접속하고, 다음 레벨의 GameMode 는 원래 몇 명이었는지
	 * 알 방법이 없다. GameInstance 는 트래블을 넘어 살아남으므로 여기에 실어 보낸다.
	 */
	void SetPendingPartySize(int32 InPartySize) { PendingPartySize = FMath::Max(InPartySize, 0); }

	/** 저장된 파티 인원수를 꺼내면서 즉시 비운다 (그 다음 레벨로 값이 새는 것 방지) */
	int32 ConsumePendingPartySize()
	{
		const int32 PartySize = PendingPartySize;
		PendingPartySize = 0;
		return PartySize;
	}

private:
	/** @see SetPendingPartySize. 0 이면 '이전 레벨에서 넘어온 정보 없음' */
	int32 PendingPartySize = 0;
};
