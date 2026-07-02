// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BossPatternTypes.h"
#include "BossPhaseDataAsset.generated.h"

class UPatternDataAsset;

/**
 * 분기형 패턴 그래프의 노드.
 * 하나의 패턴을 감싸고, 성공/실패 시 이동할 노드를 지정한다.
 * (선형 1234가 아니라, 1 성공->3 / 1 실패->2 같은 그래프 구성)
 */
USTRUCT(BlueprintType)
struct FBossPatternNode
{
	GENERATED_BODY()

	/** 이 노드의 식별자 (분기 연결에 사용) */
	UPROPERTY(EditDefaultsOnly, Category = "Node")
	FName NodeId;

	/** 이 노드가 실행할 패턴 */
	UPROPERTY(EditDefaultsOnly, Category = "Node")
	TObjectPtr<UPatternDataAsset> Pattern = nullptr;

	/** 성공 시 이동할 노드 (비우면 진입 노드로 루프) */
	UPROPERTY(EditDefaultsOnly, Category = "Node")
	FName NextOnSuccess;

	/** 실패 시 이동할 노드 (비우면 진입 노드로 루프) */
	UPROPERTY(EditDefaultsOnly, Category = "Node")
	FName NextOnFail;
};

/**
 * 한 페이즈의 패턴셋 + 분기 흐름 + 진입 조건.
 * 맵은 그대로, 체력에 따라 이 애셋만 교체된다.
 */
UCLASS(BlueprintType)
class LOSTARK_API UBossPhaseDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 현재 체력(%)이 이 값 이하가 되면 이 페이즈로 진입. Phases 배열은 이 값 내림차순으로 정렬(100,75,50,25) */
	UPROPERTY(EditDefaultsOnly, Category = "Phase", meta = (ClampMin = "0", ClampMax = "100"))
	float EnterHealthPercent = 100.f;

	/** 이 페이즈 진입 시 1회 실행할 전환 기믹 패턴 (비우면 스킵하고 바로 EntryNode) */
	UPROPERTY(EditDefaultsOnly, Category = "Phase")
	TObjectPtr<UPatternDataAsset> TransitionGimmick = nullptr;

	/** 시작 노드 */
	UPROPERTY(EditDefaultsOnly, Category = "Phase")
	FName EntryNodeId;

	/** 분기 그래프 노드들 */
	UPROPERTY(EditDefaultsOnly, Category = "Phase")
	TArray<FBossPatternNode> Nodes;

	/** NodeId 로 노드 찾기 (없으면 nullptr) */
	const FBossPatternNode* FindNode(FName NodeId) const;
};
