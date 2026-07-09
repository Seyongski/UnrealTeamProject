// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "LostArkPoolableInterface.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class ULostArkPoolableInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 오브젝트 풀에 의해 생명주기가 관리되는 객체들을 위한 인터페이스
 */
class LOSTARK_API ILostArkPoolableInterface
{
	GENERATED_BODY()

public:
	/** 풀에서 획득(활성화)될 때 호출되는 함수 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Object Pool")
	void OnAcquiredFromPool();

	/** 풀에 반환(비활성화)될 때 호출되는 함수 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Object Pool")
	void OnReleasedToPool();
};

