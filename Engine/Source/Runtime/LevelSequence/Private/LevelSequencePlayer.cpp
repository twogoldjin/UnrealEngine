// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "LevelSequencePCH.h"
#include "LevelSequencePlayer.h"
#include "MovieScene.h"
#include "MovieSceneSequence.h"
#include "MovieSceneSequenceInstance.h"
#include "LevelSequenceSpawnRegister.h"
#include "Engine/LevelStreaming.h"


struct FTickAnimationPlayers : public FTickableGameObject
{
	TArray<TWeakObjectPtr<ULevelSequencePlayer>> ActiveInstances;
	
	virtual bool IsTickable() const override
	{
		return ActiveInstances.Num() != 0;
	}

	virtual TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FTickAnimationPlayers, STATGROUP_Tickables);
	}
	
	virtual void Tick(float DeltaSeconds) override
	{
		for (int32 Index = 0; Index < ActiveInstances.Num();)
		{
			if (auto* Player = ActiveInstances[Index].Get())
			{
				Player->Update(DeltaSeconds);
				++Index;
			}
			else
			{
				ActiveInstances.RemoveAt(Index, 1, false);
			}
		}
	}
};

struct FAutoDestroyAnimationTicker
{
	FAutoDestroyAnimationTicker()
	{
		FCoreDelegates::OnPreExit.AddLambda([&]{
			Impl.Reset();
		});
	}
	
	void Add(ULevelSequencePlayer* Player)
	{
		if (!Impl.IsValid())
		{
			Impl.Reset(new FTickAnimationPlayers);
		}
		Impl->ActiveInstances.Add(Player);
	}

	TUniquePtr<FTickAnimationPlayers> Impl;
} GAnimationPlayerTicker;

/* ULevelSequencePlayer structors
 *****************************************************************************/

ULevelSequencePlayer::ULevelSequencePlayer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, LevelSequence(nullptr)
	, bIsPlaying(false)
	, TimeCursorPosition(0.0f)
	, LastCursorPosition(0.0f)
	, StartTime(0.f)
	, EndTime(0.f)
	, CurrentNumLoops(0)
	, bHasCleanedUpSequence(false)
{
	SpawnRegister = MakeShareable(new FLevelSequenceSpawnRegister);
}


/* ULevelSequencePlayer interface
 *****************************************************************************/

ULevelSequencePlayer* ULevelSequencePlayer::CreateLevelSequencePlayer(UObject* WorldContextObject, ULevelSequence* InLevelSequence, FLevelSequencePlaybackSettings Settings)
{
	if (InLevelSequence == nullptr)
	{
		return nullptr;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);
	check(World != nullptr);

	ULevelSequencePlayer* NewPlayer = NewObject<ULevelSequencePlayer>(GetTransientPackage(), NAME_None, RF_Transient);
	check(NewPlayer != nullptr);

	NewPlayer->Initialize(InLevelSequence, World, Settings);

	// Automatically tick this player
	GAnimationPlayerTicker.Add(NewPlayer);

	return NewPlayer;
}


bool ULevelSequencePlayer::IsPlaying() const
{
	return bIsPlaying;
}


void ULevelSequencePlayer::Pause()
{
	bIsPlaying = false;
}

void ULevelSequencePlayer::Stop()
{
	bIsPlaying = false;
	TimeCursorPosition = PlaybackSettings.PlayRate < 0.f ? GetLength() : 0.f;
	CurrentNumLoops = 0;

	// todo: Trigger an event?
}

void ULevelSequencePlayer::Play()
{
	if ((LevelSequence == nullptr) || !World.IsValid())
	{
		return;
	}

	// @todo Sequencer playback: Should we recreate the instance every time?
	// We must not recreate the instance since it holds stateful information (such as which objects it has spawned). Recreating the instance would break any 
	if (!RootMovieSceneInstance.IsValid())
	{
		RootMovieSceneInstance = MakeShareable(new FMovieSceneSequenceInstance(*LevelSequence));
		RootMovieSceneInstance->RefreshInstance(*this);
	}

	bIsPlaying = true;
	bHasCleanedUpSequence = false;

	UpdateMovieSceneInstance(TimeCursorPosition, TimeCursorPosition);
}

void ULevelSequencePlayer::PlayLooping(int32 NumLoops)
{
	PlaybackSettings.LoopCount = NumLoops;
	Play();
}

float ULevelSequencePlayer::GetPlaybackPosition() const
{
	return TimeCursorPosition;
}

void ULevelSequencePlayer::SetPlaybackPosition(float NewPlaybackPosition)
{
	UpdateTimeCursorPosition(NewPlaybackPosition);
	UpdateMovieSceneInstance(TimeCursorPosition, LastCursorPosition);
}

float ULevelSequencePlayer::GetLength() const
{
	return EndTime - StartTime;
}

float ULevelSequencePlayer::GetPlayRate() const
{
	return PlaybackSettings.PlayRate;
}

void ULevelSequencePlayer::SetPlayRate(float PlayRate)
{
	PlaybackSettings.PlayRate = PlayRate;
}

void ULevelSequencePlayer::SetPlaybackRange( const float NewStartTime, const float NewEndTime )
{
	StartTime = NewStartTime;
	EndTime = FMath::Max(NewEndTime, StartTime);

	TimeCursorPosition = FMath::Clamp(TimeCursorPosition, 0.f, GetLength());
}

void ULevelSequencePlayer::UpdateTimeCursorPosition(float NewPosition)
{
	float Length = GetLength();

	if ((NewPosition >= Length) || (NewPosition < 0))
	{
		// loop playback
		if (PlaybackSettings.LoopCount < 0 || CurrentNumLoops < PlaybackSettings.LoopCount)
		{
			++CurrentNumLoops;
			const float Overplay = FMath::Fmod(NewPosition, Length);
			TimeCursorPosition = Overplay < 0 ? Length + Overplay : Overplay;
			LastCursorPosition = TimeCursorPosition;

			SpawnRegister->ForgetExternallyOwnedSpawnedObjects(*this);

			return;
		}

		// stop playback
		bIsPlaying = false;
		CurrentNumLoops = 0;
	}

	LastCursorPosition = TimeCursorPosition;
	TimeCursorPosition = NewPosition;
}

/* ULevelSequencePlayer implementation
 *****************************************************************************/

void ULevelSequencePlayer::Initialize(ULevelSequence* InLevelSequence, UWorld* InWorld, const FLevelSequencePlaybackSettings& Settings)
{
	LevelSequence = InLevelSequence;

	World = InWorld;
	PlaybackSettings = Settings;

	if (UMovieScene* MovieScene = LevelSequence->GetMovieScene())
	{
		TRange<float> PlaybackRange = MovieScene->GetPlaybackRange();
		SetPlaybackRange(PlaybackRange.GetLowerBoundValue(), PlaybackRange.GetUpperBoundValue());
	}

	// Ensure everything is set up, ready for playback
	Stop();
}


/* IMovieScenePlayer interface
 *****************************************************************************/

void ULevelSequencePlayer::GetRuntimeObjects(TSharedRef<FMovieSceneSequenceInstance> MovieSceneInstance, const FGuid& ObjectId, TArray<TWeakObjectPtr<UObject>>& OutObjects) const
{
	UObject* FoundObject = MovieSceneInstance->FindObject(ObjectId, *this);
	if (FoundObject)
	{
		OutObjects.Add(FoundObject);
	}
}


void ULevelSequencePlayer::UpdateCameraCut(UObject* CameraObject, UObject* UnlockIfCameraObject, bool bJumpCut) const
{
	// skip missing player controller
	APlayerController* PC = World->GetGameInstance()->GetFirstLocalPlayerController();

	if (PC == nullptr)
	{
		return;
	}

	// skip same view target
	AActor* ViewTarget = PC->GetViewTarget();

	if (CameraObject == ViewTarget)
	{
		return;
	}

	// skip unlocking if the current view target differs
	ACameraActor* UnlockIfCameraActor = Cast<ACameraActor>(UnlockIfCameraObject);

	if ((CameraObject == nullptr) && (UnlockIfCameraActor != ViewTarget))
	{
		return;
	}

	// override the player controller's view target
	ACameraActor* CameraActor = Cast<ACameraActor>(CameraObject);

	FViewTargetTransitionParams TransitionParams;
	PC->SetViewTarget(CameraActor, TransitionParams);

	if (PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager->bClientSimulatingViewTarget = (CameraActor != nullptr);
		PC->PlayerCameraManager->bGameCameraCutThisFrame = true;
	}
}

void ULevelSequencePlayer::SetViewportSettings(const TMap<FViewportClient*, EMovieSceneViewportParams>& ViewportParamsMap)
{
}

void ULevelSequencePlayer::GetViewportSettings(TMap<FViewportClient*, EMovieSceneViewportParams>& ViewportParamsMap) const
{
}

EMovieScenePlayerStatus::Type ULevelSequencePlayer::GetPlaybackStatus() const
{
	return bIsPlaying ? EMovieScenePlayerStatus::Playing : EMovieScenePlayerStatus::Stopped;
}

void ULevelSequencePlayer::SetPlaybackStatus(EMovieScenePlayerStatus::Type InPlaybackStatus)
{
}

void ULevelSequencePlayer::AddOrUpdateMovieSceneInstance(UMovieSceneSection& MovieSceneSection, TSharedRef<FMovieSceneSequenceInstance> InstanceToAdd)
{
}

void ULevelSequencePlayer::RemoveMovieSceneInstance(UMovieSceneSection& MovieSceneSection, TSharedRef<FMovieSceneSequenceInstance> InstanceToRemove)
{
}

TSharedRef<FMovieSceneSequenceInstance> ULevelSequencePlayer::GetRootMovieSceneSequenceInstance() const
{
	return RootMovieSceneInstance.ToSharedRef();
}

UObject* ULevelSequencePlayer::GetPlaybackContext() const
{
	return World.Get();
}

TArray<UObject*> ULevelSequencePlayer::GetEventContexts() const
{
	TArray<UObject*> EventContexts;
	if (World.IsValid())
	{
		if (World->GetLevelScriptActor())
		{
			EventContexts.Add(World->GetLevelScriptActor());
		}

		for (ULevelStreaming* StreamingLevel : World->StreamingLevels)
		{
			if (StreamingLevel->GetLevelScriptActor())
			{
				EventContexts.Add(StreamingLevel->GetLevelScriptActor());
			}
		}
	}
	return EventContexts;
}

void ULevelSequencePlayer::Update(const float DeltaSeconds)
{
	if (bIsPlaying)
	{
		UpdateTimeCursorPosition(TimeCursorPosition + DeltaSeconds * PlaybackSettings.PlayRate);
		UpdateMovieSceneInstance(TimeCursorPosition, LastCursorPosition);
	}
	else if (!bHasCleanedUpSequence && TimeCursorPosition >= GetLength())
	{
		UpdateMovieSceneInstance(TimeCursorPosition, LastCursorPosition);

		bHasCleanedUpSequence = true;
		
		SpawnRegister->ForgetExternallyOwnedSpawnedObjects(*this);
		SpawnRegister->CleanUp(*this);
	}
}

void ULevelSequencePlayer::UpdateMovieSceneInstance(float CurrentPosition, float PreviousPosition)
{
	if(RootMovieSceneInstance.IsValid())
	{
		EMovieSceneUpdateData UpdateData(CurrentPosition + StartTime, PreviousPosition + StartTime);
		RootMovieSceneInstance->Update(UpdateData, *this);
#if WITH_EDITOR
		OnLevelSequencePlayerUpdate.Broadcast(*this, CurrentPosition, PreviousPosition);
#endif
	}
}

IMovieSceneSpawnRegister& ULevelSequencePlayer::GetSpawnRegister()
{
	return *SpawnRegister;
}