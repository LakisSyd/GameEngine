"""Build a looping walk in Kevin's compact unarmed guard.

Open either the main or FP control .blend in Blender and run this script.
An in-place boxing step keeps the guard and stance while shifting weight.
"""

import math
import bpy
from mathutils import Vector

rig = bpy.data.objects['Armature']
scene = bpy.context.scene
animation = rig.animation_data_create()
for track in animation.nla_tracks:
    track.mute = True

guard = bpy.data.actions['Kevin_Melee_Guard']
is_first_person = 'FP_Source_Body_Bounds_Min' in rig
initial = {bone.name: bone.matrix_basis.copy() for bone in rig.pose.bones}


def activate(action, frame):
    animation.action = None
    for bone in rig.pose.bones:
        bone.matrix_basis = initial[bone.name]
    animation.action = action
    animation.action_slot = action.slots[0]
    whole = math.floor(frame)
    scene.frame_set(whole, subframe=frame - whole)
    bpy.context.view_layer.update()


def capture():
    return {
        bone.name: (bone.matrix_basis.copy(), {
            name: value for name, value in bone.items()
            if isinstance(value, (int, float, bool))})
        for bone in rig.pose.bones
    }


def move_world(name, offset):
    bone = rig.pose.bones[name]
    matrix = bone.matrix.copy()
    matrix.translation += rig.matrix_world.inverted().to_3x3() @ Vector(offset)
    bone.matrix = matrix
    bpy.context.view_layer.update()


samples = []
for frame in range(1, 62, 2):
    phase = 2.0 * math.pi * (frame - 1) / 60.0
    guard_frame = 1.0 + 48.0 * (frame - 1) / 60.0
    activate(guard, guard_frame)
    sway = math.sin(phase)
    step = 0.25 * sway
    left_lift = 0.09 * max(0.0, sway)
    right_lift = 0.09 * max(0.0, -sway)
    body_bob = -0.025 * (1.0 - math.cos(2.0 * phase))

    # Alternating short steps keep the feet in their original staggered lanes.
    move_world('CTRL_Foot_IK.L', (0.015 * sway, -step, left_lift))
    move_world('CTRL_Foot_IK.R', (-0.015 * sway, step, right_lift))
    move_world('CTRL_Knee_Pole.L', (0, -step * 0.35, 0))
    move_world('CTRL_Knee_Pole.R', (0, step * 0.35, 0))
    move_world('CTRL_COG', (0.025 * sway, -0.045 * sway,
                            body_bob * (0.55 if is_first_person else 1.0)))

    # The gloves follow the gait slightly without leaving the face unguarded.
    glove_lift = 0.014 * (math.cos(phase) - 1.0)
    move_world('CTRL_Hand_IK.L', (0.012 * sway, -0.025 * sway,
                                  glove_lift))
    move_world('CTRL_Hand_IK.R', (-0.012 * sway, 0.025 * sway,
                                  -glove_lift * 0.7))
    samples.append((frame, capture()))

name = 'Kevin_Melee_Guard_Walk'
old = bpy.data.actions.get(name)
if old:
    animation.action = None
    bpy.data.actions.remove(old)
action = bpy.data.actions.new(name)
action.use_fake_user = True
action['Kevin_Melee_Description'] = 'Looping guarded walk with subtle glove motion'
animation.action = action
for frame, pose in samples:
    scene.frame_set(frame)
    for bone_name, (basis, properties) in pose.items():
        bone = rig.pose.bones[bone_name]
        bone.matrix_basis = basis
        for key, value in properties.items():
            bone[key] = value
        for path in ('location',
                     'rotation_quaternion' if bone.rotation_mode == 'QUATERNION'
                     else 'rotation_euler', 'scale'):
            bone.keyframe_insert(data_path=path, frame=frame, group=bone_name)
        for key in properties:
            bone.keyframe_insert(data_path=f'["{key}"]', frame=frame,
                                 group=bone_name)
animation.action_slot = action.slots[0]
scene.frame_set(1)
scene.frame_start = 1
scene.frame_end = 61
scene.render.fps = 30
bpy.context.preferences.filepaths.save_version = 0
bpy.ops.wm.save_as_mainfile(filepath=bpy.data.filepath, compress=True,
                            check_existing=False)
print('GUARD_WALK_SAVED', bpy.data.filepath, tuple(action.frame_range),
      len(samples), 'FP' if is_first_person else '3P')
