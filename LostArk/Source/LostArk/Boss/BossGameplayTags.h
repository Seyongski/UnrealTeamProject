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

	// 카운터 창: AnimNotifyState_BossCounter 가 구간 동안 부여.
	// 부모(State.Boss.Counterable)로 매치하면 종류 무관 "카운터 창 열림"만 감지되므로
	// 플레이어/UI 쪽은 가짜 카운터를 구분할 수 없다(의도된 설계)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Boss_Counterable_Normal);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Boss_Counterable_Multi);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Boss_Counterable_Fake);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Debuff_Shock);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dead);

	// 잡기 패턴: 보스에게 잡힌 상태 (플레이어 쪽 입력/이동 잠금은 팀원이 이 태그에 바인딩)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Player_Grabbed);

	// 저스트가드 패턴 (설계: docs/09_JUSTGUARD_PATTERN.md)
	// 창 열림: AnimNotifyState_BossJustGuard 가 구간 동안 보스에 부여 (카운터의 Counterable 대응).
	//          보스 '노란 글로우' 연출을 이 태그(또는 컴포넌트 델리게이트)에 BP에서 바인딩한다.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Boss_JustGuardable);
	// 가드 가능(1회): 창 열림 동안 플레이어에 부여. G 입력을 소모하면 제거 -> '패턴당 1회' 게이트.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Player_GuardReady);
	// 가드 모션 재생 중: 이동/스킬 잠금은 플레이어 쪽이 이 태그에 바인딩
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Player_Guarding);

	// 전하 기믹: 조우 시 랜덤 부여, 변환장판으로 스왑
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Charge_Red);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Charge_Blue);

	// 약점포착: 지형 파괴 후 보스에 부여 -> 어디서 때려도 백/헤드어택 보너스
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Boss_WeakPointExposed);

	// 패턴 결과: 패턴 진행 중 보스에 부여되는 일시 태그. 스텝 Branch(TagQuery)가 평가하고,
	// 패턴 어빌리티가 종료될 때 하위 태그 전부 자동 제거 (다음 패턴으로 안 샘)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Boss_PatternResult);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Boss_PatternResult_Grabbed);	// 잡기 성공 (1명 이상)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Boss_PatternResult_AoeHit);	// 장판 적중 (1명 이상)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Boss_PatternResult_JustGuarded);	// 저스트가드 성공 (1명 이상, Branch 조건용)

	// 카운터 결과 (수명이 서로 다른 3종 — BossCounterComponent 가 발행)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Boss_PatternResult_Countered);		// 진짜 카운터 성공. 패턴 종료까지 유지 -> 그로기 분기용
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Boss_PatternResult_FakeCountered);	// '이 창에서' 가짜를 침. 창이 닫히면 제거 -> 즉시 분기용
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Boss_PatternResult_CounterFailed);	// 기믹 실패 확정. 패턴 종료까지 유지 -> 남은 카운터 창 전부 무시

	// 플레이어 카운터 스킬 적중 이벤트: 스킬 쪽은 보스 상태를 몰라도 되고,
	// 적중 시 SendGameplayEventToActor(보스, 이 태그, Instigator=플레이어) 만 보내면 된다
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Boss_CounterHit);

	// 플레이어 저스트가드 입력 이벤트: 가드(G) 발동 시 SendGameplayEventToActor(보스, 이 태그, Instigator=플레이어).
	// 가드 시점의 방향은 보스가 Instigator 의 전방 벡터로 읽으므로 페이로드에 방향을 실을 필요 없다.
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Boss_JustGuardInput);

	// SetByCaller: 데미지 GE에 공격력계수를 실어 보내는 데이터 태그
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Damage);

	// SetByCaller: 지속시간형 GE(그로기 등)에 지속시간(초)을 실어 보내는 데이터 태그
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Duration);
}
