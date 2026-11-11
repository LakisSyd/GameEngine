"""Refine Kevin_Aim_Shoot_R while preserving the user's authored rifle stance.

Run on Kevin_Main_Switchable.blend. Pass --output <path> after -- to save a
candidate elsewhere; omit it only after visually checking the candidate.
The previous shot Action is kept in the blend under a dated backup name.
"""
import bpy
import math
import sys
from pathlib import Path
from mathutils import Matrix, Vector

rig = bpy.data.objects['Armature']
scene = bpy.context.scene
anim = rig.animation_data_create()
for track in anim.nla_tracks:
    track.mute = True

stance = bpy.data.actions['Kevin_Aim_Rifle']
old_shot = bpy.data.actions['Kevin_Aim_Shoot_R']
backup_name = 'Kevin_Aim_Shoot_R_before_codex_20260927'
if bpy.data.actions.get(backup_name):
    raise RuntimeError(f'Backup Action already exists: {backup_name}')
old_shot.name = backup_name
old_shot.use_fake_user = True
shot = stance.copy()
shot.name = 'Kevin_Aim_Shoot_R'
shot.use_fake_user = True
anim.action = shot
anim.action_slot = shot.slots[0]
scene.frame_set(0)
bpy.context.view_layer.update()

controls = ['CTRL_Spine_01', 'CTRL_Chest', 'CTRL_Shoulder.R',
            'CTRL_Shoulder.L', 'CTRL_Head', 'CTRL_Hand_IK.R']
base = {}
for name in controls:
    pb = rig.pose.bones[name]
    base[name] = (pb.location.copy(), pb.rotation_euler.copy(), pb.scale.copy())

def restore_controls():
    for name in controls:
        pb = rig.pose.bones[name]
        loc, rot, scale = base[name]
        pb.location = loc.copy()
        pb.rotation_euler = rot.copy()
        pb.scale = scale.copy()
    bpy.context.view_layer.update()

# The source file is 60 fps. A fast recoil impulse in the first 3 frames and a
# 0.38 s settling tail read clearly without freezing the locomotion below Spine.
poses = [
    (0, 0.00),
    (2, 0.35),
    (4, 1.00),
    (6, 0.75),
    (10, 0.34),
    (15, -0.10),
    (20, 0.02),
    (24, 0.00),
]
for frame, amount in poses:
    scene.frame_set(frame)
    restore_controls()
    # Upper body catches the hit, then settles; the head resists a little.
    rig.pose.bones['CTRL_Spine_01'].rotation_euler.x -= math.radians(1.1) * amount
    rig.pose.bones['CTRL_Chest'].rotation_euler.x -= math.radians(2.0) * amount
    rig.pose.bones['CTRL_Chest'].rotation_euler.z += math.radians(0.5) * amount
    rig.pose.bones['CTRL_Shoulder.R'].rotation_euler.x += math.radians(1.6) * amount
    rig.pose.bones['CTRL_Shoulder.L'].rotation_euler.x += math.radians(0.4) * amount
    rig.pose.bones['CTRL_Head'].rotation_euler.x += math.radians(1.0) * amount
    bpy.context.view_layer.update()

    # Weapon is parented to CTRL_Hand_IK.R. The left IK controller's Child Of
    # follows mixamorig:RightHand, so the support hand follows the same motion.
    hand = rig.pose.bones['CTRL_Hand_IK.R']
    hand_world = rig.matrix_world @ hand.matrix
    pivot = hand_world.translation.copy()
    pitch = Matrix.Rotation(math.radians(-3.4) * amount, 4, 'X')
    moved = (Matrix.Translation(Vector((0.0, 0.13, 0.045)) * amount)
             @ Matrix.Translation(pivot) @ pitch
             @ Matrix.Translation(-pivot) @ hand_world)
    hand.matrix = rig.matrix_world.inverted() @ moved
    bpy.context.view_layer.update()

    for name in controls:
        pb = rig.pose.bones[name]
        for prop in ('location', 'rotation_euler', 'scale'):
            pb.keyframe_insert(data_path=prop, frame=frame, group=name)

# Avoid Bezier handle overshoot beyond the recoil and the exact stance seam.
for layer in shot.layers:
    for strip in layer.strips:
        for slot in shot.slots:
            bag = strip.channelbag(slot)
            if not bag:
                continue
            for curve in bag.fcurves:
                for key in curve.keyframe_points:
                    key.interpolation = 'BEZIER'
                    key.handle_left_type = 'AUTO_CLAMPED'
                    key.handle_right_type = 'AUTO_CLAMPED'

scene.frame_start = 0
scene.frame_end = 24
scene.render.fps = 60
anim.action = shot
anim.action_slot = shot.slots[0]
scene.frame_set(0)
bpy.context.view_layer.update()

out = Path(bpy.data.filepath)
if '--output' in sys.argv:
    out = Path(sys.argv[sys.argv.index('--output') + 1]).resolve()
out.parent.mkdir(parents=True, exist_ok=True)
bpy.context.preferences.filepaths.save_version = 0
bpy.ops.wm.save_as_mainfile(filepath=str(out), compress=True)
print('SAVED_SHOT', out, 'range', tuple(shot.frame_range), 'backup', backup_name)
