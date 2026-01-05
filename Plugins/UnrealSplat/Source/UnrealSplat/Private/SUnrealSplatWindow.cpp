// SUnrealSplatWindow.cpp

#include "SUnrealSplatWindow.h"
#include "Parser.h"
#include "DesktopPlatformModule.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/FileManager.h"

#define LOCTEXT_NAMESPACE "UnrealSplatWindow"

void SUnrealSplatWindow::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SVerticalBox)

		// === Header ===
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(10)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("WindowTitle", "UnrealSplat - 3DGS Preprocessor"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(10, 0)
		[
			SNew(SSeparator)
		]

		// === Input Section ===
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(10)
		[
			SNew(SVerticalBox)

			// Base Path
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 5)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 10, 0)
				[
					SNew(SBox)
					.WidthOverride(100)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("BasePath", "Base Path:"))
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SAssignNew(BasePathInput, SEditableTextBox)
					.Text(FText::FromString(TEXT("Splats")))
					.HintText(LOCTEXT("BasePathHint", "Folder under Content/ (default: Splats)"))
				]
			]

			// File/Folder Path
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 5)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 10, 0)
				[
					SNew(SBox)
					.WidthOverride(100)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("FilePath", "PLY File:"))
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SAssignNew(FilePathInput, SEditableTextBox)
					.HintText(LOCTEXT("FilePathHint", "model.ply or folder with *.ply files"))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(5, 0, 0, 0)
				[
					SNew(SButton)
					.Text(LOCTEXT("Browse", "Browse..."))
					.OnClicked(this, &SUnrealSplatWindow::OnBrowseClicked)
				]
			]

			// Model Name
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 5)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 10, 0)
				[
					SNew(SBox)
					.WidthOverride(100)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ModelName", "Model Name:"))
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SAssignNew(ModelNameInput, SEditableTextBox)
					.HintText(LOCTEXT("ModelNameHint", "Output folder name (auto-filled from filename)"))
				]
			]

			// Sequence Mode Checkbox
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 5)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 10, 0)
				[
					SNew(SBox)
					.WidthOverride(100)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Mode", "Mode:"))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SAssignNew(SequenceModeCheckbox, SCheckBox)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(5, 0, 0, 0)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("SequenceMode", "Sequence Mode (process folder of PLY files as frames)"))
				]
			]
		]

		// === Buttons ===
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(10, 5)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("Preprocess", "Preprocess"))
				.OnClicked(this, &SUnrealSplatWindow::OnPreprocessClicked)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(10, 0, 0, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("ClearLog", "Clear Log"))
				.OnClicked(this, &SUnrealSplatWindow::OnClearLogClicked)
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(10, 5, 10, 0)
		[
			SNew(SSeparator)
		]

		// === Output Log ===
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(10, 5)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("OutputLabel", "Output:"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(10, 0, 10, 10)
		[
			SNew(SBox)
			.MinDesiredHeight(200)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						SAssignNew(OutputLog, SMultiLineEditableText)
						.IsReadOnly(true)
						.AutoWrapText(true)
					]
				]
			]
		]
	];

	// Initial log message
	AppendLog(TEXT("UnrealSplat Preprocessor Ready"));
	AppendLog(TEXT("---"));
	AppendLog(TEXT("Place PLY files in Content/Splats/ folder"));
	AppendLog(TEXT("Enter relative path (e.g., 'mymodel.ply' or 'sequence_folder')"));
}

FReply SUnrealSplatWindow::OnBrowseClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		return FReply::Handled();
	}

	FString BasePath = BasePathInput->GetText().ToString();
	FString StartDirectory = FPaths::ProjectContentDir() / BasePath;

	// Check if sequence mode - browse for folder
	bool bSequenceMode = SequenceModeCheckbox->IsChecked();

	if (bSequenceMode)
	{
		FString SelectedFolder;
		if (DesktopPlatform->OpenDirectoryDialog(
			FSlateApplication::Get().GetActiveTopLevelWindow()->GetNativeWindow()->GetOSWindowHandle(),
			TEXT("Select folder with PLY sequence"),
			StartDirectory,
			SelectedFolder))
		{
			// Normalize paths for comparison
			FString ContentBasePath = FPaths::ProjectContentDir() / BasePath;
			FPaths::NormalizeDirectoryName(ContentBasePath);
			FPaths::NormalizeDirectoryName(SelectedFolder);

			// Make relative to Content/BasePath/ (UI will add BasePath back)
			FString RelativePath = SelectedFolder;
			if (RelativePath.StartsWith(ContentBasePath))
			{
				RelativePath = RelativePath.RightChop(ContentBasePath.Len());
				// Remove leading slash
				while (RelativePath.StartsWith(TEXT("/")) || RelativePath.StartsWith(TEXT("\\")))
				{
					RelativePath = RelativePath.RightChop(1);
				}
			}
			else
			{
				// Fallback: just use folder name
				RelativePath = FPaths::GetBaseFilename(SelectedFolder);
			}
			FilePathInput->SetText(FText::FromString(RelativePath));

			// Auto-fill model name
			FString FolderName = FPaths::GetBaseFilename(SelectedFolder);
			ModelNameInput->SetText(FText::FromString(FolderName));
		}
	}
	else
	{
		TArray<FString> OutFiles;
		if (DesktopPlatform->OpenFileDialog(
			FSlateApplication::Get().GetActiveTopLevelWindow()->GetNativeWindow()->GetOSWindowHandle(),
			TEXT("Select PLY file"),
			StartDirectory,
			TEXT(""),
			TEXT("PLY Files (*.ply)|*.ply"),
			EFileDialogFlags::None,
			OutFiles))
		{
			if (OutFiles.Num() > 0)
			{
				// Normalize paths for comparison
				FString ContentBasePath = FPaths::ProjectContentDir() / BasePath;
				FPaths::NormalizeDirectoryName(ContentBasePath);
				FString SelectedFile = OutFiles[0];
				FPaths::NormalizeFilename(SelectedFile);

				// Make relative to Content/BasePath/ (UI will add BasePath back)
				FString RelativePath = SelectedFile;
				if (RelativePath.StartsWith(ContentBasePath))
				{
					RelativePath = RelativePath.RightChop(ContentBasePath.Len());
					while (RelativePath.StartsWith(TEXT("/")) || RelativePath.StartsWith(TEXT("\\")))
					{
						RelativePath = RelativePath.RightChop(1);
					}
				}
				else
				{
					// Fallback: just use filename
					RelativePath = FPaths::GetCleanFilename(SelectedFile);
				}
				FilePathInput->SetText(FText::FromString(RelativePath));

				// Auto-fill model name from filename
				ModelNameInput->SetText(FText::FromString(GetDefaultModelName(RelativePath)));
			}
		}
	}

	return FReply::Handled();
}

FReply SUnrealSplatWindow::OnPreprocessClicked()
{
	FString FilePath = FilePathInput->GetText().ToString();
	FString ModelName = ModelNameInput->GetText().ToString();
	FString BasePath = BasePathInput->GetText().ToString();
	bool bSequenceMode = SequenceModeCheckbox->IsChecked();

	if (FilePath.IsEmpty())
	{
		AppendLog(TEXT("ERROR: Please enter a file or folder path"));
		return FReply::Handled();
	}

	if (ModelName.IsEmpty())
	{
		ModelName = GetDefaultModelName(FilePath);
		ModelNameInput->SetText(FText::FromString(ModelName));
	}

	// Build full path relative to Content/
	FString FullPath = BasePath / FilePath;

	AppendLog(TEXT("---"));
	AppendLog(FString::Printf(TEXT("Starting preprocessing...")));
	AppendLog(FString::Printf(TEXT("  Full Path: Content/%s"), *FullPath));
	AppendLog(FString::Printf(TEXT("  Model Name: %s"), *ModelName));
	AppendLog(FString::Printf(TEXT("  Mode: %s"), bSequenceMode ? TEXT("Sequence") : TEXT("Single")));

	bool bSuccess = false;
	FString OutputString;
	int32 Result = 0;

	if (bSequenceMode)
	{
		// Count PLY files first for progress bar
		FString SourcePath = FPaths::ProjectContentDir() / FullPath;
		TArray<FString> PlyFiles;
		IFileManager::Get().FindFiles(PlyFiles, *(SourcePath / TEXT("*.ply")), true, false);
		int32 NumFiles = PlyFiles.Num();

		if (NumFiles == 0)
		{
			AppendLog(FString::Printf(TEXT("ERROR: No PLY files found in %s"), *SourcePath));
			return FReply::Handled();
		}

		AppendLog(FString::Printf(TEXT("Found %d PLY files"), NumFiles));

		// Show progress dialog
		FScopedSlowTask SlowTask(NumFiles, FText::FromString(FString::Printf(TEXT("Processing %d frames..."), NumFiles)));
		SlowTask.MakeDialog(true);

		// Process each file manually with progress updates
		PlyFiles.Sort();
		FString OutputBasePath = FPaths::ProjectContentDir() / FPaths::GetPath(FullPath) / ModelName;
		IFileManager::Get().MakeDirectory(*OutputBasePath, true);

		int32 FramesProcessed = 0;
		for (int32 i = 0; i < NumFiles; i++)
		{
			if (SlowTask.ShouldCancel())
			{
				AppendLog(TEXT("Cancelled by user"));
				break;
			}

			SlowTask.EnterProgressFrame(1, FText::FromString(FString::Printf(TEXT("Processing frame %d/%d: %s"), i + 1, NumFiles, *PlyFiles[i])));

			// Build path for this frame
			FString FramePlyPath = FullPath / PlyFiles[i];

			bool bFrameSuccess = false;
			FString FrameOutput;
			TArray<FTextureLocations> FrameLocations;

			int32 NumVerts = UParser::Preprocess3DGSModel(FramePlyPath, bFrameSuccess, FrameOutput, FrameLocations);

			if (bFrameSuccess && NumVerts > 0)
			{
				FramesProcessed++;
			}
			else
			{
				AppendLog(FString::Printf(TEXT("  Frame %d failed: %s"), i, *FrameOutput));
			}
		}

		Result = FramesProcessed;
		bSuccess = FramesProcessed > 0;
		AppendLog(FString::Printf(TEXT("Frames processed: %d/%d"), FramesProcessed, NumFiles));
	}
	else
	{
		// Single file - simple progress
		FScopedSlowTask SlowTask(1, LOCTEXT("ProcessingSingle", "Processing PLY file..."));
		SlowTask.MakeDialog(true);
		SlowTask.EnterProgressFrame(1);

		TArray<FTextureLocations> TexLocations;
		Result = UParser::Preprocess3DGSModel(FullPath, bSuccess, OutputString, TexLocations);
		AppendLog(FString::Printf(TEXT("Vertices processed: %d"), Result));
	}

	if (bSuccess)
	{
		AppendLog(TEXT("SUCCESS!"));
		AppendLog(FString::Printf(TEXT("Output: Content/%s/%s/"), *BasePath, *ModelName));
	}
	else
	{
		AppendLog(TEXT("FAILED!"));
		if (!OutputString.IsEmpty())
		{
			AppendLog(OutputString);
		}
	}

	return FReply::Handled();
}

FReply SUnrealSplatWindow::OnClearLogClicked()
{
	OutputLog->SetText(FText::GetEmpty());
	AppendLog(TEXT("Log cleared"));
	return FReply::Handled();
}

void SUnrealSplatWindow::AppendLog(const FString& Message)
{
	FString CurrentText = OutputLog->GetText().ToString();
	if (!CurrentText.IsEmpty())
	{
		CurrentText += TEXT("\n");
	}
	CurrentText += Message;
	OutputLog->SetText(FText::FromString(CurrentText));
}

FString SUnrealSplatWindow::GetDefaultModelName(const FString& FilePath)
{
	// Extract base filename without extension
	return FPaths::GetBaseFilename(FilePath);
}

#undef LOCTEXT_NAMESPACE
