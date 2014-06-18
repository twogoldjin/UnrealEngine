// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ModuleInterface.h"

/**
 * The module holding all of the UI related pieces for SubLevels management
 */
class FWorldBrowserModule : public IModuleInterface
{

public:

	/**
	 * Called right after the module DLL has been loaded and the module object has been created
	 */
	virtual void StartupModule();

	/**
	 * Called before the module is unloaded, right before the module object is destroyed.
	 */
	virtual void ShutdownModule();
	
	/**
	 * Creates a levels hierarchy widget
	 */
	virtual TSharedRef<class SWidget> CreateWorldBrowserHierarchy();
	
	/**
	 * Creates a levels details widget
	 */
	virtual TSharedRef<class SWidget> CreateWorldBrowserDetails();

	/**
	 * Creates a levels composition widget
	 */
	virtual TSharedRef<class SWidget> CreateWorldBrowserComposition();

	/**
	 * @return world model shared between all World Browser editors
	 */
	virtual TSharedPtr<class FLevelCollectionModel> SharedWorldModel(UWorld* InWorld);
	
	/**
	 * 
	 */
	DECLARE_EVENT_OneParam(FWorldBrowserModule, FOnBrowseWorld, UWorld*);
	FOnBrowseWorld OnBrowseWorld;
		
private:
	void OnWorldCreated(UWorld* InWorld, const UWorld::InitializationValues IVS);
	void OnWorldCleanup(UWorld* InWorld, bool bSessionEnded, bool bCleanupResources);
	void OnWorldCompositionChanged(UWorld* InWorld);
		
private:
	TWeakPtr<class FLevelCollectionModel>	WorldModel;
};
