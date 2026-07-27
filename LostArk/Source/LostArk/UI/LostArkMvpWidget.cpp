#include "UI/LostArkMvpWidget.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/World.h"

TArray<FPlayerDamageInfo> ULostArkMvpWidget::GetSortedMvpList() const
{
	TArray<FPlayerDamageInfo> SortedList;
	if (UWorld* World = GetWorld())
	{
		if (ABossRaidGameState* GS = World->GetGameState<ABossRaidGameState>())
		{
			SortedList = GS->PlayerDamageList;
			
			// 딜량 기준 내림차순 정렬
			SortedList.Sort([](const FPlayerDamageInfo& A, const FPlayerDamageInfo& B) {
				return A.DamageDealt > B.DamageDealt;
			});
		}
	}
	return SortedList;
}

void ULostArkMvpWidget::OnExitButtonClicked()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		UKismetSystemLibrary::QuitGame(this, PC, EQuitPreference::Quit, false);
	}
}
