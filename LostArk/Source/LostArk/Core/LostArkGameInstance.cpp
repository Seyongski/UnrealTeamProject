#include "LostArkGameInstance.h"
#include "MoviePlayer.h"
#include "Blueprint/UserWidget.h"
#include "UObject/ConstructorHelpers.h"

void ULostArkGameInstance::Init()
{
	Super::Init();

	// Register map load delegates for loading screen
	FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &ULostArkGameInstance::BeginLoadingScreen);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ULostArkGameInstance::EndLoadingScreen);
}

void ULostArkGameInstance::Shutdown()
{
	// Unregister map load delegates
	FCoreUObjectDelegates::PreLoadMap.RemoveAll(this);
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);

	Super::Shutdown();
}

void ULostArkGameInstance::ShowLoadingScreen()
{
	if (!ActiveLoadingWidget && LoadingScreenWidgetClass)
	{
		ActiveLoadingWidget = CreateWidget<UUserWidget>(this, LoadingScreenWidgetClass);
		if (ActiveLoadingWidget)
		{
			ActiveLoadingWidget->AddToViewport(9999);
			UE_LOG(LogTemp, Warning, TEXT("[GameInstance] ShowLoadingScreen manually added widget to viewport"));
		}
	}
}

void ULostArkGameInstance::BeginLoadingScreen(const FString& MapName)
{
	UE_LOG(LogTemp, Warning, TEXT("[GameInstance] BeginLoadingScreen Called for Map: %s"), *MapName);

	if (!IsRunningDedicatedServer())
	{
		FLoadingScreenAttributes LoadingScreen;
		LoadingScreen.bAutoCompleteWhenLoadingCompletes = true;
		LoadingScreen.bWaitForManualStop = false;
		LoadingScreen.bAllowEngineTick = true;
		LoadingScreen.MinimumLoadingScreenDisplayTime = 1.0f;

		// PIE 대응 및 사전 UI 표시
		ShowLoadingScreen();

		if (ActiveLoadingWidget)
		{
			// TakeWidget converts the UUserWidget to a Slate widget for MoviePlayer
			LoadingScreen.WidgetLoadingScreen = ActiveLoadingWidget->TakeWidget();
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[GameInstance] LoadingScreenWidgetClass is NOT set in Blueprint!"));
		}

		GetMoviePlayer()->SetupLoadingScreen(LoadingScreen);
	}
}

void ULostArkGameInstance::EndLoadingScreen(UWorld* InLoadedWorld)
{
	UE_LOG(LogTemp, Warning, TEXT("[GameInstance] EndLoadingScreen Called"));
	
	if (ActiveLoadingWidget)
	{
		ActiveLoadingWidget->RemoveFromParent();
		ActiveLoadingWidget = nullptr;
		UE_LOG(LogTemp, Warning, TEXT("[GameInstance] Removed Loading Widget from Viewport"));
	}
}
