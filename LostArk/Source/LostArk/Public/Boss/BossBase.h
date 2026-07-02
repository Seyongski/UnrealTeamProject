// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BossBase.generated.h"

class UBackHeadDecalComponent;

UCLASS()
class LOSTARK_API ABossBase : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ABossBase();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/** 캡슐 반경에 맞춰 백헤드 데칼 크기/위치 갱신. 캡슐 크기가 바뀔 때(버프 등) 호출 */
	UFUNCTION(BlueprintCallable, Category = "Boss|BackHead")
	void UpdateBackHeadDecal();

protected:
	/** 백/헤드 어택 방향을 표시하는 지면 데칼 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|BackHead")
	TObjectPtr<UBackHeadDecalComponent> BackHeadDecal;

};
