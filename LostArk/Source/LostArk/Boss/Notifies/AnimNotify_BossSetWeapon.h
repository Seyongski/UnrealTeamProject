// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Boss/Weapon/BossWeaponComponent.h"	// EBossWeaponState
#include "AnimNotify_BossSetWeapon.generated.h"

/**
 * 몽타주 특정 프레임에 보스 무기 착용 상태를 전환하는 노티파이.
 * (맨손 -> 양손(Hammer2) -> 합체(Hammer1) 전환 타점을 애니메이터가 직접 지정)
 *
 * - 실제 전환은 UBossWeaponComponent::SetWeaponState 가 처리(스폰 없이 숨김 토글).
 * - 서버/클라 모두에서 발동(코스메틱이라 HasAuthority 게이트 없음) -> 각 머신이 즉시 반영.
 */
UCLASS(meta = (DisplayName = "Boss Set Weapon"))
class LOSTARK_API UAnimNotify_BossSetWeapon : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

protected:
	/** 이 노티파이 시점에 전환할 무기 상태 */
	UPROPERTY(EditAnywhere, Category = "Weapon")
	EBossWeaponState WeaponState = EBossWeaponState::Unarmed;
};
