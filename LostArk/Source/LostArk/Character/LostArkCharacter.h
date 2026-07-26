#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayAbilitySpec.h"
#include "Core/LostArkCombatInterface.h"
#include "Core/LostArkAbilityTypes.h"
#include "LostArkCharacter.generated.h"

USTRUCT(BlueprintType)
struct FLostArkSkillInputBind
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	class UInputAction* InputAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ELostArkAbilityInputID InputID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<class UGameplayAbility> AbilityClass;
};

UCLASS(Blueprintable)
class ALostArkCharacter : public ACharacter, public IAbilitySystemInterface, public ILostArkCombatInterface
{
	GENERATED_BODY()

public:
	ALostArkCharacter();

	virtual class UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	virtual void PossessedBy(AController* NewController) override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	virtual void BeginPlay() override;

	virtual void Die() override;

	virtual void Revive() override;

	virtual bool IsDead() const override { return bIsDead; }

	virtual FGameplayTag GetCurrentStateTag() const override { return CurrentStateTag; }

	virtual void SetCombatState(FGameplayTag NewStateTag) override;

	virtual void ShowDamageText(float DamageAmount) override;

	// 서버가 클라이언트에게 직접 호출 - 피격된 캐릭터 화면에서만 데미지 텍스트 생성
	UFUNCTION(Client, Reliable)
	void Client_ShowDamageText(float DamageAmount, FVector SpawnLocation);

	UFUNCTION(BlueprintCallable, Category = "Character|Abilities")
	void RequestComboAttackInput();

	UFUNCTION(BlueprintImplementableEvent, Category = "Character|UI")
	void OnCounterSuccess_UI();

	UFUNCTION(BlueprintCallable, Category = "Character|Weapon")
	void SetWeaponEquipped(bool bIsEquipped);

	UFUNCTION(BlueprintPure, Category = "Character|Weapon")
	bool IsWeaponEquipped() const { return bIsWeaponEquipped; }

	FORCEINLINE class UCameraComponent* GetTopDownCameraComponent() const { return TopDownCameraComponent; }
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	FORCEINLINE class USkeletalMeshComponent* GetWeaponMesh() const { return WeaponMesh; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UFUNCTION(Server, Reliable)
	void Server_SetCharacterRotation(FRotator NewRotation);
public:
	UPROPERTY(BlueprintReadWrite, Category = "Character|Anim", Replicated)
	bool bIsLeftFootForward;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character|UI")
	TSubclassOf<class ULostArkHUDWidget> CharacterHUDClass;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities")
	class UAbilitySystemComponent* AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities")
	class ULostArkAttributeSet* AttributeSet;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	class USkeletalMeshComponent* WeaponMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	FName WeaponEquippedSocketName = FName("b_wp_1");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	FName WeaponUnequippedSocketName = FName("b_wp_back");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	float SheathWeaponTimeout = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	class UAnimMontage* SheathWeaponMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character|UI")
	TSubclassOf<class ALostArkDamageTextActor> DamageTextClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character|Abilities")
	TSubclassOf<class UGameplayAbility> ComboAttackAbilityClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character|Abilities")
	TSubclassOf<class UGameplayAbility> JustGuardAbilityClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character|Abilities")
	TArray<FLostArkSkillInputBind> SkillInputBinds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character|Stats")
	float InitialMaxHealth = 1000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character|Stats")
	float InitialMaxMana = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character|Movement")
	float BaseRunSpeed;

	UPROPERTY(BlueprintReadOnly, Category = "Character", Replicated)
	bool bIsDead;

	UPROPERTY(BlueprintReadOnly, Category = "Character|Weapon", ReplicatedUsing = OnRep_IsWeaponEquipped)
	bool bIsWeaponEquipped;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character|State", Replicated)
	FGameplayTag CurrentStateTag;

private:
	UFUNCTION()
	void OnRep_IsWeaponEquipped();

	void OnSkillInputPressed(ELostArkAbilityInputID InputID);
	void OnSkillInputReleased(ELostArkAbilityInputID InputID);

	void HandleCounterSuccessEvent(const FGameplayEventData* Payload);

	void OnAttackingTagChanged(const FGameplayTag CallbackTag, int32 NewCount);
	void PlaySheathWeaponMontage();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlaySheathWeaponMontage();

	FTimerHandle SheathWeaponTimerHandle;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class UCameraComponent* TopDownCameraComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class USpringArmComponent* CameraBoom;

	FGameplayAbilitySpecHandle ComboAttackAbilityHandle;
};







