// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "NativeGameplayTags.h"

/**
 * 보스 관련 네이티브 GameplayTag.
 * C++에 정의하므로 ini 스캔과 무관하게 항상 등록되고 에디터 피커에도 뜬다.
 * (보스 전용 파일이라 팀 머지 충돌 없음)
 */
namespace LostArkTags
{
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Boss_Phase_1);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Boss_Phase_2);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Boss_Phase_3);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Boss_Phase_4);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Boss_Counterable);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Boss_Countered);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Boss_Groggy);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Boss_TrackTarget);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Debuff_Shock);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dead);
}
