"""Create a stripped, game-ready Kevin rig from Kevin_Main.blend.

Run from Blender with the source blend already opened. The script deliberately
keeps only the deform skeleton, bakes the three locomotion actions visually,
puts the mesh feet at the world origin, and saves a new .blend.
"""

import bpy
from pathlib import Path


OUTPUT = Path(r"C:\Users\karin\OneDrive\Desktop\Kevin.blend")
ARMATURE_NAME = "Armature"
MESH_NAME = "MaleBaseMesh LowPoly"
LOCOMOTION_ACTIONS = ("Kevin_Idle", "Kevin_Walk", "Kevin_Run")
TARGET_HEIGHT = 1.2034  # measured from MainPlayer.glb


def activate_only(obj):
    bpy.ops.object.mode_set(mode="OBJECT") if bpy.context.object and bpy.context.object.mode != "OBJECT" else None
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj


def bake_locomotion_actions(armature):
    animation_data = armature.animation_data_create()
    scene = bpy.context.scene
    deform_names = [bone.name for bone in armature.data.bones if bone.use_deform]
    deform_names.sort(key=lambda name: bone_depth(armature.data.bones[name]))
    samples = {}

    # The authored Kevin actions animate controller bones. Sample their final
    # evaluated pose before we remove those controllers, then write that pose
    # directly to the 65 skin/deform bones as normal keyframes.
    for action_name in LOCOMOTION_ACTIONS:
        action = bpy.data.actions.get(action_name)
        if action is None:
            print(f"[Kevin] Skipping missing action: {action_name}")
            continue

        start, end = (round(value) for value in action.frame_range)
        animation_data.action = action
        action_frames = {}
        for frame in range(start, end + 1):
            scene.frame_set(frame)
            bpy.context.view_layer.update()
            action_frames[frame] = {
                name: armature.pose.bones[name].matrix.copy()
                for name in deform_names
            }
        samples[action_name] = action_frames

    for track in list(animation_data.nla_tracks):
        animation_data.nla_tracks.remove(track)
    animation_data.action = None
    for pose_bone in armature.pose.bones:
        for constraint in list(pose_bone.constraints):
            pose_bone.constraints.remove(constraint)

    baked_actions = {}
    for action_name, action_frames in samples.items():
        baked_action = bpy.data.actions.new(f"__KevinBaked__{action_name}")
        # Only one action can be active on an armature at a time. Preserve the
        # inactive locomotion clips in the .blend instead of letting Blender
        # discard them as orphaned data during save.
        baked_action.use_fake_user = True
        baked_actions[action_name] = baked_action
        animation_data.action = baked_action
        for frame, pose_matrices in action_frames.items():
            scene.frame_set(frame)
            for name in deform_names:
                pose_bone = armature.pose.bones[name]
                pose_bone.rotation_mode = "QUATERNION"
                pose_bone.matrix = pose_matrices[name]
                pose_bone.keyframe_insert(data_path="location", frame=frame)
                pose_bone.keyframe_insert(data_path="rotation_quaternion", frame=frame)
                pose_bone.keyframe_insert(data_path="scale", frame=frame)
        print(f"[Kevin] Baked deform-bone pose: {action_name}")

    return baked_actions


def bone_depth(bone):
    depth = 0
    while bone.parent is not None:
        depth += 1
        bone = bone.parent
    return depth


def remove_non_deform_bones(armature):
    non_deform_names = {
        bone.name for bone in armature.data.bones if not bone.use_deform
    }
    activate_only(armature)
    bpy.ops.object.mode_set(mode="POSE")
    bpy.ops.object.mode_set(mode="EDIT")
    edit_bones = armature.data.edit_bones
    # bpy.ops.armature.delete() does not reliably remove every selected helper
    # bone in Blender 5.2 when the rig has nested controller chains. Remove
    # the edit bones directly; Blender safely reparents surviving deform bones.
    for name in non_deform_names:
        if name in edit_bones:
            edit_bones.remove(edit_bones[name])
    bpy.ops.object.mode_set(mode="OBJECT")
    print(f"[Kevin] Retained {len(armature.data.bones)} deform bones")


def move_feet_to_origin(armature, mesh):
    depsgraph = bpy.context.evaluated_depsgraph_get()
    evaluated_mesh = mesh.evaluated_get(depsgraph)
    feet_z = min((evaluated_mesh.matrix_world @ vertex.co).z for vertex in evaluated_mesh.data.vertices)
    armature.location.z -= feet_z

    # Apply object transforms so the engine receives an identity root object.
    activate_only(armature)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    activate_only(mesh)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    print(f"[Kevin] Moved feet to Z=0 (source feet Z was {feet_z:.4f})")


def scale_to_main_player_height(armature, mesh):
    vertices = [mesh.matrix_world @ vertex.co for vertex in mesh.data.vertices]
    current_height = max(vertex.z for vertex in vertices) - min(vertex.z for vertex in vertices)
    if current_height <= 0.0001:
        raise RuntimeError("Kevin mesh has no usable height")
    factor = TARGET_HEIGHT / current_height
    activate_only(armature)
    armature.scale = (factor, factor, factor)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    print(f"[Kevin] Scaled to MainPlayer height {TARGET_HEIGHT:.4f} (factor {factor:.6f})")


def clean_scene(armature, mesh, baked_actions):
    for obj in list(bpy.data.objects):
        if obj not in {armature, mesh}:
            bpy.data.objects.remove(obj, do_unlink=True)

    # The mesh must remain bound to the one exported armature.
    mesh.parent = armature
    for modifier in mesh.modifiers:
        if modifier.type == "ARMATURE":
            modifier.object = armature

    # The Mixamo actions and original controller-driven actions belong to the
    # deleted transfer/control rig. Keep only our independently baked actions.
    armature.animation_data.action = None
    for action in list(bpy.data.actions):
        if action not in baked_actions.values():
            bpy.data.actions.remove(action)

    for original_name, baked_action in baked_actions.items():
        baked_action.name = original_name


def main():
    armature = bpy.data.objects.get(ARMATURE_NAME)
    mesh = bpy.data.objects.get(MESH_NAME)
    if armature is None or armature.type != "ARMATURE":
        raise RuntimeError(f"Expected armature '{ARMATURE_NAME}' was not found")
    if mesh is None or mesh.type != "MESH":
        raise RuntimeError(f"Expected mesh '{MESH_NAME}' was not found")

    baked_actions = bake_locomotion_actions(armature)
    remove_non_deform_bones(armature)
    move_feet_to_origin(armature, mesh)
    scale_to_main_player_height(armature, mesh)
    clean_scene(armature, mesh, baked_actions)

    armature.animation_data.action = bpy.data.actions.get("Kevin_Idle")
    bpy.context.scene.frame_set(1)
    bpy.ops.wm.save_as_mainfile(filepath=str(OUTPUT), check_existing=False)
    print(f"[Kevin] Saved game-ready character to {OUTPUT}")


main()
