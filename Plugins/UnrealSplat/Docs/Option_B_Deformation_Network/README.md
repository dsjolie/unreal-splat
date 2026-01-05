# Option B: Deformation Network

**Status:** FUTURE WORK - Sketched but not implemented
**Complexity:** High (weeks of work)
**Prerequisites:** Option A working (done), understanding of 4DGaussians architecture

---

## Overview

Port the HexPlane + MLP deformation network from 4DGaussians to a GPU compute shader. This enables true real-time 4D Gaussian Splatting with arbitrary time interpolation.

### Comparison to Option A (PLY Sequence)

| Aspect | Option A (Current) | Option B (This) |
|--------|-------------------|-----------------|
| **Storage** | ~6MB × frames | ~20MB fixed |
| **GPU Memory** | Multiple frame textures | Single canonical + deformed |
| **Quality** | Exact per-frame | Smooth interpolation |
| **Frame Rate** | Limited to pre-baked | Arbitrary time values |
| **Complexity** | Simple | Complex |

---

## Architecture

```
┌─────────────────┐     ┌──────────────────┐     ┌─────────────────┐
│ Canonical       │     │ Deformation      │     │ Deformed        │
│ Gaussians       │────▶│ Compute Shader   │────▶│ Gaussians       │
│ (GPU Buffer)    │     │                  │     │ (GPU Buffer)    │
└─────────────────┘     │  ┌────────────┐  │     └────────┬────────┘
                        │  │ HexPlane   │  │              │
┌─────────────────┐     │  │ Sample     │  │              ▼
│ Time (0-1)      │────▶│  └─────┬──────┘  │     ┌─────────────────┐
└─────────────────┘     │        │         │     │ Niagara         │
                        │        ▼         │     │ Renderer        │
                        │  ┌────────────┐  │     └─────────────────┘
                        │  │ MLP        │  │
                        │  │ Forward    │  │
                        │  └────────────┘  │
                        └──────────────────┘
```

### Compute Shader Flow

1. **Input**: Canonical Gaussian position + time value (0-1)
2. **HexPlane Query**: Sample 6 feature planes (XY, XZ, YZ, XT, YT, ZT)
3. **Feature MLP**: 8 layers × 256 width → hidden features
4. **Deformation Heads**: Separate MLPs for position/scale/rotation/opacity deltas
5. **Output**: Deformed Gaussian parameters → Niagara reads from GPU buffer

---

## Files in This Folder

| File | Purpose |
|------|---------|
| `export_deformation_network.py` | Python script to export trained network weights |
| `GaussianDeformationComponent.h` | UE5 component header (sketch) |
| `GaussianDeformationComponent.cpp` | UE5 component implementation (sketch) |
| `Shaders/GaussianDeformation.usf` | HLSL compute shader (sketch) |

**Note:** These are sketches/starting points, not production code. Significant work needed.

---

## Python Export

Export trained 4DGaussians model to raw weights:

```bash
python export_deformation_network.py \
    --model_path output/dnerf/bouncingballs \
    --configs arguments/dnerf/bouncingballs.py \
    --output_dir output/dnerf/bouncingballs/ue5_export
```

Output structure:
```
ue5_export/
├── deformation_network.json   # Metadata (dims, bounds, etc.)
├── positions.raw              # Canonical Gaussians (float32)
├── scales.raw
├── rotations.raw
├── opacities.raw
├── sh_dc.raw
├── sh_rest.raw
├── plane_xy.raw               # HexPlane grids (float32)
├── plane_xz.raw
├── plane_yz.raw
├── plane_xt.raw
├── plane_yt.raw
├── plane_zt.raw
├── feature_out_*_weight.raw   # MLP weights
├── feature_out_*_bias.raw
├── pos_deform_*_weight.raw
└── ...
```

---

## Implementation Notes

### HexPlane Textures

- 6 planes stored as 2D textures (R32G32B32A32_FLOAT or similar)
- Bilinear sampling for smooth interpolation
- Typical resolution: 64×64 to 256×256 per plane
- Total: ~50-200MB GPU memory depending on resolution

### MLP in HLSL

- Weights packed into textures (width × input_dim/4)
- Biases in structured buffers
- ReLU activations between layers
- Main bottleneck: 8 layers × 256 width × N Gaussians

### Performance Targets

- 30k Gaussians at 60 FPS on RTX 3080
- Main cost: MLP evaluation per Gaussian per frame
- Optimization: Batch processing, wave intrinsics, shared memory

### Niagara Integration

Two approaches:
1. **GPU Buffer → Niagara**: Use Niagara Data Interface to read from compute output
2. **Compute → Texture**: Write deformed Gaussians to texture, sample in Niagara

Option 2 is more compatible with existing UnrealSplat Niagara systems.

---

## Implementation Steps

1. **Python export working** - Verify weights export correctly
2. **Load weights in UE5** - Read raw files, create GPU buffers/textures
3. **Compute shader basics** - Dispatch, read canonical, write deformed
4. **HexPlane sampling** - Implement feature plane queries
5. **MLP forward pass** - Implement layer-by-layer evaluation
6. **Niagara integration** - Connect deformed buffer to renderer
7. **Optimization** - Profile and tune for target performance

---

## When to Use Option B

Consider this approach when:
- Need smooth time interpolation (slow motion, variable speed)
- Storage is a concern (mobile, streaming)
- Want arbitrary playback without pre-baking
- Performance budget allows compute overhead

Stick with Option A when:
- Pre-baked quality is acceptable
- Simpler implementation preferred
- Fixed frame rate content
- GPU compute budget is tight

---

## References

- [4DGaussians Paper](https://guanjunwu.github.io/4dgs/)
- [HexPlane Paper](https://caoang327.github.io/HexPlane/)
- [Original 4DGaussians Code](https://github.com/hustvl/4DGaussians)
