// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "LevelSequencePCH.h"
#include "LevelSequenceSpawnRegister.h"
#include "MovieSceneSequenceInstance.h"
#include "MovieSceneSpawnable.h"
#include "MovieSceneSequence.h"
#include "MovieScene.h"
#include "MovieSceneCommonHelpers.h"
#include "Particles/ParticleSystem.h"
#include "IMovieScenePlayer.h"

static const FName SequencerActorTag(TEXT("SequencerActor"));

UObject* FLevelSequenceSpawnRegister::SpawnObject(const FGuid& BindingId, FMovieSceneSequenceInstance& SequenceInstance, IMovieScenePlayer& Player)
{
	FMovieSceneSpawnRegisterKey Key(BindingId, SequenceInstance);

	FSpawnedObject* Existing = Register.Find(Key);
	UObject* ObjectInstance = Existing ? Existing->Object.Get() : nullptr;
	if (ObjectInstance)
	{
		return ObjectInstance;
	}

	// Find the spawnable definition
	FMovieSceneSpawnable* Spawnable = SequenceInstance.GetSequence()->GetMovieScene()->FindSpawnable(BindingId);
	if (!Spawnable)
	{
		return nullptr;
	}

	AActor* ObjectTemplate = Cast<AActor>(Spawnable->GetObjectTemplate());
	if (!ObjectTemplate)
	{
		return nullptr;
	}

	// @todo sequencer: We should probably spawn these in a specific sub-level!
	// World->CurrentLevel = ???;

	const FName ActorName = NAME_None;

	// Override the object flags so that RF_Transactional is not set.  Puppet actors are never transactional
	// @todo sequencer: These actors need to avoid any transaction history.  However, RF_Transactional can currently be set on objects on the fly!
	// NOTE: We are omitting RF_Transactional intentionally
	const EObjectFlags ObjectFlags = RF_Transient;

	// @todo sequencer livecapture: Consider using SetPlayInEditorWorld() and RestoreEditorWorld() here instead
	
	// @todo sequencer actors: We need to make sure puppet objects aren't copied into PIE/SIE sessions!  They should be omitted from that duplication!

	// Spawn the puppet actor
	FActorSpawnParameters SpawnInfo;
	{
		SpawnInfo.Name = ActorName;
		SpawnInfo.ObjectFlags = ObjectFlags;
		SpawnInfo.Template = ObjectTemplate;
	}

	FTransform SpawnTransform;

	if (USceneComponent* RootComponent = ObjectTemplate->GetRootComponent())
	{
		SpawnTransform.SetTranslation(RootComponent->RelativeLocation);
		SpawnTransform.SetRotation(RootComponent->RelativeRotation.Quaternion());
	}

	{
		// Disable all particle components so that they don't auto fire as soon as the actor is spawned. The particles should be triggered through the particle track.
		TArray<UActorComponent*> ParticleComponents = ObjectTemplate->GetComponentsByClass(UParticleSystemComponent::StaticClass());
		for (int32 ComponentIdx = 0; ComponentIdx < ParticleComponents.Num(); ++ComponentIdx)
		{
			ParticleComponents[ComponentIdx]->bAutoActivate = false;
		}
	}

	UWorld* WorldContext = Cast<UWorld>(Player.GetPlaybackContext());
	if(WorldContext == nullptr)
	{
		WorldContext = GWorld;
	}

	// @todo: Remove when instanced components work correctly on spawned templates
	TArray<UActorComponent*> TemplateInstanceComponents = ObjectTemplate->GetInstanceComponents();
	ObjectTemplate->ClearInstanceComponents(false);

	AActor* SpawnedActor = WorldContext->SpawnActorAbsolute(ObjectTemplate->GetClass(), SpawnTransform, SpawnInfo);
	if (!SpawnedActor)
	{
		return nullptr;
	}

	// First, duplicate instance components
	for (UActorComponent* TemplateComponent : TemplateInstanceComponents)
	{
		ObjectTemplate->AddInstanceComponent(TemplateComponent);

		UActorComponent* NewComponent = DuplicateObject<UActorComponent>(TemplateComponent, SpawnedActor, TemplateComponent->GetFName());
		SpawnedActor->AddInstanceComponent(NewComponent);
	}

	// Second, attach the components. We do this as a different pass, since instance components may be attached to each other
	for (UActorComponent* ActorComp : SpawnedActor->GetInstanceComponents())
	{
		USceneComponent* SceneComp = Cast<USceneComponent>(ActorComp);
		if (!SceneComp)
		{
			continue;
		}
		
		USceneComponent* NewParent = SpawnedActor->GetRootComponent();

		USceneComponent* OldParent = SceneComp->GetAttachParent();
		if (OldParent)
		{
			FString PathName = OldParent->GetPathName(OldParent->GetOwner());

			NewParent = FindObject<USceneComponent>(SpawnedActor, *PathName);
		}

		if (ensure(NewParent))
		{
			SceneComp->AttachToComponent(NewParent, FAttachmentTransformRules::KeepRelativeTransform, SceneComp->GetAttachSocketName());
		}
	}

	SpawnedActor->RegisterAllComponents();

	// tag this actor so we know it was spawned by sequencer
	SpawnedActor->Tags.Add(SequencerActorTag);

	Register.Add(Key, FSpawnedObject(BindingId, *SpawnedActor, Spawnable->GetSpawnOwnership()));

	SequenceInstance.OnObjectSpawned(BindingId, *SpawnedActor, Player);
	return SpawnedActor;
}

void FLevelSequenceSpawnRegister::DestroySpawnedObject(const FGuid& BindingId, FMovieSceneSequenceInstance& SequenceInstance, IMovieScenePlayer& Player)
{
	FMovieSceneSpawnRegisterKey Key(BindingId, SequenceInstance);
	
	FSpawnedObject* Existing = Register.Find(Key);
	UObject* SpawnedObject = Existing ? Existing->Object.Get() : nullptr;
	if (SpawnedObject)
	{
		PreDestroyObject(*SpawnedObject, BindingId, SequenceInstance);
		DestroySpawnedObject(*SpawnedObject);
	}

	SequenceInstance.OnSpawnedObjectDestroyed(BindingId, Player);
	Register.Remove(Key);
}

void FLevelSequenceSpawnRegister::PreDestroyObject(UObject& Object, const FGuid& BindingId, FMovieSceneSequenceInstance& SequenceInstance)
{
}

void FLevelSequenceSpawnRegister::DestroySpawnedObject(UObject& Object)
{
	AActor* Actor = Cast<AActor>(&Object);
	if (!ensure(Actor))
	{
		return;
	}

	UWorld* World = Actor->GetWorld();
	if (ensure(World))
	{
		const bool bNetForce = false;
		const bool bShouldModifyLevel = false;
		World->DestroyActor(Actor, bNetForce, bShouldModifyLevel);
	}
}

void FLevelSequenceSpawnRegister::ForgetExternallyOwnedSpawnedObjects(IMovieScenePlayer& Player)
{
	for (auto It = Register.CreateIterator(); It; ++It)
	{
		if (It.Value().Ownership == ESpawnOwnership::External)
		{
			TSharedPtr<FMovieSceneSequenceInstance> SequenceInstance = It.Key().SequenceInstance.Pin();
			if (SequenceInstance.IsValid())
			{
				SequenceInstance->OnSpawnedObjectDestroyed(It.Key().BindingId, Player);
			}
			It.RemoveCurrent();
		}
	}
}

void FLevelSequenceSpawnRegister::DestroyObjectsByPredicate(IMovieScenePlayer& Player, const TFunctionRef<bool(const FGuid&, ESpawnOwnership, FMovieSceneSequenceInstance&)>& Predicate)
{
	for (auto It = Register.CreateIterator(); It; ++It)
	{
		FMovieSceneSequenceInstance* ThisInstance = It.Key().SequenceInstance.Pin().Get();
		if (!ThisInstance || Predicate(It.Value().Guid, It.Value().Ownership, *ThisInstance))
		{
			UObject* SpawnedObject = It.Value().Object.Get();
			if (SpawnedObject)
			{
				if (ThisInstance)
				{
					PreDestroyObject(*SpawnedObject, It.Key().BindingId, *ThisInstance);
				}
				DestroySpawnedObject(*SpawnedObject);
			}

			if (ThisInstance)
			{
				ThisInstance->OnSpawnedObjectDestroyed(It.Key().BindingId, Player);
			}
			It.RemoveCurrent();
		}
	}
}

void FLevelSequenceSpawnRegister::PreUpdateSequenceInstance(FMovieSceneSequenceInstance& Instance, IMovieScenePlayer& Player)
{
	++CurrentlyUpdatingSequenceCount;
	ActiveInstances.Add(Instance.AsShared());
}

void FLevelSequenceSpawnRegister::PostUpdateSequenceInstance(FMovieSceneSequenceInstance& InInstance, IMovieScenePlayer& Player)
{
	if (--CurrentlyUpdatingSequenceCount == 0)
	{
		for (TWeakPtr<FMovieSceneSequenceInstance>& WeakInstance : PreviouslyActiveInstances)
		{
			TSharedPtr<FMovieSceneSequenceInstance> Instance = WeakInstance.Pin();
			if (Instance.IsValid() && !ActiveInstances.Contains(Instance))
			{
				OnSequenceExpired(*Instance, Player);
			}
		}

		Swap(ActiveInstances, PreviouslyActiveInstances);
		ActiveInstances.Reset();
	}
}