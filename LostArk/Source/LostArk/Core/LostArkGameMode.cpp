// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/LostArkGameMode.h"
#include "Core/LostArkPlayerController.h"
#include "Character/LostArkCharacter.h"
#include "UObject/ConstructorHelpers.h"
#include "Monster/LostArkMonsterSpawner.h"
#include "Monster/LostArkStagePortalArea.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

ALostArkGameMode::ALostArkGameMode()
{
	bUseSeamlessTravel = false; // PIE 에디터 환경에서는 SeamlessTravel이 일시정지(Hang) 버그를 유발하므로 우선 false로 설정합니다.

	PlayerControllerClass = ALostArkPlayerController::StaticClass();

	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/TopDown/Blueprints/BP_TopDownCharacter"));
	if (PlayerPawnBPClass.Class != nullptr)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}

	static ConstructorHelpers::FClassFinder<APlayerController> PlayerControllerBPClass(TEXT("/Game/TopDown/Blueprints/BP_TopDownPlayerController"));
	if (PlayerControllerBPClass.Class != NULL)
	{
		PlayerControllerClass = PlayerControllerBPClass.Class;
	}

	StagePortalAreaClass = ALostArkStagePortalArea::StaticClass();
}

void ALostArkGameMode::BeginPlay()
{
	Super::BeginPlay();

	TArray<AActor*> Spawners;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ALostArkMonsterSpawner::StaticClass(), Spawners);
	for (AActor* SpawnerActor : Spawners)
	{
		if (ALostArkMonsterSpawner* Spawner = Cast<ALostArkMonsterSpawner>(SpawnerActor))
		{
			Spawner->OnAllMonstersKilled.AddDynamic(this, &ALostArkGameMode::OnAllMonstersKilled);
		}
	}
}

void ALostArkGameMode::OnAllMonstersKilled(ALostArkMonsterSpawner* Spawner)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (ALostArkPlayerController* PC = Cast<ALostArkPlayerController>(It->Get()))
		{
			PC->ClientShowStageClearUI();
		}
	}

	TArray<AActor*> Portals;
	UGameplayStatics::GetAllActorsOfClass(World, ALostArkStagePortalArea::StaticClass(), Portals);

	ALostArkStagePortalArea* TargetPortal = nullptr;
	for (AActor* PortalActor : Portals)
	{
		if (ALostArkStagePortalArea* Portal = Cast<ALostArkStagePortalArea>(PortalActor))
		{
			TargetPortal = Portal;
			break;
		}
	}

	if (TargetPortal)
	{
		TargetPortal->ActivatePortal();
	}
	else if (StagePortalAreaClass && !SpawnedPortalArea)
	{
		FVector SpawnLocation = FVector::ZeroVector;
		FRotator SpawnRotation = FRotator::ZeroRotator;
		if (IsValid(Spawner))
		{
			SpawnLocation = Spawner->GetActorLocation();
			SpawnRotation = Spawner->GetActorRotation();
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnedPortalArea = World->SpawnActor<ALostArkStagePortalArea>(StagePortalAreaClass, SpawnLocation, SpawnRotation, SpawnParams);
		if (SpawnedPortalArea)
		{
			SpawnedPortalArea->ActivatePortal();
		}
	}
}

void ALostArkGameMode::OnStagePortalAreaReady()
{
	UWorld* World = GetWorld();
	if (!World || NextLevelURL.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[GameMode] Cannot execute ServerTravel: World is invalid or NextLevelURL is empty!"));
		return;
	}

	// Tell all clients to show the loading screen
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (ALostArkPlayerController* PC = Cast<ALostArkPlayerController>(It->Get()))
		{
			PC->ClientShowLoadingScreen();
		}
	}

	// Give the UI 0.5 seconds to render before the main thread blocks for ServerTravel
	FTimerHandle TravelTimerHandle;
	GetWorldTimerManager().SetTimer(TravelTimerHandle, this, &ALostArkGameMode::ExecuteServerTravel, 0.5f, false);
}

void ALostArkGameMode::ExecuteServerTravel()
{
	UWorld* World = GetWorld();
	if (!World) return;

	FString TravelURL = NextLevelURL;
	if (!TravelURL.Contains(TEXT("?listen")))
	{
		TravelURL += TEXT("?listen");
	}

	UE_LOG(LogTemp, Log, TEXT("[GameMode] All players in Stage Portal Area! Executing ServerTravel to: %s"), *TravelURL);
	World->ServerTravel(TravelURL, false);
}
