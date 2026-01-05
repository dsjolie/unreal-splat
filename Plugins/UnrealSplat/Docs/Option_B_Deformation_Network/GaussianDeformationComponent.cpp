// GaussianDeformationComponent.cpp
// Implementation of GPU-based 4D Gaussian deformation

#include "GaussianDeformationComponent.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFilemanager.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

// ============================================================================
// Compute Shader Declaration
// ============================================================================

class FGaussianDeformationCS : public FGlobalShader
{
    DECLARE_GLOBAL_SHADER(FGaussianDeformationCS);
    SHADER_USE_PARAMETER_STRUCT(FGaussianDeformationCS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        // Input buffers
        SHADER_PARAMETER_SRV(StructuredBuffer<float3>, CanonicalPositions)
        SHADER_PARAMETER_SRV(StructuredBuffer<float3>, CanonicalScales)
        SHADER_PARAMETER_SRV(StructuredBuffer<float4>, CanonicalRotations)
        SHADER_PARAMETER_SRV(StructuredBuffer<float>, CanonicalOpacities)

        // Output buffers
        SHADER_PARAMETER_UAV(RWStructuredBuffer<float3>, DeformedPositions)
        SHADER_PARAMETER_UAV(RWStructuredBuffer<float3>, DeformedScales)
        SHADER_PARAMETER_UAV(RWStructuredBuffer<float4>, DeformedRotations)
        SHADER_PARAMETER_UAV(RWStructuredBuffer<float>, DeformedOpacities)

        // HexPlane textures
        SHADER_PARAMETER_TEXTURE(Texture2D, PlaneXY)
        SHADER_PARAMETER_TEXTURE(Texture2D, PlaneXZ)
        SHADER_PARAMETER_TEXTURE(Texture2D, PlaneYZ)
        SHADER_PARAMETER_TEXTURE(Texture2D, PlaneXT)
        SHADER_PARAMETER_TEXTURE(Texture2D, PlaneYT)
        SHADER_PARAMETER_TEXTURE(Texture2D, PlaneZT)
        SHADER_PARAMETER_SAMPLER(SamplerState, PlaneSampler)

        // MLP weights
        SHADER_PARAMETER_TEXTURE(Texture2D, FeatureMLPWeights)
        SHADER_PARAMETER_SRV(StructuredBuffer<float>, FeatureMLPBiases)

        // Uniforms
        SHADER_PARAMETER(float, CurrentTime)
        SHADER_PARAMETER(FVector3f, AABBMin)
        SHADER_PARAMETER(FVector3f, AABBMax)
        SHADER_PARAMETER(int32, NumGaussians)
        SHADER_PARAMETER(int32, FeatureDim)
        SHADER_PARAMETER(int32, MLPWidth)
    END_SHADER_PARAMETER_STRUCT()

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
    {
        return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
    }
};

IMPLEMENT_GLOBAL_SHADER(FGaussianDeformationCS, "/Plugin/UnrealSplat/Private/GaussianDeformation.usf", "DeformGaussiansCS", SF_Compute);

// ============================================================================
// Component Implementation
// ============================================================================

UGaussianDeformationComponent::UGaussianDeformationComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

UGaussianDeformationComponent::~UGaussianDeformationComponent()
{
}

void UGaussianDeformationComponent::BeginPlay()
{
    Super::BeginPlay();

    if (!NetworkDirectory.IsEmpty())
    {
        LoadNetwork(NetworkDirectory);
    }

    if (bAutoPlay && bNetworkLoaded)
    {
        Play();
    }
}

void UGaussianDeformationComponent::BeginDestroy()
{
    // Release GPU resources
    // (RHI resources are ref-counted and will be released automatically)

    Super::BeginDestroy();
}

bool UGaussianDeformationComponent::ParseNetworkConfig(const FString& JsonPath)
{
    FString JsonString;
    if (!FFileHelper::LoadFileToString(JsonString, *JsonPath))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to load network config: %s"), *JsonPath);
        return false;
    }

    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

    if (!FJsonSerializer::Deserialize(Reader, JsonObject))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to parse network config JSON"));
        return false;
    }

    Config.NumGaussians = JsonObject->GetIntegerField(TEXT("num_gaussians"));
    Config.SHDegree = JsonObject->GetIntegerField(TEXT("sh_degree"));

    // Parse AABB
    const TArray<TSharedPtr<FJsonValue>>* AABBArray;
    if (JsonObject->TryGetArrayField(TEXT("aabb"), AABBArray) && AABBArray->Num() >= 2)
    {
        const TArray<TSharedPtr<FJsonValue>>& MinArray = (*AABBArray)[0]->AsArray();
        const TArray<TSharedPtr<FJsonValue>>& MaxArray = (*AABBArray)[1]->AsArray();

        Config.AABBMin = FVector(
            MinArray[0]->AsNumber(),
            MinArray[1]->AsNumber(),
            MinArray[2]->AsNumber()
        );
        Config.AABBMax = FVector(
            MaxArray[0]->AsNumber(),
            MaxArray[1]->AsNumber(),
            MaxArray[2]->AsNumber()
        );
    }

    // Parse network config
    const TSharedPtr<FJsonObject>* NetworkConfig;
    if (JsonObject->TryGetObjectField(TEXT("network_config"), NetworkConfig))
    {
        Config.FeatureDim = (*NetworkConfig)->GetIntegerField(TEXT("feature_dim"));
        Config.MLPWidth = (*NetworkConfig)->GetIntegerField(TEXT("mlp_width"));
        Config.MLPDepth = (*NetworkConfig)->GetIntegerField(TEXT("mlp_depth"));
    }

    UE_LOG(LogTemp, Log, TEXT("Loaded config: %d Gaussians, AABB [%.2f,%.2f,%.2f] to [%.2f,%.2f,%.2f]"),
           Config.NumGaussians,
           Config.AABBMin.X, Config.AABBMin.Y, Config.AABBMin.Z,
           Config.AABBMax.X, Config.AABBMax.Y, Config.AABBMax.Z);

    return true;
}

TArray<float> UGaussianDeformationComponent::LoadRawFloatArray(const FString& FilePath, int32 ExpectedSize)
{
    TArray<uint8> RawData;
    if (!FFileHelper::LoadFileToArray(RawData, *FilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to load raw file: %s"), *FilePath);
        return TArray<float>();
    }

    int32 NumFloats = RawData.Num() / sizeof(float);
    TArray<float> FloatData;
    FloatData.SetNumUninitialized(NumFloats);
    FMemory::Memcpy(FloatData.GetData(), RawData.GetData(), RawData.Num());

    return FloatData;
}

bool UGaussianDeformationComponent::LoadCanonicalGaussians(const FString& Directory)
{
    // Load positions
    TArray<float> Positions = LoadRawFloatArray(
        FPaths::Combine(Directory, TEXT("positions.raw")),
        Config.NumGaussians * 3
    );
    if (Positions.Num() == 0) return false;

    // Load scales
    TArray<float> Scales = LoadRawFloatArray(
        FPaths::Combine(Directory, TEXT("scales.raw")),
        Config.NumGaussians * 3
    );
    if (Scales.Num() == 0) return false;

    // Load rotations
    TArray<float> Rotations = LoadRawFloatArray(
        FPaths::Combine(Directory, TEXT("rotations.raw")),
        Config.NumGaussians * 4
    );
    if (Rotations.Num() == 0) return false;

    // Load opacities
    TArray<float> Opacities = LoadRawFloatArray(
        FPaths::Combine(Directory, TEXT("opacities.raw")),
        Config.NumGaussians
    );
    if (Opacities.Num() == 0) return false;

    // Create GPU buffers
    ENQUEUE_RENDER_COMMAND(CreateGaussianBuffers)(
        [this, Positions, Scales, Rotations, Opacities](FRHICommandListImmediate& RHICmdList)
        {
            // Create position buffer
            FRHIResourceCreateInfo CreateInfo(TEXT("CanonicalPositions"));
            GaussianBuffers.CanonicalPositions = RHICmdList.CreateStructuredBuffer(
                sizeof(FVector3f),
                Config.NumGaussians,
                BUF_ShaderResource | BUF_Static,
                CreateInfo
            );

            // Upload data
            void* Data = RHICmdList.LockBuffer(GaussianBuffers.CanonicalPositions, 0,
                                                Config.NumGaussians * sizeof(FVector3f), RLM_WriteOnly);
            FMemory::Memcpy(Data, Positions.GetData(), Positions.Num() * sizeof(float));
            RHICmdList.UnlockBuffer(GaussianBuffers.CanonicalPositions);

            // Create SRV
            GaussianBuffers.CanonicalPositionsSRV = RHICmdList.CreateShaderResourceView(
                GaussianBuffers.CanonicalPositions
            );

            // Similar for scales, rotations, opacities...
            // (Abbreviated for clarity - same pattern)

            // Create output buffers (UAV)
            FRHIResourceCreateInfo OutputCreateInfo(TEXT("DeformedPositions"));
            GaussianBuffers.DeformedPositions = RHICmdList.CreateStructuredBuffer(
                sizeof(FVector3f),
                Config.NumGaussians,
                BUF_UnorderedAccess | BUF_ShaderResource,
                OutputCreateInfo
            );

            GaussianBuffers.DeformedPositionsUAV = RHICmdList.CreateUnorderedAccessView(
                GaussianBuffers.DeformedPositions, false, false
            );
        }
    );

    return true;
}

bool UGaussianDeformationComponent::LoadNetwork(const FString& Directory)
{
    UE_LOG(LogTemp, Log, TEXT("Loading deformation network from: %s"), *Directory);

    // Parse config
    FString ConfigPath = FPaths::Combine(Directory, TEXT("deformation_network.json"));
    if (!ParseNetworkConfig(ConfigPath))
    {
        return false;
    }

    // Load canonical Gaussians
    if (!LoadCanonicalGaussians(Directory))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to load canonical Gaussians"));
        return false;
    }

    // Load HexPlane grids
    if (!LoadHexPlaneGrids(Directory))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to load HexPlane grids"));
        return false;
    }

    // Load MLP weights
    if (!LoadMLPWeights(Directory))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to load MLP weights"));
        return false;
    }

    bNetworkLoaded = true;
    UE_LOG(LogTemp, Log, TEXT("Deformation network loaded successfully"));

    return true;
}

bool UGaussianDeformationComponent::LoadHexPlaneGrids(const FString& Directory)
{
    // Load each plane from raw files
    // Planes are stored as Resolution x Resolution x FeatureDim

    TArray<FString> PlaneNames = {
        TEXT("plane_xy"), TEXT("plane_xz"), TEXT("plane_yz"),
        TEXT("plane_xt"), TEXT("plane_yt"), TEXT("plane_zt")
    };

    // NOTE: Implementation would read plane dimensions from JSON
    // and create appropriate textures

    return true;
}

bool UGaussianDeformationComponent::LoadMLPWeights(const FString& Directory)
{
    // Load MLP weights from raw files
    // Pack into textures for efficient GPU access

    return true;
}

void UGaussianDeformationComponent::Play()
{
    bIsPlaying = true;
}

void UGaussianDeformationComponent::Pause()
{
    bIsPlaying = false;
}

void UGaussianDeformationComponent::SetTime(float Time)
{
    CurrentTime = FMath::Clamp(Time, 0.0f, 1.0f);
    if (bNetworkLoaded)
    {
        ExecuteDeformation();
    }
}

void UGaussianDeformationComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                                   FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bNetworkLoaded || !bIsPlaying)
    {
        return;
    }

    // Advance time
    CurrentTime += (DeltaTime / Duration) * PlaybackSpeed;

    // Handle looping
    if (CurrentTime >= 1.0f)
    {
        if (bLooping)
        {
            CurrentTime = FMath::Fmod(CurrentTime, 1.0f);
        }
        else
        {
            CurrentTime = 1.0f;
            bIsPlaying = false;
        }
    }

    // Execute deformation
    ExecuteDeformation();
}

void UGaussianDeformationComponent::ExecuteDeformation()
{
    ENQUEUE_RENDER_COMMAND(ExecuteGaussianDeformation)(
        [this](FRHICommandListImmediate& RHICmdList)
        {
            // Get compute shader
            TShaderMapRef<FGaussianDeformationCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));

            // Set parameters
            FGaussianDeformationCS::FParameters Parameters;

            Parameters.CanonicalPositions = GaussianBuffers.CanonicalPositionsSRV;
            Parameters.CanonicalScales = GaussianBuffers.CanonicalScalesSRV;
            Parameters.CanonicalRotations = GaussianBuffers.CanonicalRotationsSRV;
            Parameters.CanonicalOpacities = GaussianBuffers.CanonicalOpacitiesSRV;

            Parameters.DeformedPositions = GaussianBuffers.DeformedPositionsUAV;
            Parameters.DeformedScales = GaussianBuffers.DeformedScalesUAV;
            Parameters.DeformedRotations = GaussianBuffers.DeformedRotationsUAV;
            Parameters.DeformedOpacities = GaussianBuffers.DeformedOpacitiesUAV;

            // HexPlane textures
            Parameters.PlaneXY = HexPlaneTextures.PlaneXY;
            Parameters.PlaneXZ = HexPlaneTextures.PlaneXZ;
            Parameters.PlaneYZ = HexPlaneTextures.PlaneYZ;
            Parameters.PlaneXT = HexPlaneTextures.PlaneXT;
            Parameters.PlaneYT = HexPlaneTextures.PlaneYT;
            Parameters.PlaneZT = HexPlaneTextures.PlaneZT;

            // Uniforms
            Parameters.CurrentTime = CurrentTime;
            Parameters.AABBMin = FVector3f(Config.AABBMin);
            Parameters.AABBMax = FVector3f(Config.AABBMax);
            Parameters.NumGaussians = Config.NumGaussians;
            Parameters.FeatureDim = Config.FeatureDim;
            Parameters.MLPWidth = Config.MLPWidth;

            // Dispatch
            int32 NumGroups = FMath::DivideAndRoundUp(Config.NumGaussians, 64);
            FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, Parameters,
                                          FIntVector(NumGroups, 1, 1));
        }
    );

    // Update Niagara after deformation completes
    UpdateNiagara();
}

void UGaussianDeformationComponent::UpdateNiagara()
{
    if (!NiagaraComponent)
    {
        return;
    }

    // Convert deformed buffers to textures for Niagara
    // This depends on your Niagara setup

    // Option 1: Use Niagara's Data Interface to read structured buffers directly
    // Option 2: Copy to textures and set as parameters

    // Implementation depends on your Niagara emitter setup
}
