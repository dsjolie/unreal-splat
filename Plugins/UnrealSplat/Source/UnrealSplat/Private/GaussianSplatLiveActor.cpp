// GaussianSplatLiveActor.cpp
// Simple 4D Gaussian Splatting - direct texture swapping on Niagara

#include "GaussianSplatLiveActor.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

AGaussianSplatLiveActor::AGaussianSplatLiveActor()
{
    PrimaryActorTick.bCanEverTick = true;
}

void AGaussianSplatLiveActor::BeginPlay()
{
    Super::BeginPlay();

    // Load frames if not already loaded
    if (Frames.Num() == 0 && !ModelName.IsEmpty())
    {
        LoadFrames();
    }

    if (bAutoPlay && Frames.Num() > 0)
    {
        Play();
    }
}

void AGaussianSplatLiveActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!bIsPlaying || Frames.Num() == 0)
    {
        return;
    }

    FrameAccumulator += DeltaTime;
    float FrameDuration = 1.0f / FMath::Max(FrameRate, 1.0f);

    if (FrameAccumulator >= FrameDuration)
    {
        FrameAccumulator -= FrameDuration;

        // Advance frame
        FrameIndex++;
        if (FrameIndex >= Frames.Num())
        {
            if (bLooping)
            {
                FrameIndex = 0;
            }
            else
            {
                FrameIndex = Frames.Num() - 1;
                bIsPlaying = false;
            }
        }

        ApplyCurrentFrame();
    }
}

void AGaussianSplatLiveActor::LoadFrames()
{
    if (ModelName.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("GaussianSplatLive: ModelName is empty!"));
        return;
    }

    Frames.Empty();

    // Helper to load texture
    auto LoadTexture = [](const FString& GamePath, const FString& TextureName) -> UTexture2D*
    {
        FString FullPath = GamePath / TextureName + TEXT(".") + TextureName;
        return LoadObject<UTexture2D>(nullptr, *FullPath);
    };

    // Expected structure: Content/{BasePath}/{ModelName}/frame_XXXXX/texturename.uasset
    FString ContentPath = FPaths::ProjectContentDir() / BasePath / ModelName;
    FString SearchPattern = ContentPath / TEXT("frame_*");

    TArray<FString> FoundFolders;
    IFileManager::Get().FindFiles(FoundFolders, *SearchPattern, false, true);
    FoundFolders.Sort();

    UE_LOG(LogTemp, Log, TEXT("GaussianSplatLive: Scanning %s, found %d frame folders"), *ContentPath, FoundFolders.Num());

    for (const FString& FolderName : FoundFolders)
    {
        FString GamePath = FString::Printf(TEXT("/Game/%s/%s/%s"), *BasePath, *ModelName, *FolderName);

        FGaussianSplatFrame Frame;
        Frame.PositionTexture = LoadTexture(GamePath, TEXT("positiontexture"));
        Frame.ScaleTexture = LoadTexture(GamePath, TEXT("scaletexture"));
        Frame.ColorTexture = LoadTexture(GamePath, TEXT("colortexture"));
        Frame.RotationTexture = LoadTexture(GamePath, TEXT("rotationtexture"));
        Frame.HarmonicsL1Texture = LoadTexture(GamePath, TEXT("harmonicsl1texture"));
        Frame.HarmonicsL2Texture = LoadTexture(GamePath, TEXT("harmonicsl2texture"));
        Frame.HarmonicsL31Texture = LoadTexture(GamePath, TEXT("harmonicsl31texture"));
        Frame.HarmonicsL32Texture = LoadTexture(GamePath, TEXT("harmonicsl32texture"));

        if (Frame.PositionTexture)
        {
            Frames.Add(Frame);
            UE_LOG(LogTemp, Log, TEXT("GaussianSplatLive: Loaded frame %d from %s"), Frames.Num() - 1, *FolderName);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("GaussianSplatLive: No positiontexture in %s"), *GamePath);
        }
    }

    NumFrames = Frames.Num();
    UE_LOG(LogTemp, Log, TEXT("GaussianSplatLive: Loaded %d frames for '%s'"), NumFrames, *ModelName);

    if (Frames.Num() > 0)
    {
        FrameIndex = 0;
        ApplyCurrentFrame();
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("GaussianSplatLive: No frames found! Expected: Content/%s/%s/frame_XXXXX/positiontexture.uasset"), *BasePath, *ModelName);
    }
}

void AGaussianSplatLiveActor::ApplyCurrentFrame()
{
    if (FrameIndex < 0 || FrameIndex >= Frames.Num())
    {
        return;
    }

    ApplyFrameToNiagara(Frames[FrameIndex]);
}

UNiagaraComponent* AGaussianSplatLiveActor::GetNiagaraComponent()
{
    if (Target3DGSActor)
    {
        return Target3DGSActor->FindComponentByClass<UNiagaraComponent>();
    }
    return nullptr;
}

void AGaussianSplatLiveActor::ApplyFrameToNiagara(const FGaussianSplatFrame& Frame)
{
    UNiagaraComponent* NC = GetNiagaraComponent();
    if (!NC)
    {
        UE_LOG(LogTemp, Warning, TEXT("GaussianSplatLive: No Niagara component!"));
        return;
    }

    // Set texture parameters directly on Niagara
    // These names must match the Niagara system's User parameters
    if (Frame.PositionTexture)
        NC->SetVariableTexture(TEXT("User.PositionTexture"), Frame.PositionTexture);

    if (Frame.ScaleTexture)
        NC->SetVariableTexture(TEXT("User.ScaleTexture"), Frame.ScaleTexture);

    if (Frame.ColorTexture)
        NC->SetVariableTexture(TEXT("User.ColorTexture"), Frame.ColorTexture);

    if (Frame.RotationTexture)
        NC->SetVariableTexture(TEXT("User.RotationTexture"), Frame.RotationTexture);

    if (Frame.HarmonicsL1Texture)
        NC->SetVariableTexture(TEXT("User.HarmonicsL1Texture"), Frame.HarmonicsL1Texture);

    if (Frame.HarmonicsL2Texture)
        NC->SetVariableTexture(TEXT("User.HarmonicsL2Texture"), Frame.HarmonicsL2Texture);

    if (Frame.HarmonicsL31Texture)
        NC->SetVariableTexture(TEXT("User.HarmonicsL31Texture"), Frame.HarmonicsL31Texture);

    if (Frame.HarmonicsL32Texture)
        NC->SetVariableTexture(TEXT("User.HarmonicsL32Texture"), Frame.HarmonicsL32Texture);
}

void AGaussianSplatLiveActor::Play()
{
    bIsPlaying = true;
    FrameAccumulator = 0.0f;
}

void AGaussianSplatLiveActor::Pause()
{
    bIsPlaying = false;
}

void AGaussianSplatLiveActor::Stop()
{
    bIsPlaying = false;
    FrameIndex = 0;
    FrameAccumulator = 0.0f;

    if (Frames.Num() > 0)
    {
        ApplyCurrentFrame();
    }
}

void AGaussianSplatLiveActor::SetFrame(int32 NewFrameIndex)
{
    if (NewFrameIndex >= 0 && NewFrameIndex < Frames.Num())
    {
        FrameIndex = NewFrameIndex;
        ApplyCurrentFrame();
    }
}
