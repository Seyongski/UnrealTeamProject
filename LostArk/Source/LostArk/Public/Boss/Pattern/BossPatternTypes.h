// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BossPatternTypes.generated.h"

/** 패턴 1개의 실행 결과. 분기 시퀀스가 이 값으로 다음 노드를 고른다. */
UENUM(BlueprintType)
enum class EPatternResult : uint8
{
	Success,	// 패턴이 의도대로 끝남 -> NextOnSuccess
	Fail		// 카운터/무력화 당하는 등 실패 -> NextOnFail
};
