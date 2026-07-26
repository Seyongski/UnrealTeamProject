// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/BossGameplayTags.h"

namespace LostArkTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Boss_Phase_1, "Boss.Phase.1", "1페이즈 (체력 100~75%)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Boss_Phase_2, "Boss.Phase.2", "2페이즈 (체력 75~50%)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Boss_Phase_3, "Boss.Phase.3", "3페이즈 (체력 50~25%)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Boss_Phase_4, "Boss.Phase.4", "4페이즈 (체력 25~5%)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Boss_Phase_5, "Boss.Phase.5", "5페이즈 (체력 5%~ 막공 기믹). 패턴 풀은 4페이즈 것을 폴백 사용 가능");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Boss_Counterable, "State.Boss.Counterable", "카운터 창 부모 (하위 Normal/Multi/Fake, 부모 매치로 종류 무관 감지)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Boss_Countered, "State.Boss.Countered", "(구) 카운터 성공. 분기는 State.Boss.PatternResult.Countered 를 사용할 것");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Boss_Counterable_Normal, "State.Boss.Counterable.Normal", "일반 카운터 창 (1명)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Boss_Counterable_Multi, "State.Boss.Counterable.Multi", "협동 카운터 창 (N명, 인원은 노티파이 인스턴스에서 편집)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Boss_Counterable_Fake, "State.Boss.Counterable.Fake", "가짜 카운터 창 (치면 기믹 실패)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Boss_Groggy, "State.Boss.Groggy", "무력화/그로기 상태");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Boss_TrackTarget, "State.Boss.TrackTarget", "타겟 추적 회전 스위치 (NotifyState가 토글)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Boss_Idle, "State.Boss.Idle", "패턴 사이 딜레이(휴식) 중 — 몽타주 없이 Idle");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Debuff_Shock, "State.Debuff.Shock", "감전 상태이상 (플레이어 대상)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dead, "State.Dead", "사망 (타겟 후보 제외용)");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Player_Grabbed, "State.Player.Grabbed", "보스에게 잡힌 상태 (입력/이동 잠금은 플레이어 쪽에서 바인딩)");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Player_Marked, "State.Player.Marked", "어그로 표식: 기믹이 지정한 1명 (복제 루스, 몸통 마커 위젯 표시 게이트)");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Boss_JustGuardable, "State.Boss.JustGuardable", "저스트가드 창 열림 (보스 노란 글로우 연출을 이 태그에 바인딩)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Player_GuardReady, "State.Player.GuardReady", "저스트가드 1회 가능 상태 (G 입력 소모 시 제거)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Player_Guarding, "State.Player.Guarding", "저스트가드 모션 재생 중 (이동/스킬 잠금은 플레이어 쪽에서 바인딩)");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Charge_Red, "State.Charge.Red", "빨간 전하 (조우 시 랜덤 부여)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Charge_Blue, "State.Charge.Blue", "파란 전하 (조우 시 랜덤 부여)");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Boss_WeakPointExposed, "State.Boss.WeakPointExposed", "약점포착: 어디서 때려도 백/헤드어택 보너스 적용");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Boss_StaggerPhase, "State.Boss.StaggerPhase", "지형파괴 기믹 무력화 페이즈 진행 중 (클라 무력화 게이지 UI 표시용, 복제 루스)");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Boss_PatternResult, "State.Boss.PatternResult", "패턴 결과 태그 부모 (패턴 종료 시 하위 전부 자동 제거)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Boss_PatternResult_Grabbed, "State.Boss.PatternResult.Grabbed", "이번 패턴에서 잡기 성공 (Branch 조건용)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Boss_PatternResult_AoeHit, "State.Boss.PatternResult.AoeHit", "이번 패턴에서 장판 적중 (Branch 조건용)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Boss_PatternResult_JustGuarded, "State.Boss.PatternResult.JustGuarded", "이번 패턴에서 저스트가드 성공 (Branch 조건용)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Boss_PatternResult_JustGuardFailed, "State.Boss.PatternResult.JustGuardFailed", "저스트가드 기믹 실패 확정 (패턴 종료까지 유지, 남은 저스트가드 창 무시 게이트)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Boss_PatternResult_Countered, "State.Boss.PatternResult.Countered", "진짜 카운터 성공 (패턴 종료까지 유지, 그로기 분기용)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Boss_PatternResult_FakeCountered, "State.Boss.PatternResult.FakeCountered", "이 창에서 가짜 카운터를 침 (창 닫힘 시 제거, 즉시 분기용)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Boss_PatternResult_CounterFailed, "State.Boss.PatternResult.CounterFailed", "카운터 기믹 실패 확정 (패턴 종료까지 유지, 남은 카운터 창 무시 게이트)");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Boss_CounterHit, "Event.Boss.CounterHit", "플레이어 카운터 스킬이 보스에 적중 (Instigator=플레이어, 판정은 보스 쪽)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Boss_JustGuardInput, "Event.Boss.JustGuardInput", "플레이어 저스트가드(G) 입력 (Instigator=플레이어, 판정은 보스 쪽)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Boss_StaggerHit, "Event.Boss.StaggerHit", "무력화 게이지 감소 (EventMagnitude=무력화 수치, 판정은 보스 쪽)");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Damage, "Data.Damage", "SetByCaller: 데미지 GE에 실을 공격력계수");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Duration, "Data.Duration", "SetByCaller: 지속시간형 GE(그로기 등)의 지속시간(초)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Stagger, "Data.Stagger", "SetByCaller: 무력화 감소량 (스킬별 무력화 수치)");
}
