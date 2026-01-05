// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnrealSplat.h"
#include "SUnrealSplatWindow.h"
#include "ToolMenus.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/Docking/TabManager.h"

IMPLEMENT_MODULE(FUnrealSplatModule, UnrealSplat)

#define LOCTEXT_NAMESPACE "FUnrealSplatModule"

// Define the tab name
const FName FUnrealSplatModule::PreprocessorTabName(TEXT("UnrealSplatPreprocessor"));

// ============================================================================
// Toolbar Button Widget
// ============================================================================

void SUnrealSplatToolbarButton::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SButton)
		.OnClicked(this, &SUnrealSplatToolbarButton::OnButtonClicked)
		.ContentPadding(FMargin(5.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(0.0f, 0.0f, 5.0f, 0.0f))
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("LevelEditor.MeshPaintMode"))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ToolbarButtonText", "UnrealSplat"))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
			]
		]
	];
}

FReply SUnrealSplatToolbarButton::OnButtonClicked()
{
	// Invoke the tab - this will spawn it if not open, or focus it if already open
	FGlobalTabmanager::Get()->TryInvokeTab(FUnrealSplatModule::PreprocessorTabName);
	return FReply::Handled();
}

// ============================================================================
// Module Implementation
// ============================================================================

void FUnrealSplatModule::StartupModule()
{
	UE_LOG(LogTemp, Log, TEXT("UnrealSplat: Module starting up"));

	// Register the tab spawner
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		PreprocessorTabName,
		FOnSpawnTab::CreateRaw(this, &FUnrealSplatModule::SpawnPreprocessorTab))
		.SetDisplayName(LOCTEXT("PreprocessorTabTitle", "UnrealSplat Preprocessor"))
		.SetTooltipText(LOCTEXT("PreprocessorTabTooltip", "Convert PLY files to 3DGS textures"))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.MeshPaintMode"));

	// Register toolbar button
	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FUnrealSplatModule::RegisterMenuExtensions));
}

void FUnrealSplatModule::ShutdownModule()
{
	// Unregister the tab spawner
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(PreprocessorTabName);

	// Unregister menu extensions
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	UE_LOG(LogTemp, Log, TEXT("UnrealSplat: Module shut down"));
}

void FUnrealSplatModule::RegisterMenuExtensions()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	// Add to the play toolbar
	UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu(
		"LevelEditor.LevelEditorToolBar.PlayToolBar");

	if (ToolbarMenu)
	{
		FToolMenuSection& ToolbarSection = ToolbarMenu->AddSection("UnrealSplatSection");
		ToolbarSection.AddEntry(FToolMenuEntry::InitWidget(
			TEXT("UnrealSplatButton"),
			SNew(SUnrealSplatToolbarButton),
			LOCTEXT("ToolbarButtonTooltip", "Open UnrealSplat Preprocessor")
		));

		UE_LOG(LogTemp, Log, TEXT("UnrealSplat: Toolbar button registered"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealSplat: Failed to find toolbar menu"));
	}
}

TSharedRef<SDockTab> FUnrealSplatModule::SpawnPreprocessorTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		.Label(LOCTEXT("TabLabel", "UnrealSplat"))
		[
			SNew(SUnrealSplatWindow)
		];
}

#undef LOCTEXT_NAMESPACE
