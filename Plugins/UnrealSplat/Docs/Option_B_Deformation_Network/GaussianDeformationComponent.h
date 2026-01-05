// GaussianDeformationComponent.h
// True 4D Gaussian Splatting with GPU-evaluated deformation network
// Option B: Port deformation network to compute shader

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RHI.h"
#include "RHIResources.h"
#include "RenderGraphResources.h"
#include "NiagaraComponent.h"
#include "GaussianDeformationComponent.generated.h"

/**
 * Deformation network configuration loaded from JSON
 */
USTRUCT(BlueprintType)
struct FDeformationNetworkConfig
{
    GENERATED_BODY()

    int32 NumGaussians = 0;
    int32 FeatureDim = 64;
    int32 MLPWidth = 256;
    int32 MLPDepth = 8;
    int32 SHDegree = 3;

    FVector AABBMin;
    FVector AABBMax;
};

/**
 * GPU buffers for Gaussian data
 */
struct FGaussianGPUBuffers
{
    // Canonical (base) Gaussians
    FBufferRHIRef CanonicalPositions;
    FBufferRHIRef CanonicalScales;
    FBufferRHIRef CanonicalRotations;
    FBufferRHIRef CanonicalOpacities;
    FBufferRHIRef CanonicalSHDC;
    FBufferRHIRef CanonicalSHRest;

    // Deformed Gaussians (output)
    FBufferRHIRef DeformedPositions;
    FBufferRHIRef DeformedScales;
    FBufferRHIRef DeformedRotations;
    FBufferRHIRef DeformedOpacities;

    // UAVs for compute shader output
    FUnorderedAccessViewRHIRef DeformedPositionsUAV;
    FUnorderedAccessViewRHIRef DeformedScalesUAV;
    FUnorderedAccessViewRHIRef DeformedRotationsUAV;
    FUnorderedAccessViewRHIRef DeformedOpacitiesUAV;

    // SRVs for compute shader input
    FShaderResourceViewRHIRef CanonicalPositionsSRV;
    FShaderResourceViewRHIRef CanonicalScalesSRV;
    FShaderResourceViewRHIRef CanonicalRotationsSRV;
    FShaderResourceViewRHIRef CanonicalOpacitiesSRV;
};

/**
 * HexPlane feature grid textures
 */
struct FHexPlaneTextures
{
    // 6 feature planes
    FTexture2DRHIRef PlaneXY;
    FTexture2DRHIRef PlaneXZ;
    FTexture2DRHIRef PlaneYZ;
    FTexture2DRHIRef PlaneXT;
    FTexture2DRHIRef PlaneYT;
    FTexture2DRHIRef PlaneZT;

    // SRVs
    FShaderResourceViewRHIRef PlaneXYSRV;
    FShaderResourceViewRHIRef PlaneXZSRV;
    FShaderResourceViewRHIRef PlaneYZSRV;
    FShaderResourceViewRHIRef PlaneXTSRV;
    FShaderResourceViewRHIRef PlaneYTSRV;
    FShaderResourceViewRHIRef PlaneZTSRV;
};

/**
 * MLP weight textures
 */
struct FMLPWeights
{
    // Feature extraction MLP
    TArray<FTexture2DRHIRef> FeatureMLPWeights;
    TArray<FBufferRHIRef> FeatureMLPBiases;

    // Deformation head weights
    FTexture2DRHIRef PosDeformWeights;
    FTexture2DRHIRef ScaleDeformWeights;
    FTexture2DRHIRef RotDeformWeights;
    FTexture2DRHIRef OpacityDeformWeights;

    FBufferRHIRef PosDeformBiases;
    FBufferRHIRef ScaleDeformBiases;
    FBufferRHIRef RotDeformBiases;
    FBufferRHIRef OpacityDeformBiases;
};

/**
 * Component for real-time 4D Gaussian deformation
 */
UCLASS(ClassGroup=(GaussianSplatting), meta=(BlueprintSpawnableComponent))
class UNREALSPLAT_API UGaussianDeformationComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UGaussianDeformationComponent();
    virtual ~UGaussianDeformationComponent();

    // ---- Properties ----

    /** Directory containing exported deformation network */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4D Gaussian Splatting")
    FString NetworkDirectory;

    /** Current time (0-1 normalized) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4D Gaussian Splatting", meta=(ClampMin=0, ClampMax=1))
    float CurrentTime = 0.0f;

    /** Playback speed */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4D Gaussian Splatting")
    float PlaybackSpeed = 1.0f;

    /** Auto-play animation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4D Gaussian Splatting")
    bool bAutoPlay = true;

    /** Loop animation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4D Gaussian Splatting")
    bool bLooping = true;

    /** Animation duration in seconds */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4D Gaussian Splatting")
    float Duration = 1.0f;

    /** Reference to Niagara component */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4D Gaussian Splatting")
    UNiagaraComponent* NiagaraComponent;

    // ---- Blueprint Functions ----

    /** Load deformation network from directory */
    UFUNCTION(BlueprintCallable, Category = "4D Gaussian Splatting")
    bool LoadNetwork(const FString& Directory);

    /** Set current time and update deformation */
    UFUNCTION(BlueprintCallable, Category = "4D Gaussian Splatting")
    void SetTime(float Time);

    /** Play animation */
    UFUNCTION(BlueprintCallable, Category = "4D Gaussian Splatting")
    void Play();

    /** Pause animation */
    UFUNCTION(BlueprintCallable, Category = "4D Gaussian Splatting")
    void Pause();

    /** Get number of Gaussians */
    UFUNCTION(BlueprintPure, Category = "4D Gaussian Splatting")
    int32 GetNumGaussians() const { return Config.NumGaussians; }

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void BeginDestroy() override;

private:
    // Network configuration
    FDeformationNetworkConfig Config;

    // GPU resources
    FGaussianGPUBuffers GaussianBuffers;
    FHexPlaneTextures HexPlaneTextures;
    FMLPWeights MLPWeights;

    // Playback state
    bool bIsPlaying = false;
    bool bNetworkLoaded = false;

    // Initialize GPU buffers
    void InitializeGPUBuffers();

    // Load data files
    bool LoadCanonicalGaussians(const FString& Directory);
    bool LoadHexPlaneGrids(const FString& Directory);
    bool LoadMLPWeights(const FString& Directory);
    bool ParseNetworkConfig(const FString& JsonPath);

    // Execute deformation compute shader
    void ExecuteDeformation();

    // Update Niagara with deformed Gaussians
    void UpdateNiagara();

    // Helper to load raw float array from file
    TArray<float> LoadRawFloatArray(const FString& FilePath, int32 ExpectedSize);

    // Helper to create texture from raw data
    FTexture2DRHIRef CreateTextureFromRawData(const TArray<float>& Data, int32 Width, int32 Height, int32 Channels);

    // Helper to create buffer from raw data
    FBufferRHIRef CreateBufferFromRawData(const TArray<float>& Data);
};
