// Fill out your copyright notice in the Description page of Project Settings.

#include "Boss/Weapon/BossWeaponComponent.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

UBossWeaponComponent::UBossWeaponComponent()
{
	// 무기 표시는 이벤트(노티파이/OnRep) 기반. 틱 불필요.
	PrimaryComponentTick.bCanEverTick = false;

	// WeaponState(enum) 만 복제. 무기 액터 자체는 각 머신 로컬 스폰(비복제).
	SetIsReplicatedByDefault(true);
}

void UBossWeaponComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UBossWeaponComponent, WeaponState);
}

void UBossWeaponComponent::BeginPlay()
{
	Super::BeginPlay();

	// 서버/클라 각자 로컬로 3개를 한 번만 스폰해 소켓에 부착 (전부 숨김으로 시작).
	// Hammer2 는 양손이라 두 번 스폰, Hammer1 은 한 자루.
	DualRightWeapon = SpawnAndAttachWeapon(DualWeaponClass, DualRightSocket);
	DualLeftWeapon  = SpawnAndAttachWeapon(DualWeaponClass, DualLeftSocket);
	TwoHandedWeapon = SpawnAndAttachWeapon(TwoHandedWeaponClass, TwoHandedSocket);

	// late-join: 접속 시점의 복제된 WeaponState 를 반영.
	// (OnRep 이 스폰보다 먼저 도착해 무기 ref 가 null 이었어도 여기서 다시 맞춰준다.)
	ApplyWeaponState();
}

void UBossWeaponComponent::SetWeaponState(EBossWeaponState NewState)
{
	// 노티파이는 몽타주가 재생되는 모든 머신에서 발동 -> 로컬 즉시 반영.
	WeaponState = NewState;
	ApplyWeaponState();

	// 서버 권위에서만 복제 대상이 됨 (클라의 로컬 대입은 이후 서버본으로 덮여도 같은 값이라 무해).
	// 복제된 WeaponState 는 노티파이를 놓쳤거나 뒤늦게 접속한 클라의 상태를 맞춰준다.
}

void UBossWeaponComponent::OnRep_WeaponState()
{
	ApplyWeaponState();
}

void UBossWeaponComponent::ApplyWeaponState()
{
	const bool bDual = (WeaponState == EBossWeaponState::DualWield);
	const bool bTwo  = (WeaponState == EBossWeaponState::TwoHanded);

	SetWeaponVisible(DualRightWeapon, bDual);
	SetWeaponVisible(DualLeftWeapon, bDual);
	SetWeaponVisible(TwoHandedWeapon, bTwo);
	// Unarmed 는 세 조건 모두 false -> 전부 숨김.
}

AActor* UBossWeaponComponent::SpawnAndAttachWeapon(TSubclassOf<AActor> WeaponClass, FName SocketName)
{
	if (!WeaponClass)
	{
		return nullptr;
	}

	UWorld* World = GetWorld();
	ACharacter* Boss = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* Mesh = Boss ? Boss->GetMesh() : nullptr;
	if (!World || !Mesh)
	{
		return nullptr;
	}

	if (SocketName != NAME_None && !Mesh->DoesSocketExist(SocketName))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[BossWeaponComponent] 소켓 '%s' 이(가) 보스 메시에 없습니다. 무기가 메시 원점에 붙습니다."),
			*SocketName.ToString());
	}

	FActorSpawnParameters Params;
	Params.Owner = Boss;
	Params.Instigator = Cast<APawn>(Boss);
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* Weapon = World->SpawnActor<AActor>(WeaponClass, Mesh->GetSocketTransform(SocketName), Params);
	if (!Weapon)
	{
		return nullptr;
	}

	// 코스메틱 무기는 각 머신 로컬. BP 가 실수로 Replicates=true 여도 중복 생성 방지.
	Weapon->SetReplicates(false);

	Weapon->AttachToComponent(Mesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketName);

	// 데미지는 AOE 가 처리 -> 무기 콜리전은 항상 꺼둔다(1회). 시작은 숨김.
	Weapon->SetActorEnableCollision(false);
	Weapon->SetActorHiddenInGame(true);

	return Weapon;
}

void UBossWeaponComponent::SetWeaponVisible(AActor* Weapon, bool bVisible)
{
	if (!Weapon)
	{
		return;
	}

	Weapon->SetActorHiddenInGame(!bVisible);
}
