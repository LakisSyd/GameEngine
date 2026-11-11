"""Build Kevin's first-person arms from the already baked game rig.

Run after build_kevin_combat_game.py, or directly with Kevin_Combat_Game.blend
open in background Blender. The master controller file is never modified.
"""

from pathlib import Path

import bmesh
import bpy


ROOT = Path(__file__).resolve().parents[1]
OUTPUT_BLEND = ROOT / "assets/model/Kevin_Combat_FP.blend"
OUTPUT_GLB = ROOT / "assets/model/Kevin_Combat_FP.glb"
ARM_WEIGHT_THRESHOLD = 0.10

rig = bpy.data.objects.get("Kevin_Game")
mesh = bpy.data.objects.get("Kevin_Body")
if not rig or rig.type != "ARMATURE" or not mesh or mesh.type != "MESH":
    raise RuntimeError("Open the baked Kevin_Combat_Game.blend before building FP arms")
if mesh.parent != rig:
    raise RuntimeError("Kevin_Body must be parented to Kevin_Game")

required_actions = {
    "Kevin_Idle", "Kevin_Walk", "Kevin_Run", "Kevin_Walk_R_I",
    "Kevin_Aim_Rifle", "Kevin_Aim_Shoot_R", "Kevin_Punch.1",
}
missing_actions = required_actions.difference(bpy.data.actions.keys())
if missing_actions:
    raise RuntimeError(f"Game rig is missing actions: {sorted(missing_actions)}")


def is_arm_group(name):
    return name.startswith((
        "mixamorig:LeftForeArm", "mixamorig:RightForeArm",
        "mixamorig:LeftHand", "mixamorig:RightHand",
    ))


group_names = {group.index: group.name for group in mesh.vertex_groups}
arm_weights = [
    sum(item.weight for item in vertex.groups
        if is_arm_group(group_names.get(item.group, "")))
    for vertex in mesh.data.vertices
]
kept_faces = {
    face.index for face in mesh.data.polygons
    if all(arm_weights[index] >= ARM_WEIGHT_THRESHOLD for index in face.vertices)
}
if len(kept_faces) < 5000:
    raise RuntimeError(f"Only {len(kept_faces)} arm faces selected; aborting export")

# Delete only geometry. BMesh preserves the existing vertex groups, UV maps,
# materials and skin weights; the full 65-bone skeleton and actions stay intact.
bm = bmesh.new()
bm.from_mesh(mesh.data)
bm.faces.ensure_lookup_table()
bm.faces.index_update()
bmesh.ops.delete(
    bm, geom=[face for face in bm.faces if face.index not in kept_faces],
    context="FACES")
bmesh.ops.delete(
    bm, geom=[vertex for vertex in bm.verts if not vertex.link_faces],
    context="VERTS")
bm.to_mesh(mesh.data)
bm.free()
mesh.data.update()
mesh.name = "Kevin_FP_Arms"
mesh.data.name = "Kevin_FP_Arms_Mesh"

if len(mesh.data.vertices) < 10000 or not any(v.groups for v in mesh.data.vertices):
    raise RuntimeError("FP arms lost geometry or skin weights")

print("FP_MESH", len(mesh.data.vertices), "vertices",
      len(mesh.data.polygons), "faces", len(rig.data.bones), "bones")

bpy.context.preferences.filepaths.save_version = 0
bpy.ops.wm.save_as_mainfile(filepath=str(OUTPUT_BLEND), compress=True)

bpy.ops.object.select_all(action="DESELECT")
rig.select_set(True)
mesh.select_set(True)
bpy.context.view_layer.objects.active = rig
bpy.ops.export_scene.gltf(
    filepath=str(OUTPUT_GLB), export_format="GLB", use_selection=True,
    export_animations=True, export_frame_range=False,
    export_force_sampling=True, export_animation_mode="ACTIONS",
    export_yup=True, export_rest_position_armature=True)
print("FP_EXPORTED", OUTPUT_BLEND, OUTPUT_GLB)
