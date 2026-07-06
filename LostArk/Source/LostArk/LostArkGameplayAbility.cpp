#include "LostArkGameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayEffect.h"
#include "GameplayTagsModule.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/Actor.h"

ULostArkGameplayAbility::ULostArkGameplayAbility()
{
	HitRadius = 80.f;
	DamageCoefficient = 1.f;
}

void ULostArkGameplayAbility::ApplyDamageInRadius(FVector Origin, float Radius, float DamageAmount, AActor* InstigatorActor)
{
	if (!DamageEffectClass || !InstigatorActor) return;

	UAbilitySystemComponent* InstigatorASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(InstigatorActor);
	if (!InstigatorASC) return;

	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));

	TArray<AActor*> IgnoredActors;
	IgnoredActors.Add(InstigatorActor);

	TArray<AActor*> OverlappedActors;
	UKismetSystemLibrary::SphereOverlapActors(
		InstigatorActor,
		Origin,
		Radius,
		ObjectTypes,
		nullptr,
		IgnoredActors,
		OverlappedActors
	);

	FGameplayEffectContextHandle ContextHandle = InstigatorASC->MakeEffectContext();
	ContextHandle.AddInstigator(InstigatorActor, InstigatorActor);

	for (AActor* HitActor : OverlappedActors)
	{
		if (!HitActor || HitActor == InstigatorActor) continue;

		if (HitActor->GetClass() == InstigatorActor->GetClass()) continue;

		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
		if (!TargetASC) continue;

		FGameplayEffectSpecHandle SpecHandle = InstigatorASC->MakeOutgoingSpec(DamageEffectClass, 1.f, ContextHandle);
		if (!SpecHandle.IsValid()) continue;

		UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(
			SpecHandle,
			FGameplayTag::RequestGameplayTag(FName("Data.Damage"), false),
			DamageAmount
		);

		InstigatorASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
	}
}
