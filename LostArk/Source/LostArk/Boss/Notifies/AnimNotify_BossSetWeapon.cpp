// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Notifies/AnimNotify_BossSetWeapon.h"
#include "Boss/Weapon/BossWeaponComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

void UAnimNotify_BossSetWeapon::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	AActor* Boss = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (!Boss)
	{
		return;
	}

	// 코스메틱 전환이라 서버/클라 모두에서 로컬 반영. (복제는 컴포넌트의 WeaponState 가 담당)
	if (UBossWeaponComponent* Weapon = Boss->FindComponentByClass<UBossWeaponComponent>())
	{
		Weapon->SetWeaponState(WeaponState);
	}
}

FString UAnimNotify_BossSetWeapon::GetNotifyName_Implementation() const
{
	const UEnum* EnumPtr = StaticEnum<EBossWeaponState>();
	const FString StateName = EnumPtr ? EnumPtr->GetDisplayNameTextByValue((int64)WeaponState).ToString()
									  : TEXT("?");
	return FString::Printf(TEXT("SetWeapon: %s"), *StateName);
}
