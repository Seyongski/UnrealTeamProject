#include "Monster/LostArkStagePortalArea.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Core/LostArkGameMode.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

ALostArkStagePortalArea::ALostArkStagePortalArea()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);

	OverlapComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("OverlapComponent"));
	SetRootComponent(OverlapComponent);
	OverlapComponent->SetBoxExtent(FVector(200.f, 200.f, 100.f));
	OverlapComponent->SetCollisionProfileName(TEXT("Trigger"));

	PortalMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PortalMeshComponent"));
	PortalMeshComponent->SetupAttachment(RootComponent);
	PortalMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	PortalType = EStagePortalType::RequireClear;
	bIsPortalActive = false;
	bPortalTriggered = false;
	CurrentPlayerCount = -1;
	RequiredPlayerCount = -1;
}

void ALostArkStagePortalArea::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ALostArkStagePortalArea, bIsPortalActive);
	DOREPLIFETIME(ALostArkStagePortalArea, CurrentPlayerCount);
	DOREPLIFETIME(ALostArkStagePortalArea, RequiredPlayerCount);
}

void ALostArkStagePortalArea::OnRep_PlayerCount()
{
	OnPlayerCountChanged(CurrentPlayerCount, RequiredPlayerCount, bIsPortalActive);
}

void ALostArkStagePortalArea::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		OverlapComponent->OnComponentBeginOverlap.AddDynamic(this, &ALostArkStagePortalArea::OnOverlapBegin);
		OverlapComponent->OnComponentEndOverlap.AddDynamic(this, &ALostArkStagePortalArea::OnOverlapEnd);

		if (PortalType == EStagePortalType::Immediate)
		{
			ActivatePortal();
		}

		// 멀티플레이어 PIE 및 클라이언트 접속 지연(로딩 시점 차이)에 대응하여
		// 0.2초, 0.5초, 1.0초, 2.0초 다단계 시점에 세션 인원수를 자동 재계산 및 복제 동기화합니다.
		FTimerHandle T1, T2, T3, T4;
		GetWorldTimerManager().SetTimer(T1, this, &ALostArkStagePortalArea::CheckAllPlayersReady, 0.2f, false);
		GetWorldTimerManager().SetTimer(T2, this, &ALostArkStagePortalArea::CheckAllPlayersReady, 0.5f, false);
		GetWorldTimerManager().SetTimer(T3, this, &ALostArkStagePortalArea::CheckAllPlayersReady, 1.0f, false);
		GetWorldTimerManager().SetTimer(T4, this, &ALostArkStagePortalArea::CheckAllPlayersReady, 2.0f, false);
	}
}

void ALostArkStagePortalArea::ActivatePortal()
{
	if (!HasAuthority() || bIsPortalActive)
	{
		return;
	}

	bIsPortalActive = true;
	OnRep_IsPortalActive();
	CheckAllPlayersReady();

	UE_LOG(LogTemp, Warning, TEXT("[StagePortal] Portal Activated!"));
}

void ALostArkStagePortalArea::OnRep_IsPortalActive()
{
	OnPlayerCountChanged(CurrentPlayerCount, RequiredPlayerCount, bIsPortalActive);
	if (bIsPortalActive)
	{
		OnPortalActivated();
	}
}

void ALostArkStagePortalArea::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority() || !bIsPortalActive || bPortalTriggered)
	{
		return;
	}

	ACharacter* PlayerCharacter = Cast<ACharacter>(OtherActor);
	if (IsValid(PlayerCharacter) && PlayerCharacter->IsPlayerControlled())
	{
		OverlappingPlayers.AddUnique(PlayerCharacter);
		CheckAllPlayersReady();
	}
}

void ALostArkStagePortalArea::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!HasAuthority() || !bIsPortalActive || bPortalTriggered)
	{
		return;
	}

	ACharacter* PlayerCharacter = Cast<ACharacter>(OtherActor);
	if (IsValid(PlayerCharacter))
	{
		OverlappingPlayers.Remove(PlayerCharacter);
		CheckAllPlayersReady();
	}
}

void ALostArkStagePortalArea::MulticastUpdatePlayerCount_Implementation(int32 CurrentCount, int32 RequiredCount)
{
	CurrentPlayerCount = CurrentCount;
	RequiredPlayerCount = RequiredCount;
	OnPlayerCountChanged(CurrentCount, RequiredCount, bIsPortalActive);
}

void ALostArkStagePortalArea::CheckAllPlayersReady()
{
	if (!HasAuthority() || bPortalTriggered)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	int32 RequiredPlayers = 0;

	// GameState의 PlayerArray 수(세션에 접속된 전체 플레이어 수) 우선 카운트
	if (AGameStateBase* GS = World->GetGameState())
	{
		RequiredPlayers = GS->PlayerArray.Num();
	}

	// 폴백: GameMode의 GetNumPlayers() 또는 PlayerControllerIterator
	if (RequiredPlayers <= 0)
	{
		if (AGameModeBase* GM = World->GetAuthGameMode())
		{
			RequiredPlayers = GM->GetNumPlayers();
		}
	}

	if (RequiredPlayers <= 0)
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			if (IsValid(PC))
			{
				RequiredPlayers++;
			}
		}
	}

	int32 CurrentCount = OverlappingPlayers.Num();
	CurrentPlayerCount = CurrentCount;
	RequiredPlayerCount = RequiredPlayers;

	MulticastUpdatePlayerCount(CurrentCount, RequiredPlayers);

	if (bIsPortalActive && RequiredPlayers > 0 && CurrentCount >= RequiredPlayers)
	{
		bPortalTriggered = true;

		ALostArkGameMode* GameMode = World->GetAuthGameMode<ALostArkGameMode>();
		if (GameMode)
		{
			GameMode->OnStagePortalAreaReady();
		}
	}
}
