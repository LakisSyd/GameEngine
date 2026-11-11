"""Create a game-ready copy of SpaceShip.blend without touching the source file.

Run with Blender, not regular Python:
    blender.exe --background SpaceShip.blend --python optimize_spaceship_blend.py -- \
        output.blend output.glb
"""

from __future__ import annotations

import sys
from pathlib import Path

import bpy


# These objects contain roughly 3.4 million of the scene's 3.65 million
# triangles. The ratios leave substantially more geometry than a door/light
# needs in gameplay while preserving the original source in a separate file.
DECIMATE_RATIOS = {
    "SHIP_DOOR_STAIR1_GUN.001": 0.025,
    "DOOR_LEAF_R_STAIR1_GUN": 0.025,
    "SHIP_GENERATOR": 0.040,
    "SHIP_LIGHT_HALL_01": 0.030,
    "SHIP_LIGHT_HALL_02": 0.030,
    "SHIP_LIGHT_REACTOR": 0.030,
    "SHIP_OXYGEN_GENERATOR_01": 0.050,
    "SHIP_STAIR_BRIDGE_02.002": 0.040,
}


def mesh_stats(obj: bpy.types.Object) -> tuple[int, int]:
    if obj.type != "MESH" or obj.data is None:
        return 0, 0
    return len(obj.data.vertices), len(obj.data.polygons)


def apply_decimate(obj: bpy.types.Object, ratio: float) -> None:
    bpy.ops.object.select_all(action="DESELECT")
    obj.hide_set(False)
    obj.hide_viewport = False
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj

    # Replace old unapplied Decimate modifiers so the requested ratio is based
    # on source geometry and the result is deterministic.
    for modifier in list(obj.modifiers):
        if modifier.type == "DECIMATE":
            obj.modifiers.remove(modifier)

    modifier = obj.modifiers.new(name="GameReadyDecimate", type="DECIMATE")
    modifier.decimate_type = "COLLAPSE"
    modifier.ratio = ratio
    modifier.use_collapse_triangulate = True
    bpy.ops.object.modifier_apply(modifier=modifier.name)


def main() -> None:
    args = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    if len(args) != 2:
        raise SystemExit("Expected output .blend and output .glb paths after --")

    output_blend = Path(args[0]).resolve()
    output_glb = Path(args[1]).resolve()
    output_blend.parent.mkdir(parents=True, exist_ok=True)
    output_glb.parent.mkdir(parents=True, exist_ok=True)

    print("=== SpaceShip optimization ===")
    for name, ratio in DECIMATE_RATIOS.items():
        obj = bpy.data.objects.get(name)
        if obj is None:
            print(f"WARNING: object not found: {name}")
            continue
        if obj.type != "MESH":
            print(f"WARNING: object is not a mesh: {name} ({obj.type})")
            continue

        before_vertices, before_faces = mesh_stats(obj)
        apply_decimate(obj, ratio)
        after_vertices, after_faces = mesh_stats(obj)
        print(
            f"{name}: vertices {before_vertices} -> {after_vertices}, "
            f"faces {before_faces} -> {after_faces}, ratio={ratio:.3f}"
        )

    bpy.ops.wm.save_as_mainfile(filepath=str(output_blend), check_existing=False)
    bpy.ops.export_scene.gltf(
        filepath=str(output_glb),
        export_format="GLB",
        export_apply=True,
        export_materials="EXPORT",
        export_yup=True,
    )
    print(f"Saved optimized blend: {output_blend}")
    print(f"Exported optimized GLB: {output_glb}")


if __name__ == "__main__":
    main()
