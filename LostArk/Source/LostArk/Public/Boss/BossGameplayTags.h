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

	// 잡기 패턴: 보스에게 잡힌 상태 (플레이어 쪽 입력/이동 잠금은 팀원이 이 태그에 바인딩)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Player_Grabbed);

	// 전하 기믹: 조우 시 랜덤 부여, 변환장판으로 스왑
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Charge_Red);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Charge_Blue);

	// 약점포착: 지형 파괴 후 보스에 부여 -> 어디서 때려도 백/헤드어택 보너스
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Boss_WeakPointExposed);

	// SetByCaller: 데미지 GE에 공격력계수를 실어 보내는 데이터 태그
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Damage);
}
