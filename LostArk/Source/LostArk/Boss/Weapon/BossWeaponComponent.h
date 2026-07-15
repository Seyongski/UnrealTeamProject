// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BossWeaponComponent.generated.h"

/**
 * 보스 무기 착용 상태.
 *  - Unarmed  : 맨손 (조우 시 기본)
 *  - DualWield : 양손에 Hammer2 하나씩 (wp_RR + wp_L)
 *  - TwoHanded : 합쳐진 Hammer1 한 자루 (wp_R)
 */
UENUM(BlueprintType)
enum class EBossWeaponState : uint8
{
	Unarmed		UMETA(DisplayName = "맨손"),
	DualWield	UMETA(DisplayName = "양손무기(Hammer2 x2)"),
	TwoHanded	UMETA(DisplayName = "합체무기(Hammer1)")
};

/**
 * 보스 무기 표시 관리 컴포넌트. 설계 의도: 스폰/삭제 반복 대신 "1회 스폰 + 숨김 토글".
 *
 * 왜 스폰/삭제가 아니라 토글인가:
 *  - 데미지는 AOE 장판이 처리하므로 무기는 순수 코스메틱(메시+이펙트). 콜리전 불필요.
 *  - 매 전환마다 SpawnActor/DestroyActor 를 반복하면 스폰 프레임 히칭 + GC 압박이 생긴다.
 *  - 무기 3개(Hammer1 x1, Hammer2 x2)를 BeginPlay 에 한 번만 스폰해 각 소켓에 붙여두고,
 *    전환은 SetActorHiddenInGame 토글만 한다. 상태가 항상 안정적이라 노티파이가 씹혀도 복구가 쉽다.
 *
 * 멀티플레이(레이드) 처리:
 *  - 무기 액터 자체는 복제하지 않고, 서버/클라가 각자 로컬로 1회씩 스폰한다(코스메틱).
 *    => BP_Hammer1/BP_Hammer2 의 "Replicates" 는 반드시 꺼 둘 것 (켜져 있으면 서버본까지
 *       복제돼 중복 생성됨). 방어적으로 스폰 직후 SetReplicates(false) 도 호출한다.
 *  - 복제되는 것은 WeaponState(enum) 뿐. 몽타주가 재생되는 모든 머신에서 노티파이가 발동해
 *    로컬 즉시 반영되고, 복제 enum 은 뒤늦게 접속한 클라(late-join)까지 상태를 맞춰준다.
 *
 * 전환 흐름(애니메이터가 몽타주 프레임에 AnimNotify_BossSetWeapon 배치):
 *  맨손 -> DualWield -> TwoHanded -> DualWield ... 각 노티파이가 SetWeaponState 만 호출.
 */
UCLASS(ClassGroup = (Boss), meta = (BlueprintSpawnableComponent))
class LOSTARK_API UBossWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBossWeaponComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * 무기 상태 전환. 노티파이가 (모든 머신에서) 호출한다.
	 * 로컬에서 즉시 ApplyWeaponState 로 반영하고, 서버 권위에서는 복제 변수도 갱신해
	 * late-join 클라까지 커버한다. 같은 상태로 반복 호출해도 안전(멱등).
	 */
	UFUNCTION(BlueprintCallable, Category = "Boss|Weapon")
	void SetWeaponState(EBossWeaponState NewState);

	UFUNCTION(BlueprintPure, Category = "Boss|Weapon")
	EBossWeaponState GetWeaponState() const { return WeaponState; }

protected:
	virtual void BeginPlay() override;

	/** BP_Hammer2 (양손에 하나씩 드는 무기) — DualWield 상태에서 wp_RR / wp_L 에 표시 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Weapon")
	TSubclassOf<AActor> DualWeaponClass;

	/** BP_Hammer1 (양손 무기를 합친 무기) — TwoHanded 상태에서 wp_R 에 표시 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Weapon")
	TSubclassOf<AActor> TwoHandedWeaponClass;

	/** Hammer2 오른손 소켓 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Weapon")
	FName DualRightSocket = TEXT("wp_RR");

	/** Hammer2 왼손 소켓 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Weapon")
	FName DualLeftSocket = TEXT("wp_L");

	/** Hammer1(합체무기) 소켓 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss|Weapon")
	FName TwoHandedSocket = TEXT("wp_R");

private:
	/** 현재 무기 상태(복제). 값이 바뀌면 클라에서 OnRep 로 표시 갱신 */
	UPROPERTY(ReplicatedUsing = OnRep_WeaponState)
	EBossWeaponState WeaponState = EBossWeaponState::Unarmed;

	UFUNCTION()
	void OnRep_WeaponState();

	/** 현재 WeaponState 에 맞춰 세 무기의 숨김 여부를 갱신 (멱등, null 안전) */
	void ApplyWeaponState();

	/** 무기 클래스를 소켓에 1회 스폰+부착하고 숨김/무콜리전으로 초기화 */
	AActor* SpawnAndAttachWeapon(TSubclassOf<AActor> WeaponClass, FName SocketName);

	/** 개별 무기 표시/숨김 (null 안전) */
	static void SetWeaponVisible(AActor* Weapon, bool bVisible);

	/** 스폰된 무기 인스턴스 (각 머신 로컬, 비복제) */
	UPROPERTY(Transient)
	TObjectPtr<AActor> DualRightWeapon;

	UPROPERTY(Transient)
	TObjectPtr<AActor> DualLeftWeapon;

	UPROPERTY(Transient)
	TObjectPtr<AActor> TwoHandedWeapon;
};
