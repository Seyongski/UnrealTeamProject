#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LostArkUserWidget.generated.h"

class UAbilitySystemComponent;

/**
 * 프로젝트 베이스 위젯 클래스
 */
UCLASS()
class LOSTARK_API ULostArkUserWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 소유자의 Ability System Component 반환
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "UI|GAS")
	UAbilitySystemComponent* GetAbilitySystemComponent() const;
};
