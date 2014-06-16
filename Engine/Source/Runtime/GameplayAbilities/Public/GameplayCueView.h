// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayTags.h"
#include "GameplayEffect.h"
#include "GameplayCueView.generated.h"

/** 
	This is meant to give a base implementation for handling GameplayCues. It will handle very simple things like
	creating and destroying ParticleSystemComponents, playing audio, etc.

	It is inevitable that specific actors and games will have to handle GameplayCues in their own way.
*/

USTRUCT()
struct FGameplayCueViewEffects
{
	// The instantied effects
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TWeakObjectPtr<UParticleSystemComponent> ParticleSystemComponent;

	UPROPERTY()
	TWeakObjectPtr<UAudioComponent>	AudioComponent;

	UPROPERTY()
	TWeakObjectPtr<AActor>	SpawnedActor;
};

USTRUCT()
struct FGameplayCueViewInfo
{
	GENERATED_USTRUCT_BODY()

	// -----------------------------
	//	Qualifiers
	// -----------------------------

	UPROPERTY(EditDefaultsOnly, Category = GameplayCue)
	FGameplayTagContainer Tags;

	UPROPERTY(EditDefaultsOnly, Category = GameplayModifier)
	TEnumAsByte<EGameplayCueEvent::Type> CueType;

	// This needs to be fleshed out more:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameplayCue)
	bool	InstigatorLocalOnly;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameplayCue)
	bool	TargetLocalOnly;

	// -----------------------------
	//	Effects
	// -----------------------------

	/** Local/remote sounds to play for weapon attacks against specific surfaces */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameplayCue)
	class USoundBase* Sound;

	/** Effects to play for weapon attacks against specific surfaces */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameplayCue)
	class UParticleSystem* ParticleSystem;

	/** Effects to play for weapon attacks against specific surfaces */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameplayCue)
	class TSubclassOf<AActor> ActorClass;

	virtual TSharedPtr<FGameplayCueViewEffects> SpawnViewEffects(AActor *Owner, TArray<UObject*> *SpawnedObjects, const FGameplayEffectInstigatorContext InstigatorContext) const;
};

UCLASS(BlueprintType)
class GAMEPLAYABILITIES_API UGameplayCueView : public UDataAsset
{
	// Defines how a GameplayCue is translated into viewable components
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditDefaultsOnly, Category = GameplayCue)
	TArray<FGameplayCueViewInfo>	Views;
};

USTRUCT()
struct GAMEPLAYABILITIES_API FGameplayCueHandler
{
	GENERATED_USTRUCT_BODY()

	FGameplayCueHandler()
		: Owner(NULL)
	{
	}

	UPROPERTY()
	AActor * Owner;

	UPROPERTY(EditDefaultsOnly, Category = GameplayCue)
	TArray<UGameplayCueView*>	Definitions;
	
	TMap<FGameplayTag, TArray< TSharedPtr<FGameplayCueViewEffects> > >	SpawnedViewEffects;

	UPROPERTY()
	TArray<UObject*> SpawnedObjects;

	virtual void GameplayCueActivated(const FGameplayTagContainer & GameplayCueTags, float NormalizedMagnitude, const FGameplayEffectInstigatorContext InstigatorContext);
	
	virtual void GameplayCueExecuted(const FGameplayTagContainer & GameplayCueTags, float NormalizedMagnitude, const FGameplayEffectInstigatorContext InstigatorContext);
	
	virtual void GameplayCueAdded(const FGameplayTagContainer & GameplayCueTags, float NormalizedMagnitude, const FGameplayEffectInstigatorContext InstigatorContext);
	
	virtual void GameplayCueRemoved(const FGameplayTagContainer & GameplayCueTags, float NormalizedMagnitude, const FGameplayEffectInstigatorContext InstigatorContext);

private:

	FGameplayCueViewInfo * GetBestMatchingView(EGameplayCueEvent::Type Type, const FGameplayTag BaseTag);

	void ClearEffects(TArray< TSharedPtr<FGameplayCueViewEffects > > &Effects);


};