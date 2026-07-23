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

	UPROPERTY()
	UUserWidget* ActiveLoadingWidget;
};
