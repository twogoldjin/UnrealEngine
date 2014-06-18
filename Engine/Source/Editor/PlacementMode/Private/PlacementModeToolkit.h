// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "BaseToolkit.h"
#include "SPlacementModeTools.h"

class FPlacementModeToolkit : public FModeToolkit
{
public:

	FPlacementModeToolkit()
	{
		SAssignNew( PlacementModeTools, SPlacementModeTools );
	}

	/** IToolkit interface */
	virtual FName GetToolkitFName() const override { return FName("PlacementMode"); }
	virtual FText GetBaseToolkitName() const override { return NSLOCTEXT("BuilderModeToolkit", "DisplayName", "Builder"); }
	virtual class FEdMode* GetEditorMode() const override { return GLevelEditorModeTools().GetActiveMode( FBuiltinEditorModes::EM_Placement ); }
	virtual TSharedPtr<class SWidget> GetInlineContent() const override { return PlacementModeTools; }

private:

	TSharedPtr<SPlacementModeTools> PlacementModeTools;
};
