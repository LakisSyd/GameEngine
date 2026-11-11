"""Bake Kevin's controller Actions to deform bones and export a game GLB."""

from pathlib import Path

import bpy


OUTPUT_BLEND = Path(r"C:\Users\karin\OneDrive\Desktop\Kevin_Rifle_Game.blend")
OUTPUT_GLB = Path(
    r"C:\Users\karin\OneDrive\Desktop\DX_FrameWork\DX_FrameWork\assets\model\Kevin_Rifle.glb"
)
ACTIONS = ("Kevin_Idle", "Kevin_Walk", "Kevin_Run", "Rifle_Idle")
TARGET_HEIGHT = 1.2034

rig = bpy.data.objects["Armature"]
mesh = bpy.data.objects["MaleBaseMesh LowPoly"]
scene = bpy.context.scene
animation = rig.animation_data_create()


def activate_only(obj):
    if bpy.context.object and bpy.context.object.mode != "OBJECT":
        bpy.ops.object.mode_set(mode="OBJECT")
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj


def depth(bone):
    result = 0
    while bone.parent:
        bone = bone.parent
        result += 1
    return result


deform_names = sorted(
    (bone.name for bone in rig.data.bones if bone.use_deform),
    key=lambda name: depth(rig.data.bones[name]),
)

samples = {}
for action_name in ACTIONS:
    action = bpy.data.actions.get(action_name)
    if action is None:
        raise RuntimeError(f"Required Action is missing: {action_name}")
    animation.action = action
    start, end = (int(round(value)) for value in action.frame_range)
    action_samples = {}
    for frame in range(start, end + 1):
        scene.frame_set(frame)
        bpy.context.view_layer.update()
        action_samples[frame] = {
            name: rig.pose.bones[name].matrix.copy() for name in deform_names
        }
    samples[action_name] = action_samples
    print("SAMPLED", action_name, start, end)

for track in list(animation.nla_tracks):
    animation.nla_tracks.remove(track)
animation.action = None
for pb in rig.pose.bones:
    for constraint in list(pb.constraints):
        pb.constraints.remove(constraint)

baked = {}
for action_name, frames in samples.items():
    result = bpy.data.actions.new(f"__baked__{action_name}")
    result.use_fake_user = True
    animation.action = result
    for frame, matrices in frames.items():
        scene.frame_set(frame)
        for name in deform_names:
            pb = rig.pose.bones[name]
            pb.rotation_mode = "QUATERNION"
            pb.matrix = matrices[name]
            pb.keyframe_insert("location", frame=frame, group=name)
            pb.keyframe_insert("rotation_quaternion", frame=frame, group=name)
            pb.keyframe_insert("scale", frame=frame, group=name)
    baked[action_name] = result
    print("BAKED", action_name, tuple(result.frame_range))

# Remove all controller/helper bones after their final poses have been baked.
non_deform = [bone.name for bone in rig.data.bones if not bone.use_deform]
activate_only(rig)
bpy.ops.object.mode_set(mode="EDIT")
for name in non_deform:
    bone = rig.data.edit_bones.get(name)
    if bone:
        rig.data.edit_bones.remove(bone)
bpy.ops.object.mode_set(mode="OBJECT")

# Keep only the game character. The rifle is spawned and socketed separately.
for obj in list(bpy.data.objects):
    if obj not in {rig, mesh}:
        bpy.data.objects.remove(obj, do_unlink=True)
mesh.parent = rig
for modifier in mesh.modifiers:
    if modifier.type == "ARMATURE":
        modifier.object = rig

animation.action = None
for action in list(bpy.data.actions):
    if action not in baked.values():
        bpy.data.actions.remove(action)
for name, action in baked.items():
    action.name = name

# Match the existing MainPlayer asset scale and place the feet at Z=0.
scene.frame_set(1)
animation.action = bpy.data.actions["Rifle_Idle"]
bpy.context.view_layer.update()
evaluated = mesh.evaluated_get(bpy.context.evaluated_depsgraph_get())
points = [evaluated.matrix_world @ vertex.co for vertex in evaluated.data.vertices]
height = max(p.z for p in points) - min(p.z for p in points)
factor = TARGET_HEIGHT / height
activate_only(rig)
rig.scale = tuple(value * factor for value in rig.scale)
bpy.context.view_layer.update()
evaluated = mesh.evaluated_get(bpy.context.evaluated_depsgraph_get())
feet = min((evaluated.matrix_world @ vertex.co).z for vertex in evaluated.data.vertices)
rig.location.z -= feet
bpy.context.view_layer.update()

bpy.context.preferences.filepaths.save_version = 0
bpy.ops.wm.save_as_mainfile(filepath=str(OUTPUT_BLEND), compress=True, check_existing=False)

activate_only(rig)
mesh.select_set(True)
kwargs = {
    "filepath": str(OUTPUT_GLB),
    "export_format": "GLB",
    "use_selection": True,
    "export_animations": True,
    "export_frame_range": False,
    "export_force_sampling": False,
    "export_yup": True,
    "export_rest_position_armature": False,
}
properties = {p.identifier for p in bpy.ops.export_scene.gltf.get_rna_type().properties}
if "export_optimize_animation_size" in properties:
    kwargs["export_optimize_animation_size"] = True
if "export_optimize_animation_keep_anim_armature" in properties:
    kwargs["export_optimize_animation_keep_anim_armature"] = True
bpy.ops.export_scene.gltf(**kwargs)
print("SAVED_BLEND", OUTPUT_BLEND)
print("EXPORTED_GLB", OUTPUT_GLB)
print("DEFORM_BONES", len(rig.data.bones))
print("ACTIONS", sorted(action.name for action in bpy.data.actions))
