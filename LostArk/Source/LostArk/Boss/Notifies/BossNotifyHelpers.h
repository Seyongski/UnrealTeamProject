// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/SkeletalMeshComponent.h"

/**
 * 보스 애님 노티파이 공용 헬퍼.
 * 노티파이는 몽타주가 재생되는 모든 머신에서 발동되지만, 판정/창 토글/태그류는
 * 서버 권위에서만 실행해야 한다. 그 "서버 게이트" 관용구를 한 곳에 모았다.
 * (코스메틱 노티파이(무기 표시 등)는 이 게이트를 쓰지 않고 모든 머신에서 실행한다)
 */
namespace BossNotify
{
	/** 서버 권한일 때만 소유 액터(보스) 반환. 클라이언트/소유자 없음이면 nullptr */
	inline AActor* GetServerOwner(const USkeletalMeshComponent* MeshComp)
	{
		AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
		return (Owner && Owner->HasAuthority()) ? Owner : nullptr;
	}

	/** 서버 권한일 때만 보스의 컴포넌트 반환 (창 토글 등 서버 전용 경로) */
	template <typename TComp>
	TComp* GetServerComponent(const USkeletalMeshComponent* MeshComp)
	{
		AActor* Owner = GetServerOwner(MeshComp);
		return Owner ? Owner->FindComponentByClass<TComp>() : nullptr;
	}

	/** 서버 권한일 때만 보스 ASC 반환 (태그 토글은 서버 전용) */
	inline UAbilitySystemComponent* GetServerASC(const USkeletalMeshComponent* MeshComp)
	{
		AActor* Owner = GetServerOwner(MeshComp);
		return Owner ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner) : nullptr;
	}
}
