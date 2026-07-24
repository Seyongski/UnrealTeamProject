#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Boss/Raid/BossRaidGameState.h"
#include "LostArkMvpWidget.generated.h"

/**
 * MVP 화면 UI 베이스 클래스
 */
UCLASS()
class LOSTARK_API ULostArkMvpWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 현재 저장된 데미지 리스트를 딜량 내림차순으로 정렬하여 반환합니다. */
	UFUNCTION(BlueprintCallable, Category = "MVP")
	TArray<FPlayerDamageInfo> GetSortedMvpList() const;

	/** 우측 하단 나가기 버튼 클릭 시 호출 (게임 종료) */
	UFUNCTION(BlueprintCallable, Category = "MVP")
	void OnExitButtonClicked();
};
