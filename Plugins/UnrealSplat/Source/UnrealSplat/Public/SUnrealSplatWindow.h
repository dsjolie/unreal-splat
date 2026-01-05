// SUnrealSplatWindow.h
// Pure Slate preprocessing UI for UnrealSplat

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Text/SMultiLineEditableText.h"

/**
 * Slate window for 3DGS/4DGS preprocessing
 * Converts PLY files to texture assets
 */
class SUnrealSplatWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SUnrealSplatWindow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	// UI Elements
	TSharedPtr<SEditableTextBox> FilePathInput;
	TSharedPtr<SEditableTextBox> ModelNameInput;
	TSharedPtr<SEditableTextBox> BasePathInput;
	TSharedPtr<SCheckBox> SequenceModeCheckbox;
	TSharedPtr<SMultiLineEditableText> OutputLog;

	// Button handlers
	FReply OnBrowseClicked();
	FReply OnPreprocessClicked();
	FReply OnClearLogClicked();

	// Helper
	void AppendLog(const FString& Message);
	FString GetDefaultModelName(const FString& FilePath);
};
