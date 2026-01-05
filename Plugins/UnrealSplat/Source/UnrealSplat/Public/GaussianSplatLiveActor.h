// GaussianSplatLiveActor.h
// Simple 4D Gaussian Splatting actor - loads frames from ModelName/frame_XXXXX/ structure
// Sets textures directly on Niagara (requires Niagara system that samples on Update, not just Init)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NiagaraComponent.h"
#include "Engine/Texture2D.h"
#include "GaussianSplatLiveActor.generated.h"

/**
 * Frame data - holds texture references for one frame
 */
USTRUCT(BlueprintType)
struct FGaussianSplatFrame
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UTexture2D* PositionTexture = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UTexture2D* ScaleTexture = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UTexture2D* ColorTexture = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UTexture2D* RotationTexture = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UTexture2D* HarmonicsL1Texture = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UTexture2D* HarmonicsL2Texture = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UTexture2D* HarmonicsL31Texture = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UTexture2D* HarmonicsL32Texture = nullptr;
};

/**
 * Live 4D Gaussian Splatting actor
 * Loads frames from Content/{BasePath}/{ModelName}/frame_XXXXX/ structure
 * Directly sets textures on Niagara each frame
 */
UCLASS(BlueprintType, Blueprintable)
class UNREALSPLAT_API AGaussianSplatLiveActor : public AActor
{
    GENERATED_BODY()

public:
    AGaussianSplatLiveActor();

    // ========== Setup ==========

    /** Base path under Content/ (default: "Splats") */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4DGS|Setup")
    FString BasePath = TEXT("Splats");

    /** Model name - e.g. "bouncingballs" loads from Content/{BasePath}/bouncingballs/frame_XXXXX/ */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4DGS|Setup")
    FString ModelName;

    /** Reference to a 3DGS actor with Niagara component */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4DGS|Setup")
    AActor* Target3DGSActor;

    // ========== Playback ==========

    /** Current frame index (visible and editable) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4DGS|Playback")
    int32 FrameIndex = 0;

    /** Playback frame rate (frames per second) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4DGS|Playback")
    float FrameRate = 30.0f;

    /** Loop playback */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4DGS|Playback")
    bool bLooping = true;

    /** Auto-play on begin */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4DGS|Playback")
    bool bAutoPlay = true;

    /** Is currently playing */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "4DGS|Playback")
    bool bIsPlaying = false;

    // ========== Debug Info ==========

    /** Number of frames loaded */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "4DGS|Debug")
    int32 NumFrames = 0;

    /** Loaded frames (visible for debugging) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "4DGS|Debug")
    TArray<FGaussianSplatFrame> Frames;

    // ========== Editor Buttons ==========

    /** Scan for frames and load textures */
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "4DGS|Setup")
    void LoadFrames();

    /** Apply current FrameIndex to Niagara */
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "4DGS|Setup")
    void ApplyCurrentFrame();

    // ========== Runtime Control ==========

    UFUNCTION(BlueprintCallable, Category = "4DGS|Playback")
    void Play();

    UFUNCTION(BlueprintCallable, Category = "4DGS|Playback")
    void Pause();

    UFUNCTION(BlueprintCallable, Category = "4DGS|Playback")
    void Stop();

    UFUNCTION(BlueprintCallable, Category = "4DGS|Playback")
    void SetFrame(int32 NewFrameIndex);

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

private:
    float FrameAccumulator = 0.0f;

    UNiagaraComponent* GetNiagaraComponent();
    void ApplyFrameToNiagara(const FGaussianSplatFrame& Frame);
};
