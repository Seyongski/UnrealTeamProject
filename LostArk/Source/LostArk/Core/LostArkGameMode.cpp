// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/LostArkGameMode.h"
#include "Core/LostArkGameInstance.h"
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

	// 다음 레벨의 '전원 로드 대기' 기준이 될 파티 인원수를 GameInstance 에 실어 보낸다.
	// (비-seamless 트래블이라 클라는 각자 재접속 -> 새 GameMode 는 원래 몇 명이었는지 알 수 없다)
	if (ULostArkGameInstance* LostArkGI = GetGameInstance<ULostArkGameInstance>())
	{
		LostArkGI->SetPendingPartySize(GetNumPlayers());
		UE_LOG(LogTemp, Log, TEXT("[GameMode] 다음 레벨로 파티 인원수 인계: %d명"), GetNumPlayers());
	}

	UE_LOG(LogTemp, Log, TEXT("[GameMode] All players in Stage Portal Area! Executing ServerTravel to: %s"), *TravelURL);
	World->ServerTravel(TravelURL, false);
}

void ALostArkGameMode::NotifyPlayerLevelLoaded(APlayerController* PC)
{
	// 기본 규칙: 기다리지 않는다 -> 보고 즉시 대기 해제 (튜토리얼/카오스던전은 기존 동작 그대로)
	if (ALostArkPlayerController* LostArkPC = Cast<ALostArkPlayerController>(PC))
	{
		LostArkPC->ClientSetWaitingForPlayers(false);
	}
}
