// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "ComponentVisualizerManager.h"
#include "ILevelEditor.h"

/** Handle a click on the specified level editor viewport client */
bool FComponentVisualizerManager::HandleClick(FLevelEditorViewportClient* InViewportClient, HHitProxy *HitProxy, const FViewportClick &Click)
{
	bool bHandled = HandleProxyForComponentVis(HitProxy);
	if (bHandled && Click.GetKey() == EKeys::RightMouseButton)
	{
		TSharedPtr<SWidget> MenuWidget = GenerateContextMenuForComponentVis();
		if (MenuWidget.IsValid())
		{
			auto ParentLevelEditorPinned = InViewportClient->ParentLevelEditor.Pin();
			if (ParentLevelEditorPinned.IsValid())
			{
				FSlateApplication::Get().PushMenu(
					ParentLevelEditorPinned.ToSharedRef(),
					MenuWidget.ToSharedRef(),
					FSlateApplication::Get().GetCursorPos(),
					FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));

				return true;
			}
		}
	}

	return false;
}

bool FComponentVisualizerManager::HandleProxyForComponentVis(HHitProxy *HitProxy)
{
	if (HitProxy->IsA(HComponentVisProxy::StaticGetType()))
	{
		HComponentVisProxy* VisProxy = (HComponentVisProxy*)HitProxy;
		const UActorComponent* ClickedComponent = VisProxy->Component.Get();
		if (ClickedComponent != NULL)
		{
			TSharedPtr<FComponentVisualizer> Visualizer = GUnrealEd->FindComponentVisualizer(ClickedComponent->GetClass());
			if (Visualizer.IsValid())
			{
				bool bIsActive = Visualizer->VisProxyHandleClick(VisProxy);
				if (bIsActive)
				{
					// call EndEditing on any currently edited visualizer, if we are going to change it
					if (EditedVisualizer.IsValid() && Visualizer.Get() != EditedVisualizer.Pin().Get())
					{
						EditedVisualizer.Pin()->EndEditing();
					}

					EditedVisualizer = Visualizer;
					return true;
				}
			}
		}
	}
	else
	{
		ClearActiveComponentVis();
	}

	return false;
}

void FComponentVisualizerManager::ClearActiveComponentVis()
{
	if (EditedVisualizer.IsValid())
	{
		EditedVisualizer.Pin()->EndEditing();
		EditedVisualizer = NULL;
	}
}

bool FComponentVisualizerManager::HandleInputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event) const
{
	if(EditedVisualizer.IsValid())
	{
		if(EditedVisualizer.Pin()->HandleInputKey(ViewportClient, Viewport, Key, Event))
		{
			return true;
		}
	}

	return false;
}

bool FComponentVisualizerManager::HandleInputDelta(FEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale) const
{
	if (EditedVisualizer.IsValid() && InViewportClient->GetCurrentWidgetAxis() != EAxisList::None)
	{
		if (EditedVisualizer.Pin()->HandleInputDelta(InViewportClient, InViewport, InDrag, InRot, InScale))
		{
			return true;
		}
	}

	return false;
}


bool FComponentVisualizerManager::GetWidgetLocation(FVector& OutLocation) const
{
	if (EditedVisualizer.IsValid())
	{
		return EditedVisualizer.Pin()->GetWidgetLocation(OutLocation);
	}

	return false;
}


TSharedPtr<SWidget> FComponentVisualizerManager::GenerateContextMenuForComponentVis() const
{
	if (EditedVisualizer.IsValid())
	{
		return EditedVisualizer.Pin()->GenerateContextMenu();
	}

	return TSharedPtr<SWidget>();
}
