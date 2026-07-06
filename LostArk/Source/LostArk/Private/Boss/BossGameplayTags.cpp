// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/BossGameplayTags.h"

namespace LostArkTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Boss_Phase_1, "Boss.Phase.1", "1페이즈 (체력 100~75%)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Boss_Phase_2, "Boss.Phase.2", "2페이즈 (체력 75~50%)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Boss_Phase_3, "Boss.Phase.3", "3페이즈 (체력 50~25%)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Boss_Phase_4, "Boss.Phase.4", "4페이즈 (체력 25~0%)");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Boss_Counterable, "State.Boss.Counterable", "카운터 가능 윈도우");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Boss_Countered, "State.Boss.Countered", "카운터 성공당함 -> 패턴 분기 트리거");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Boss_Groggy, "State.Boss.Groggy", "무력화/그로기 상태");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Boss_TrackTarget, "State.Boss.TrackTarget", "타겟 추적 회전 스위치 (NotifyState가 토글)");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Debuff_Shock, "State.Debuff.Shock", "감전 상태이상 (플레이어 대상)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dead, "State.Dead", "사망 (타겟 후보 제외용)");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Player_Grabbed, "State.Player.Grabbed", "보스에게 잡힌 상태 (입력/이동 잠금은 플레이어 쪽에서 바인딩)");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Charge_Red, "State.Charge.Red", "빨간 전하 (조우 시 랜덤 부여)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Charge_Blue, "State.Charge.Blue", "파란 전하 (조우 시 랜덤 부여)");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Boss_WeakPointExposed, "State.Boss.WeakPointExposed", "약점포착: 어디서 때려도 백/헤드어택 보너스 적용");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Damage, "Data.Damage", "SetByCaller: 데미지 GE에 실을 공격력계수");
}
