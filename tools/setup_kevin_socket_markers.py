"""Add editable weapon socket empties to Kevin's Blender master.

Run once (existing DX_SOCKET_* objects are left untouched):
blender --background Kevin_Main_Switchable.blend --python tools/setup_kevin_socket_markers.py

Use -- --output PATH to create a separate .blend during validation.
"""

import argparse
import json
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Vector

sys.path.insert(0, str(Path(__file__).resolve().parent))
from kevin_socket_bridge import (
    engine_local_to_blender, game_normalization, normalized_transform)


parser = argparse.ArgumentParser()
parser.add_argument('--output', type=Path)
args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else [])
root = Path(__file__).resolve().parents[1]
profile = json.loads((root / 'assets/config/weapons/Kevin_M4.json').read_text())
rig = bpy.data.objects['Armature']
mesh = bpy.data.objects['MaleBaseMesh LowPoly']
gun = bpy.data.objects['Rifle_M4_CC0']
scene = bpy.context.scene
original_frame = scene.frame_current
original_pose_position = rig.data.pose_position
animation = rig.animation_data_create()
original_action = animation.action
original_slot = animation.action_slot
original_mutes = [(track, track.mute) for track in animation.nla_tracks]

for track, _ in original_mutes:
    track.mute = True
rig.data.pose_position = 'POSE'
aim = bpy.data.actions['Kevin_Aim_Rifle']
animation.action = aim
if aim.slots:
    animation.action_slot = next(
        (slot for slot in aim.slots if slot.target_id_type == 'OBJECT'), aim.slots[0])
scene.frame_set(int(round(aim.frame_range[0])))
bpy.context.view_layer.update()

normal = game_normalization(mesh)
evaluated = rig.evaluated_get(bpy.context.evaluated_depsgraph_get())
right_world = rig.matrix_world @ evaluated.pose.bones[profile['right_hand_bone']].matrix
right_game = normalized_transform(normal @ right_world)
gun_game = right_game @ engine_local_to_blender(
    profile['world_position'], profile['world_rotation'], profile['world_scale'])
source_gun = gun.evaluated_get(bpy.context.evaluated_depsgraph_get()).matrix_world.copy()

collection = bpy.data.collections.get('DX Weapon Sockets')
if collection is None:
    collection = bpy.data.collections.new('DX Weapon Sockets')
    scene.collection.children.link(collection)


def add_marker(name, world_matrix, display_type, color):
    marker = bpy.data.objects.get(name)
    if marker is not None:
        print('SOCKET_MARKER_EXISTS', name)
        return marker
    marker = bpy.data.objects.new(name, None)
    collection.objects.link(marker)
    marker.empty_display_type = display_type
    marker.empty_display_size = 0.14
    marker.color = color
    marker.hide_render = True
    marker.parent = gun
    marker.matrix_parent_inverse = source_gun.inverted()
    marker.matrix_world = world_matrix
    bpy.context.view_layer.update()
    error = max(abs(marker.matrix_world[i][j] - world_matrix[i][j])
                for i in range(4) for j in range(4))
    print('SOCKET_MARKER_CREATED', name, 'world_matrix_error', round(error, 6))
    return marker


grip = add_marker('DX_SOCKET_WeaponGrip', source_gun,
                  'ARROWS', (0.1, 0.65, 1.0, 1.0))
grip['dx_weapon_model'] = profile['visual_model']
grip['dx_profile_name'] = profile['profile_name']
grip['dx_right_hand_bone'] = profile['right_hand_bone']
grip['dx_engine_scale'] = float(profile['world_scale'][0])

left_local = engine_local_to_blender(
    profile['left_hand_grip'], profile['left_hand_rotation'])
left_world = normal.inverted() @ gun_game @ left_local
add_marker('DX_SOCKET_LeftHand', left_world,
           'SPHERE', (0.2, 1.0, 0.35, 1.0))

muzzle_local = engine_local_to_blender(profile['muzzle_position'], (0, 0, 0))
muzzle_world = normal.inverted() @ gun_game @ muzzle_local
add_marker('DX_SOCKET_Muzzle', muzzle_world,
           'CIRCLE', (1.0, 0.65, 0.1, 1.0))

animation.action = original_action
if original_action is not None and original_slot is not None:
    animation.action_slot = original_slot
for track, mute in original_mutes:
    track.mute = mute
rig.data.pose_position = original_pose_position
scene.frame_set(original_frame)
bpy.context.view_layer.update()

output = args.output or Path(bpy.data.filepath)
if not output:
    raise RuntimeError('Open Kevin_Main_Switchable.blend before running this script')
output = output.resolve()
output.parent.mkdir(parents=True, exist_ok=True)
bpy.context.preferences.filepaths.save_version = 0
bpy.ops.wm.save_as_mainfile(filepath=str(output), compress=True)
print('SOCKET_MARKER_SAVED', output)
