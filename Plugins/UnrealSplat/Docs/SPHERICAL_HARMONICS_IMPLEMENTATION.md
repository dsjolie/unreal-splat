# Spherical Harmonics Implementation Guide

Implementation plan for view-dependent color rendering in UnrealSplat.

---

## 1. Background: What SH Does in 3DGS

3D Gaussian Splatting stores **view-dependent appearance** using spherical harmonics. Each splat has coefficients that encode how its color changes based on viewing angle.

### Degree Breakdown

| Degree | Coefficients | Effect |
|--------|-------------|--------|
| L0 (DC) | 3 (RGB) | Base color, view-independent |
| L1 | 9 (3x3) | Linear variation (diffuse shading) |
| L2 | 15 (5x3) | Quadratic (soft specular) |
| L3 | 21 (7x3) | Cubic (sharper specular) |
| **Total** | **48** | Full view-dependent appearance |

Without SH evaluation, splats render with flat L0 color only - no specular highlights, no view-dependent effects.

---

## 2. Current State in UnrealSplat

### What's Implemented

1. **PLY Parsing** - All 48 coefficients extracted (`Parser.cpp:400-428`)
2. **Texture Storage** - Coefficients packed into RGBA32F textures
3. **Niagara Binding** - Textures passed as user parameters

### What's Missing

- View direction calculation per splat
- SH basis function evaluation
- Color reconstruction from coefficients
- Integration into Niagara rendering

### Current Texture Layout

```
ColorTexture:       [L0.r, L0.g, L0.b, opacity] - 1 pixel per splat
HarmonicsL1Texture: [f_rest_0..2], [f_rest_3..5], [f_rest_6..8] - 3 pixels per splat
HarmonicsL2Texture: [f_rest_9..11], ..., [f_rest_21..23] - 5 pixels per splat
HarmonicsL31Texture: [f_rest_24..26], ..., [f_rest_33..35] - 4 pixels per splat
HarmonicsL32Texture: [f_rest_36..38], ..., [f_rest_42..44] - 3 pixels per splat
```

Note: L1/L2/L3 textures have different dimensions than Position/Color/Scale/Rotation due to multiple pixels per splat.

---

## 3. SH Basis Functions (Mathematical Reference)

The spherical harmonic basis functions for a direction vector `d = (x, y, z)`:

```hlsl
// Constants
#define SH_C0  0.28209479177387814f      // 1/(2*sqrt(pi))
#define SH_C1  0.4886025119029199f       // sqrt(3/(4*pi))
#define SH_C2_0 1.0925484305920792f      // sqrt(15/pi)
#define SH_C2_1 0.31539156525252005f     // sqrt(5/(16*pi))
#define SH_C2_2 0.5462742152960396f      // sqrt(15/(16*pi))
#define SH_C3_0 0.5900435899266435f
#define SH_C3_1 2.890611442640554f
#define SH_C3_2 0.4570457994644658f
#define SH_C3_3 0.3731763325901154f

// L0 (1 basis function)
float Y0 = SH_C0;

// L1 (3 basis functions)
float Y1 = SH_C1 * y;
float Y2 = SH_C1 * z;
float Y3 = SH_C1 * x;

// L2 (5 basis functions)
float Y4 = SH_C2_0 * x * y;
float Y5 = SH_C2_0 * y * z;
float Y6 = SH_C2_1 * (3.0 * z * z - 1.0);
float Y7 = SH_C2_0 * x * z;
float Y8 = SH_C2_2 * (x * x - y * y);

// L3 (7 basis functions)
float Y9  = SH_C3_0 * y * (3.0 * x * x - y * y);
float Y10 = SH_C3_1 * x * y * z;
float Y11 = SH_C3_2 * y * (5.0 * z * z - 1.0);
float Y12 = SH_C3_3 * z * (5.0 * z * z - 3.0);
float Y13 = SH_C3_2 * x * (5.0 * z * z - 1.0);
float Y14 = SH_C3_1 * z * (x * x - y * y);
float Y15 = SH_C3_0 * x * (x * x - 3.0 * y * y);
```

---

## 4. Implementation Approaches

### Option A: Niagara Scratch Pad Module (Recommended)

Create a custom Niagara module that evaluates SH per-particle.

**Pros:**
- Stays within existing architecture
- Hot-reloadable in editor
- No C++ shader code

**Cons:**
- Performance overhead from Niagara graph
- Complex texture sampling logic

### Option B: Custom Material Function

Create an HLSL material function called from the splat material.

**Pros:**
- Direct HLSL, optimal performance
- Reusable across materials

**Cons:**
- Requires material graph modification
- Less flexible than Niagara approach

### Option C: Compute Shader Pre-pass

Evaluate SH on GPU each frame, write results back to color texture.

**Pros:**
- Best performance
- Decoupled from rendering

**Cons:**
- Most complex implementation
- Requires texture write-back

---

## 5. Recommended Implementation: Niagara Module

### Step 1: Create SH Evaluation Module

Create `NS_EvaluateSH` Scratch Pad module with inputs:

```
Inputs:
- float3 ViewDirection      // Camera to splat, normalized
- float3 L0Coeffs           // From ColorTexture.rgb
- Texture2D HarmonicsL1     // L1 texture
- Texture2D HarmonicsL2     // L2 texture
- Texture2D HarmonicsL31    // L3 part 1
- Texture2D HarmonicsL32    // L3 part 2
- int ParticleIndex         // For texture coordinate calculation
- int TotalParticles        // For texture size calculation
- int ShDegree              // 0-3, which degrees to evaluate

Outputs:
- float3 FinalColor         // View-dependent RGB
```

### Step 2: HLSL Implementation (Scratch Pad)

```hlsl
// Input parameters bound by Niagara
float3 ViewDir;      // Normalized view direction
float3 L0;           // DC coefficients from ColorTexture
int ParticleIdx;
int ShDegree;

// Texture samplers
Texture2D L1Tex;
Texture2D L2Tex;
Texture2D L31Tex;
Texture2D L32Tex;
SamplerState TexSampler;

// Calculate texture coordinates for this particle
// L1: 3 pixels per particle
// L2: 5 pixels per particle
// L31: 4 pixels per particle
// L32: 3 pixels per particle

float2 GetTexCoord(int pixelIndex, int texWidth, int texHeight)
{
    int x = pixelIndex % texWidth;
    int y = pixelIndex / texWidth;
    return float2((x + 0.5) / texWidth, (y + 0.5) / texHeight);
}

// Start with DC term
float3 Result = SH_C0 * L0 + 0.5;

if (ShDegree >= 1)
{
    // Sample L1 coefficients (3 pixels)
    int basePixel = ParticleIdx * 3;
    float3 L1_0 = L1Tex.SampleLevel(TexSampler, GetTexCoord(basePixel + 0, L1Width, L1Height), 0).rgb;
    float3 L1_1 = L1Tex.SampleLevel(TexSampler, GetTexCoord(basePixel + 1, L1Width, L1Height), 0).rgb;
    float3 L1_2 = L1Tex.SampleLevel(TexSampler, GetTexCoord(basePixel + 2, L1Width, L1Height), 0).rgb;

    // Unpack: each pixel has 3 floats, total 9 coefficients = 3 per RGB channel
    // Layout: [R0,R1,R2], [G0,G1,G2], [B0,B1,B2] packed as [c0,c1,c2] x 3 pixels
    // Actually: [f_rest_0, f_rest_1, f_rest_2], [f_rest_3, f_rest_4, f_rest_5], ...

    float x = ViewDir.x;
    float y = ViewDir.y;
    float z = ViewDir.z;

    // L1 basis: y, z, x (standard real SH ordering)
    float Y1 = SH_C1 * y;
    float Y2 = SH_C1 * z;
    float Y3 = SH_C1 * x;

    // Each f_rest coefficient applies to one basis function for one color channel
    // f_rest_0..2 = L1 for R, f_rest_3..5 = L1 for G, f_rest_6..8 = L1 for B
    Result.r += L1_0.r * Y1 + L1_0.g * Y2 + L1_0.b * Y3;
    Result.g += L1_1.r * Y1 + L1_1.g * Y2 + L1_1.b * Y3;
    Result.b += L1_2.r * Y1 + L1_2.g * Y2 + L1_2.b * Y3;
}

if (ShDegree >= 2)
{
    // Sample L2 coefficients (5 pixels)
    int basePixel = ParticleIdx * 5;
    float3 L2_0 = L2Tex.SampleLevel(TexSampler, GetTexCoord(basePixel + 0, L2Width, L2Height), 0).rgb;
    float3 L2_1 = L2Tex.SampleLevel(TexSampler, GetTexCoord(basePixel + 1, L2Width, L2Height), 0).rgb;
    float3 L2_2 = L2Tex.SampleLevel(TexSampler, GetTexCoord(basePixel + 2, L2Width, L2Height), 0).rgb;
    float3 L2_3 = L2Tex.SampleLevel(TexSampler, GetTexCoord(basePixel + 3, L2Width, L2Height), 0).rgb;
    float3 L2_4 = L2Tex.SampleLevel(TexSampler, GetTexCoord(basePixel + 4, L2Width, L2Height), 0).rgb;

    float x = ViewDir.x, y = ViewDir.y, z = ViewDir.z;
    float xx = x*x, yy = y*y, zz = z*z;

    // L2 basis functions
    float Y4 = SH_C2_0 * x * y;
    float Y5 = SH_C2_0 * y * z;
    float Y6 = SH_C2_1 * (3.0 * zz - 1.0);
    float Y7 = SH_C2_0 * x * z;
    float Y8 = SH_C2_2 * (xx - yy);

    // Apply L2 coefficients
    // 15 coefficients = 5 per color channel
    Result.r += L2_0.r * Y4 + L2_0.g * Y5 + L2_0.b * Y6 + L2_1.r * Y7 + L2_1.g * Y8;
    Result.g += L2_1.b * Y4 + L2_2.r * Y5 + L2_2.g * Y6 + L2_2.b * Y7 + L2_3.r * Y8;
    Result.b += L2_3.g * Y4 + L2_3.b * Y5 + L2_4.r * Y6 + L2_4.g * Y7 + L2_4.b * Y8;
}

if (ShDegree >= 3)
{
    // Sample L3 coefficients (4 + 3 = 7 pixels)
    // ... similar pattern for L3 basis functions
}

// Clamp to valid range
FinalColor = saturate(Result);
```

### Step 3: Integrate into Niagara System

1. Open `GaussianSplatRendererTextureBased` Niagara asset
2. In Particle Update, add the SH evaluation module
3. Wire view direction from camera position - particle position
4. Output to particle color attribute

### Step 4: Pass Camera Position to Niagara

In `GaussianSplatLiveActor.cpp`, add:

```cpp
void AGaussianSplatLiveActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (NiagaraComponent && GetWorld())
    {
        APlayerCameraManager* CamMgr = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0);
        if (CamMgr)
        {
            FVector CamPos = CamMgr->GetCameraLocation();
            NiagaraComponent->SetVariableVec3(TEXT("User.CameraPosition"), CamPos);
        }
    }
}
```

---

## 6. Texture Coordinate Math

### Mapping Particle Index to Texture UV

The harmonics textures have **multiple pixels per splat**:

```
L1: 3 pixels/splat  -> UV for splat i, coeff j: pixel = i*3 + j
L2: 5 pixels/splat  -> UV for splat i, coeff j: pixel = i*5 + j
L31: 4 pixels/splat -> UV for splat i, coeff j: pixel = i*4 + j
L32: 3 pixels/splat -> UV for splat i, coeff j: pixel = i*3 + j
```

Texture dimensions are computed as:

```
numPixels = numSplats * pixelsPerSplat
width = ceil(sqrt(numPixels))
height = ceil(numPixels / width)
```

To sample:

```hlsl
float2 PixelToUV(int pixelIndex, int width, int height)
{
    int x = pixelIndex % width;
    int y = pixelIndex / width;
    // +0.5 to sample pixel center
    return float2((x + 0.5) / width, (y + 0.5) / height);
}
```

---

## 7. Coefficient Ordering (Important!)

The PLY stores coefficients in a specific order that must match the SH basis evaluation:

```
f_rest_0..8   = L1 coefficients (9 total: 3 per RGB)
f_rest_9..23  = L2 coefficients (15 total: 5 per RGB)
f_rest_24..44 = L3 coefficients (21 total: 7 per RGB)
```

Each set of coefficients corresponds to:
- First 1/3: Red channel
- Middle 1/3: Green channel
- Last 1/3: Blue channel

Within each color, the basis function order is:
- L1: Y_1^{-1}, Y_1^0, Y_1^1 (y, z, x directions)
- L2: Y_2^{-2}, Y_2^{-1}, Y_2^0, Y_2^1, Y_2^2
- L3: Y_3^{-3} through Y_3^3

---

## 8. Performance Considerations

### Degree Selection

| Degree | Texture Reads | Quality | Use Case |
|--------|--------------|---------|----------|
| 0 | 1 | Flat | Preview, VR, mobile |
| 1 | 4 | Diffuse | General use |
| 2 | 9 | Specular | High quality |
| 3 | 16 | Full | Showcase, cutscenes |

### Optimization Options

1. **LOD by distance** - Use lower SH degree for distant splats
2. **View coherence** - Cache SH results when camera moves slowly
3. **Precompute for static views** - Bake SH into color for known viewpoints

---

## 9. Implementation Checklist

- [ ] Create Niagara Scratch Pad module `NS_EvaluateSH`
- [ ] Implement SH basis functions in HLSL
- [ ] Handle texture coordinate calculation for multi-pixel textures
- [ ] Add camera position binding in LiveActor
- [ ] Create `ShDegree` user parameter for quality control
- [ ] Test with known good PLY (verify coefficient ordering)
- [ ] Add LOD support based on distance
- [ ] Performance profiling

---

## 10. References

- [Original 3DGS Paper](https://repo-sam.inria.fr/fungraph/3d-gaussian-splatting/) - Section on SH representation
- [sebh/HLSL-Spherical-Harmonics](https://github.com/sebh/HLSL-Spherical-Harmonics) - Reference HLSL implementation
- [Real Spherical Harmonics](https://en.wikipedia.org/wiki/Spherical_harmonics#Real_form) - Mathematical background
- [Stupid SH Tricks](https://www.ppsloan.org/publications/StupidSH36.pdf) - Classic SH optimization paper

---

## 11. Alternative: Simplified L1-Only Implementation

For a quick win with most visual benefit, implement L1 only:

```hlsl
// Simplified L1 evaluation (most of the view-dependent effect)
float3 EvaluateSH_L1(float3 viewDir, float3 dc, float3 l1_r, float3 l1_g, float3 l1_b)
{
    float3 result = SH_C0 * dc + 0.5;

    float x = viewDir.x, y = viewDir.y, z = viewDir.z;
    float3 sh1 = float3(y, z, x) * SH_C1;

    result.r += dot(l1_r, sh1);
    result.g += dot(l1_g, sh1);
    result.b += dot(l1_b, sh1);

    return saturate(result);
}
```

This adds only 3 texture samples per splat (vs 1 for L0-only) and captures most view-dependent variation.
