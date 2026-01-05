"""
Export 4DGS deformation network to UE5-compatible format.

Exports:
1. HexPlane feature grids as raw float arrays (for 3D textures)
2. MLP weights as raw float arrays
3. Metadata JSON for reconstruction in UE5

Usage:
    python export_deformation_network.py --model_path output/dnerf/bouncingballs \
        --configs arguments/dnerf/bouncingballs.py \
        --output_dir output/dnerf/bouncingballs/ue5_export
"""

import os
import json
import torch
import numpy as np
from argparse import ArgumentParser

from scene import Scene
from gaussian_renderer import GaussianModel
from arguments import ModelParams, PipelineParams, get_combined_args, ModelHiddenParams
from utils.general_utils import safe_state


def export_hexplane_grids(deformation, output_dir):
    """Export HexPlane feature grids as raw float arrays."""

    grid = deformation.grid
    plane_data = {}

    # HexPlane has 6 planes: XY, XZ, YZ, XT, YT, ZT
    plane_names = ['plane_xy', 'plane_xz', 'plane_yz', 'plane_xt', 'plane_yt', 'plane_zt']

    for i, name in enumerate(plane_names):
        if hasattr(grid, 'grids') and len(grid.grids) > i:
            plane_tensor = grid.grids[i]
            plane_np = plane_tensor.detach().cpu().numpy()

            # Save as raw float32
            output_path = os.path.join(output_dir, f'{name}.raw')
            plane_np.astype(np.float32).tofile(output_path)

            plane_data[name] = {
                'shape': list(plane_np.shape),
                'dtype': 'float32',
                'file': f'{name}.raw'
            }
            print(f"  {name}: {plane_np.shape}")

    return plane_data


def export_mlp_weights(deformation, output_dir):
    """Export MLP weights as raw float arrays."""

    mlp_data = {}

    # Feature extraction MLP
    if hasattr(deformation, 'feature_out'):
        for i, layer in enumerate(deformation.feature_out):
            if hasattr(layer, 'weight'):
                w = layer.weight.detach().cpu().numpy()
                b = layer.bias.detach().cpu().numpy() if layer.bias is not None else None

                w_path = os.path.join(output_dir, f'feature_out_{i}_weight.raw')
                w.astype(np.float32).tofile(w_path)

                mlp_data[f'feature_out_{i}'] = {
                    'weight_shape': list(w.shape),
                    'weight_file': f'feature_out_{i}_weight.raw'
                }

                if b is not None:
                    b_path = os.path.join(output_dir, f'feature_out_{i}_bias.raw')
                    b.astype(np.float32).tofile(b_path)
                    mlp_data[f'feature_out_{i}']['bias_shape'] = list(b.shape)
                    mlp_data[f'feature_out_{i}']['bias_file'] = f'feature_out_{i}_bias.raw'

    # Deformation heads
    head_names = ['pos_deform', 'scales_deform', 'rotations_deform', 'opacity_deform', 'shs_deform']
    head_outputs = [3, 3, 4, 1, 48]  # Output dimensions

    for head_name, out_dim in zip(head_names, head_outputs):
        if hasattr(deformation, head_name):
            head = getattr(deformation, head_name)
            head_data = {'output_dim': out_dim, 'layers': []}

            for i, layer in enumerate(head):
                if hasattr(layer, 'weight'):
                    w = layer.weight.detach().cpu().numpy()
                    b = layer.bias.detach().cpu().numpy() if layer.bias is not None else None

                    w_path = os.path.join(output_dir, f'{head_name}_{i}_weight.raw')
                    w.astype(np.float32).tofile(w_path)

                    layer_data = {
                        'weight_shape': list(w.shape),
                        'weight_file': f'{head_name}_{i}_weight.raw'
                    }

                    if b is not None:
                        b_path = os.path.join(output_dir, f'{head_name}_{i}_bias.raw')
                        b.astype(np.float32).tofile(b_path)
                        layer_data['bias_shape'] = list(b.shape)
                        layer_data['bias_file'] = f'{head_name}_{i}_bias.raw'

                    head_data['layers'].append(layer_data)

            mlp_data[head_name] = head_data
            print(f"  {head_name}: {len(head_data['layers'])} layers -> {out_dim}D")

    return mlp_data


def export_canonical_gaussians(gaussians, output_dir):
    """Export canonical Gaussian parameters as raw arrays."""

    gaussian_data = {}

    # Position
    xyz = gaussians._xyz.detach().cpu().numpy()
    xyz.astype(np.float32).tofile(os.path.join(output_dir, 'positions.raw'))
    gaussian_data['positions'] = {'shape': list(xyz.shape), 'file': 'positions.raw'}

    # Scale (log space)
    scale = gaussians._scaling.detach().cpu().numpy()
    scale.astype(np.float32).tofile(os.path.join(output_dir, 'scales.raw'))
    gaussian_data['scales'] = {'shape': list(scale.shape), 'file': 'scales.raw'}

    # Rotation (quaternion)
    rot = gaussians._rotation.detach().cpu().numpy()
    rot.astype(np.float32).tofile(os.path.join(output_dir, 'rotations.raw'))
    gaussian_data['rotations'] = {'shape': list(rot.shape), 'file': 'rotations.raw'}

    # Opacity (logit space)
    opacity = gaussians._opacity.detach().cpu().numpy()
    opacity.astype(np.float32).tofile(os.path.join(output_dir, 'opacities.raw'))
    gaussian_data['opacities'] = {'shape': list(opacity.shape), 'file': 'opacities.raw'}

    # SH DC
    f_dc = gaussians._features_dc.detach().cpu().numpy()
    f_dc.astype(np.float32).tofile(os.path.join(output_dir, 'sh_dc.raw'))
    gaussian_data['sh_dc'] = {'shape': list(f_dc.shape), 'file': 'sh_dc.raw'}

    # SH rest
    f_rest = gaussians._features_rest.detach().cpu().numpy()
    f_rest.astype(np.float32).tofile(os.path.join(output_dir, 'sh_rest.raw'))
    gaussian_data['sh_rest'] = {'shape': list(f_rest.shape), 'file': 'sh_rest.raw'}

    print(f"  {xyz.shape[0]} Gaussians exported")

    return gaussian_data


def export_deformation_network(dataset, hyperparam, output_dir):
    """Export complete deformation network for UE5."""

    with torch.no_grad():
        # Load model
        gaussians = GaussianModel(dataset.sh_degree, hyperparam)
        scene = Scene(dataset, gaussians, load_iteration=-1, shuffle=False)

        print(f"Loaded {gaussians._xyz.shape[0]} Gaussians")

        # Create output directory
        os.makedirs(output_dir, exist_ok=True)

        # Export components
        print("\nExporting canonical Gaussians...")
        gaussian_data = export_canonical_gaussians(gaussians, output_dir)

        print("\nExporting HexPlane grids...")
        plane_data = export_hexplane_grids(gaussians._deformation, output_dir)

        print("\nExporting MLP weights...")
        mlp_data = export_mlp_weights(gaussians._deformation, output_dir)

        # Get AABB
        aabb = gaussians._deformation.grid.get_aabb
        if isinstance(aabb, torch.Tensor):
            aabb = aabb.detach().cpu().numpy().tolist()

        # Write metadata
        metadata = {
            'num_gaussians': gaussians._xyz.shape[0],
            'sh_degree': gaussians.max_sh_degree,
            'aabb': aabb,
            'gaussians': gaussian_data,
            'hexplane': plane_data,
            'mlp': mlp_data,
            'network_config': {
                'feature_dim': 64,  # From config
                'mlp_width': 256,
                'mlp_depth': 8,
            }
        }

        metadata_path = os.path.join(output_dir, 'deformation_network.json')
        with open(metadata_path, 'w') as f:
            json.dump(metadata, f, indent=2)

        print(f"\nExported to: {output_dir}")
        print(f"  Metadata: deformation_network.json")


if __name__ == "__main__":
    parser = ArgumentParser(description="Export 4DGS deformation network for UE5")
    model = ModelParams(parser, sentinel=True)
    hyperparam = ModelHiddenParams(parser)

    parser.add_argument("--output_dir", type=str, default=None)
    parser.add_argument("--configs", type=str, required=True)
    parser.add_argument("--quiet", action="store_true")

    args = get_combined_args(parser)

    if args.configs:
        import mmcv
        from utils.params_utils import merge_hparams
        config = mmcv.Config.fromfile(args.configs)
        args = merge_hparams(args, config)

    safe_state(args.quiet)

    output_dir = args.output_dir or os.path.join(args.model_path, "ue5_export")

    export_deformation_network(
        model.extract(args),
        hyperparam.extract(args),
        output_dir
    )
