// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Docking/SDockTab.h"

// Forward declaration
class SUnrealSplatWindow;

/**
 * Toolbar button widget - opens the preprocessing window
 */
class SUnrealSplatToolbarButton : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SUnrealSplatToolbarButton) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FReply OnButtonClicked();
};

/**
 * UnrealSplat module - registers toolbar button and preprocessing window
 */
class FUnrealSplatModule : public IModuleInterface
{
public:
	// Tab identifier
	static const FName PreprocessorTabName;

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterMenuExtensions();
	TSharedRef<SDockTab> SpawnPreprocessorTab(const FSpawnTabArgs& Args);
};
