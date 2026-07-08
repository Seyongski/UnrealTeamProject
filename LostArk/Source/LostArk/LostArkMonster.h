#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "GameplayAbilitySpec.h"
#include "LostArkPoolableInterface.h"
#include "LostArkCombatInterface.h"
#include "LostArkMonster.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMonsterKilledSignature, ALostArkMonster*, KilledMonster);

UCLASS()
class LOSTARK_API ALostArkMonster : public ACharacter, public ILostArkPoolableInterface, public IAbilitySystemInterface, public ILostArkCombatInterface
{
	GENERATED_BODY()

public:
	ALostArkMonster();

	virtual class UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	virtual void OnAcquiredFromPool_Implementation() override;
	virtual void OnReleasedToPool_Implementation() override;

	virtual void PossessedBy(AController* NewController) override;

	virtual void Die() override;

	virtual bool IsDead() const override { return bIsDead; }

	UFUNCTION(BlueprintCallable, Category = "Monster|State")
	void SetMonsterState(FGameplayTag NewStateTag);

	virtual FGameplayTag GetCurrentStateTag() const override { return CurrentStateTag; }

	virtual void SetCombatState(FGameplayTag NewStateTag) override { SetMonsterState(NewStateTag); }

	UFUNCTION(BlueprintCallable, Category = "Monster|Abilities")
	bool TryActivateAttack();

	UPROPERTY(BlueprintAssignable, Category = "Monster|Events")
	FOnMonsterKilledSignature OnMonsterKilled;

protected:
	virtual void BeginPlay() override;

	void OnSpawnFinished();
	void FinishDeath();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities", meta = (AllowPrivateAccess = "true"))
	class UAbilitySystemComponent* AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities", meta = (AllowPrivateAccess = "true"))
	class ULostArkAttributeSet* AttributeSet;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Monster|Abilities")
	TSubclassOf<class UGameplayAbility> AttackAbilityClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Settings")
	float SpawnDuration;

	/** 에디터에서 조절 가능한 몬스터 기본 공격 사거리 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Settings")
	float BaseAttackRange = 300.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Monster|State")
	FGameplayTag CurrentStateTag;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Monster|State")
	bool bIsDead;

	FGameplayTag StateSpawningTag;
	FGameplayTag StateIdleTag;
	FGameplayTag StateMovingTag;
	FGameplayTag StateAttackingTag;
	FGameplayTag StateDeadTag;

private:
	FTimerHandle SpawnFinishedTimerHandle;
	FTimerHandle DeathTimerHandle;
	FGameplayAbilitySpecHandle AttackAbilityHandle;
};
