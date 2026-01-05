# Exploration Notes

Things explored and tabled or abandoned for now. Some might be taken up again later. Others may warn against trying it again.

---

## 1. RenderTarget Intermediary Approach

**Status:** TABLED - simpler solution found, but approach valid for other use cases
**Verdict:** May revisit for GPU-side texture manipulation or async loading
**Date explored:** January 2026

### The Problem

The original Niagara system only sampled texture parameters at particle **Init** (spawn). Changing texture references at runtime had no effect - particles kept using textures sampled when spawned. This blocked 4DGS sequence playback.

### Attempted Solution

Use RenderTargets as stable intermediaries:

```
Frame Textures → Copy → RenderTargets → Niagara samples → Particles
     ↓                       ↑
   (changes)            (fixed refs)
```

1. Create 8 RenderTargets (position, scale, color, rotation, harmonics x4)
2. Assign RT references to Niagara (these never change)
3. Each frame, copy current textures into the RTs
4. Niagara sees updated content through fixed RT references

### Implementation Attempts

#### Canvas-Based Copy

```cpp
UCanvas* Canvas;
FVector2D CanvasSize;
FDrawToRenderTargetContext Context;
UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(
    WorldContextObject, Target, Canvas, CanvasSize, Context);
Canvas->K2_DrawTexture(Source, FVector2D::ZeroVector, CanvasSize, ...);
UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(WorldContextObject, Context);
```

**Note:** Had some issues during testing but may have been due to other factors changed simultaneously. Not definitively broken - worth retrying if RT approach is revisited.

#### Material-Based Copy (Worked)

```cpp
// Required M_CopyTexture material asset:
// - Domain: Post Process
// - TextureSampleParameter2D("SourceTexture") → Emissive Color

UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(CopyMaterial, ...);
MID->SetTextureParameterValue(TEXT("SourceTexture"), Source);
UKismetRenderingLibrary::DrawMaterialToRenderTarget(WorldContextObject, Target, MID);
```

**Problem:** Required material asset, RT size management, initialization ordering.

### The Real Solution

Modify Niagara to sample textures on **Update**, not just Init:
- In Niagara editor, texture sample modules were only in Particle Spawn section
- Moving/copying them to Particle Update enables runtime texture changes
- With this, direct `SetVariableTexture()` works without RT intermediaries

### When RT Approach Might Be Useful

1. **GPU-side texture manipulation** - blend, transform, composite before Niagara reads
2. **Streaming/async loading** - RTs provide stable refs while textures load
3. **Multiple consumers** - one RT read by multiple Niagara systems
4. **Format conversion** - source in one format, Niagara needs another

### Alternative: Direct Texture Memory Writing

For highest performance, consider writing directly to texture memory instead of the RT copy approach:

```cpp
// Lock texture, write data, unlock
FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
// ... write pixel data directly ...
Mip.BulkData.Unlock();
Texture->UpdateResource();
```

This bypasses the render pipeline entirely - useful for streaming raw frame data from disk or network without going through UAsset loading.

### Code Preserved (not in fork)

The original utilities were in `GaussianSplatRTUtils.h/cpp`:
- `CreateRenderTarget()` - Create sized RT with RGBA32f format
- `CreateAllRenderTargets()` - Create the 8 standard RTs
- `CopyTextureToRT()` - Material-based texture copy
- `CopyAllTexturesToRTs()` - Bulk copy for frame updates

---

## 2. Grid Subdivision

**Status:** REMOVED - incomplete implementation, never used at runtime
**Verdict:** May revisit for massive static models (>2M splats) if culling/LOD needed
**Date removed:** January 2026

### Original Intent

Split large splat models into spatial grid cells for:
- Frustum culling per cell
- LOD based on cell distance
- Streaming cells in/out

### How It Worked

```cpp
int getGridIndex(const FVector& Position, int CellsPerEdge, ...) {
    // Map position to grid cell index
    int CellX = FMath::Clamp((int)((Position.X - MinBounds.X) / CellSize), 0, CellsPerEdge-1);
    int CellY = FMath::Clamp((int)((Position.Y - MinBounds.Y) / CellSize), 0, CellsPerEdge-1);
    int CellZ = FMath::Clamp((int)((Position.Z - MinBounds.Z) / CellSize), 0, CellsPerEdge-1);
    return CellX + CellY * CellsPerEdge + CellZ * CellsPerEdge * CellsPerEdge;
}
```

Preprocessing would:
1. Compute bounding box of all splats
2. Divide into `CellsPerEdge^3` cells
3. Sort splats into cells
4. Create separate texture sets per cell
5. Output to `Models/{name}/Emitters/0/`, `Emitters/1/`, etc.

### Why It Was Removed

1. **Runtime never used it** - All code paths only accessed `TexLocations[0]`
2. **No culling implemented** - Grid existed but no frustum/distance logic
3. **Added complexity** - Extra folder nesting, array management
4. **Untested** - Never validated with real large models

### Future Consideration

Could be re-added if needed for massive static models (>2M splats). Would require:
- Per-cell Niagara emitters or instancing
- Frustum culling logic
- Distance-based LOD or streaming

---

## 3. Removed Actor Classes

### GaussianSplatSequence / GaussianSplatSequenceActor

**What:** Component + Actor for sequence playback using RT approach
**Why removed:** RT approach wasn't needed once Niagara sampling fixed
**Replaced by:** `GaussianSplatLiveActor` (simpler, direct texture swapping)

### GaussianSplatRTTestActor

**What:** Debug actor with editor buttons for step-by-step RT testing
**Why removed:** Only useful during RT approach development
**Contents:** Buttons like "Create RTs", "Load Frame", "Copy to RTs", exposed RT references

---

## 4. Key Learnings

### Niagara Texture Sampling

- **Init-only sampling:** Default behavior, textures read once at particle spawn
- **Update sampling:** Add texture sample to Particle Update to enable runtime changes
- **Parameter types:** Use `Texture2D` user parameters, set via `SetVariableTexture()`

### Texture Asset Management

- `UEditorAssetLibrary::SaveLoadedAsset()` is synchronous and slow
- 30 frames × 8 textures = 240 save operations = noticeable delay
- Future optimization: async saving, batch operations, or raw file output

### Path Handling

- UE expects paths relative to Content/ for asset operations
- `FPaths::NormalizeDirectoryName()` essential for cross-platform path comparison
- File dialogs return absolute paths, must convert to relative

---

## 5. Upstream Comparison

### JI20/unreal-splat (this fork's upstream)

- Academic project from TU Munich
- Early-stage, minimal maintenance
- Spherical harmonics WIP/disabled
- Good foundation, needs polish

### XV3DGS / XScene-UEPlugin (alternative)

- 971+ stars, actively maintained by XVERSE Technology (Shenzhen)
- Claims SH support up to degree 3
- **Binary-only distribution** - Source folder contains only Build.cs, no .cpp/.h
- Cannot learn from or modify their SH implementation
- Useful for comparison/validation, not as code reference

### liaoKM/3DGS-UE5-Plugin (RetinaGS)

- Open source with actual shader code
- Has SH infrastructure but **only L0 implemented**
- View direction calculated but unused (dead code)
- Minimal maintenance (10 commits, UE 5.4.4 only)
- Useful reference for shader structure

### SH Implementation Status (January 2026)

No open-source UE5 3DGS plugin has working higher-order SH evaluation.
See `SPHERICAL_HARMONICS_IMPLEMENTATION.md` for implementation plan

---

## 6. Files Reference

### Kept in Fork

| File | Purpose |
|------|---------|
| `Parser.h/cpp` | PLY preprocessing, texture generation |
| `GaussianSplatLiveActor.h/cpp` | 4DGS sequence playback |
| `SUnrealSplatWindow.h/cpp` | Native Slate preprocessing UI |
| `Miniply.h/cpp` | PLY parsing library |
| `UnrealSplat.h/cpp` | Plugin module |

### Not in Fork (local only)

| File | Purpose |
|------|---------|
| `GaussianSplatRTUtils.h/cpp` | RT utility functions |
| `GaussianSplatSequence.h/cpp` | RT-based sequence component |
| `GaussianSplatSequenceActor.h/cpp` | RT-based sequence actor |
| `GaussianSplatRTTestActor.h/cpp` | RT debugging actor |
| `M_CopyTexture.uasset` | Material for RT copying |
| `create_copy_material.py` | Script to create copy material |

### Documentation

| File | Purpose |
|------|---------|
| `EXPLORATION_NOTES.md` | This file - tabled/abandoned approaches |
| `SPHERICAL_HARMONICS_IMPLEMENTATION.md` | SH implementation plan and HLSL code |
