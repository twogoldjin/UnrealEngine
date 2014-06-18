// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "WorldBrowserPrivatePCH.h"

#include "Editor/PropertyEditor/Public/IDetailsView.h"
#include "Editor/LevelEditor/Public/LevelEditor.h"
#include "SWorldDetails.h"

#define LOCTEXT_NAMESPACE "WorldBrowser"

SWorldDetails::SWorldDetails()
{
}

SWorldDetails::~SWorldDetails()
{
	GLevelEditorModeTools().DeactivateMode(FBuiltinEditorModes::EM_StreamingLevel);
	
	FWorldBrowserModule& WorldBrowserModule = FModuleManager::GetModuleChecked<FWorldBrowserModule>("WorldBrowser");
	WorldBrowserModule.OnBrowseWorld.RemoveAll(this);
	
	OnBrowseWorld(nullptr);
}

void SWorldDetails::Construct(const FArguments& InArgs)
{
	FWorldBrowserModule& WorldBrowserModule = FModuleManager::GetModuleChecked<FWorldBrowserModule>("WorldBrowser");
	WorldBrowserModule.OnBrowseWorld.AddSP(this, &SWorldDetails::OnBrowseWorld);
	
	OnBrowseWorld(InArgs._InWorld);
}

void SWorldDetails::OnBrowseWorld(UWorld* InWorld)
{
	// Remove old world bindings
	ChildSlot
	[
		SNullWidget::NullWidget
	];
	
	if (WorldModel.IsValid())
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		WorldModel->UnregisterDetailsCustomization(PropertyModule, DetailsView);
		WorldModel->SelectionChanged.RemoveAll(this);
	}
	
	WorldModel = nullptr;

	// Bind to a new world
	if (InWorld)
	{
		FWorldBrowserModule& WorldBrowserModule = FModuleManager::GetModuleChecked<FWorldBrowserModule>("WorldBrowser");
		WorldModel = WorldBrowserModule.SharedWorldModel(InWorld);

		WorldModel->SelectionChanged.AddSP(this, &SWorldDetails::OnSelectionChanged);
	
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		FDetailsViewArgs Args(false, false, false, false, true);
		Args.bShowActorLabel = false;
	
		DetailsView = PropertyModule.CreateDetailView(Args);
		ChildSlot
		[
			SNew(SVerticalBox)

			// Inspect level box
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
				[
					SNew(SHorizontalBox)

					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Left)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("Inspect level:")))
					]
					
					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Left)
					.Padding(4,0,4,0)
					[
						SAssignNew(SubLevelsComboBox, SComboBox<TSharedPtr<FLevelModel>>)
						.OptionsSource(&WorldModel->GetFilteredLevels())
						.OnSelectionChanged(this, &SWorldDetails::OnSetInspectedLevel)
						.OnGenerateWidget(this, &SWorldDetails::HandleInspectedLevelComboBoxGenerateWidget)
						.Content()
						[
							SNew(STextBlock)
							.Text(this, &SWorldDetails::GetInspectedLevelText)
						]
					]
					
					// Button to summon levels hierarchy tab
					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Left)
					[
						SNew(SButton)
						.ButtonStyle(FEditorStyle::Get(), "ToggleButton")
						.OnClicked(this, &SWorldDetails::OnSummonHierarchy)
						.ToolTipText(LOCTEXT("SummonHierarchyToolTipText", "Summons sublevels hierarchy"))
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.Content()
						[
							SNew(SImage)
							.Image(this, &SWorldDetails::GetSummonHierarchyBrush)
						]
					]

					// Button to summon world composition tab
					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Left)
					[
						SNew(SButton)
						.Visibility(this, &SWorldDetails::GetCompositionButtonVisibility)
						.ButtonStyle(FEditorStyle::Get(), "ToggleButton")
						.OnClicked(this, &SWorldDetails::OnSummonComposition)
						.ToolTipText(LOCTEXT("SummonHierarchyToolTipText", "Summons world composition"))
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.Content()
						[
							SNew(SImage)
							.Image(this, &SWorldDetails::GetSummonCompositionBrush)
						]
					]
				]
			]
			
			// Level details
			+SVerticalBox::Slot()
			.FillHeight(1.f)
			.Padding(0,4,0,0)
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
				[
					DetailsView.ToSharedRef()
				]
			]
		];

		WorldModel->RegisterDetailsCustomization(PropertyModule, DetailsView);

		// Current selection
		OnSelectionChanged();
	}
}

void SWorldDetails::OnSelectionChanged()
{
	auto SelectedLevels = WorldModel->GetSelectedLevels();
	TArray<UObject*> TileProperties;
	
	for (auto It = SelectedLevels.CreateIterator(); It; ++It)
	{
		UObject* PropertiesObject = (*It)->GetNodeObject();
		if (PropertiesObject)
		{
			TileProperties.Add(PropertiesObject);
		}
	}

	DetailsView->SetObjects(TileProperties, true);
}

void SWorldDetails::OnSetInspectedLevel(TSharedPtr<FLevelModel> InLevelModel, ESelectInfo::Type SelectInfo)
{
	FLevelModelList SelectedLevels; SelectedLevels.Add(InLevelModel);
	WorldModel->SetSelectedLevels(SelectedLevels);
}

TSharedRef<SWidget> SWorldDetails::HandleInspectedLevelComboBoxGenerateWidget(TSharedPtr<FLevelModel> InLevelModel) const
{
	return SNew(SBox)
	.Padding(4)
	[
		SNew(STextBlock).Text(InLevelModel->GetDisplayName())
	];
}

FText SWorldDetails::GetInspectedLevelText() const
{
	const FLevelModelList& SelectedLevels = WorldModel->GetSelectedLevels();
	
	if (SelectedLevels.Num() > 1)
	{
		return LOCTEXT("MultipleInspectedLevelText", "Multiple Values");
	}
	else if (SelectedLevels.Num() == 1)
	{
		return FText::FromString(SelectedLevels[0]->GetDisplayName());
	}

	return LOCTEXT("EmptyInspectedLevelText", "None");
}

FReply SWorldDetails::OnSummonHierarchy()
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>( "LevelEditor" );
	LevelEditorModule.SummonWorldBrowserHierarchy();
	return FReply::Handled();
}

const FSlateBrush* SWorldDetails::GetSummonHierarchyBrush() const
{
	return FEditorStyle::GetBrush("WorldBrowser.HierarchyButtonBrush");
}

EVisibility SWorldDetails::GetCompositionButtonVisibility() const
{
	return WorldModel->IsTileWorld() ? EVisibility::Visible : EVisibility::Collapsed;
}

FReply SWorldDetails::OnSummonComposition()
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>( "LevelEditor" );
	LevelEditorModule.SummonWorldBrowserComposition();
	return FReply::Handled();
}

const FSlateBrush* SWorldDetails::GetSummonCompositionBrush() const
{
	return FEditorStyle::GetBrush("WorldBrowser.CompositionButtonBrush");
}


#undef LOCTEXT_NAMESPACE