#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "Abilities/LostArkGameplayAbility.h"
#include "LostArkShadowClone.generated.h"

UCLASS()
class LOSTARK_API ALostArkShadowClone : public AActor
{
	GENERATED_BODY()
	
public:	
	ALostArkShadowClone();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class USkeletalMeshComponent* MeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class USkeletalMeshComponent* WeaponMeshComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shadow")
	class UMaterialInterface* ShadowMaterial;

	void InitShadow(
		class USkeletalMeshComponent* SourceMeshComp,
		class USkeletalMeshComponent* SourceWeaponMeshComp,
		class UAnimMontage* MontageToPlay,
		float Rate = 1.0f,
		float DashSpeed = 0.f,
		float DashDuration = 0.f,
		class UAbilitySystemComponent* InInstigatorASC = nullptr,
		TSubclassOf<class UGameplayEffect> InDamageEffectClass = nullptr,
		FDamageShapeParams InDamageShape = FDamageShapeParams()
	);

	void ApplyCloneDamage(FVector Origin, FRotator Rotation);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(ReplicatedUsing = OnRep_CloneData)
	AActor* ReplicatedSourceActor;
	
	UPROPERTY(ReplicatedUsing = OnRep_CloneData)
	class UAnimMontage* ReplicatedMontage;
	
	UPROPERTY(ReplicatedUsing = OnRep_CloneData)
	float ReplicatedDashSpeed;
	
	UPROPERTY(ReplicatedUsing = OnRep_CloneData)
	float ReplicatedDashDuration;

	UFUNCTION()
	void OnRep_CloneData();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

private:
	UFUNCTION()
	void OnMontageEnded(class UAnimMontage* Montage, bool bInterrupted);

	FTimerHandle SelfDestructTimerHandle;

	UFUNCTION()
	void SelfDestruct();

	float CurrentDashSpeed;
	float CurrentDashDuration;
	float ElapsedDashTime;
	FVector DashDirection;

	TWeakObjectPtr<class UAbilitySystemComponent> InstigatorASC;
	TSubclassOf<class UGameplayEffect> DamageEffectClass;
	FDamageShapeParams DamageShape;
};




