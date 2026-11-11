"""Validate the authored or exported guarded-walk cycle in Blender."""

import json
import sys
from pathlib import Path

import bpy

if '--' in sys.argv:
    glb = Path(sys.argv[sys.argv.index('--') + 1])
    bpy.ops.import_scene.gltf(filepath=str(glb))

rig = next(obj for obj in bpy.data.objects if obj.type == 'ARMATURE'
           and obj.data.bones.get('mixamorig:LeftFoot'))
action = bpy.data.actions['Kevin_Melee_Guard_Walk']
animation = rig.animation_data_create()
for track in animation.nla_tracks:
    track.mute = True
animation.action = action
animation.action_slot = action.slots[0]
scene = bpy.context.scene
poses = {}
for frame in (1, 16, 31, 46, 61):
    scene.frame_set(frame)
    bpy.context.view_layer.update()
    poses[frame] = {
        name: (rig.matrix_world @ rig.pose.bones[name].head).copy()
        for name in ('mixamorig:LeftFoot', 'mixamorig:RightFoot',
                     'mixamorig:LeftHand', 'mixamorig:RightHand',
                     'mixamorig:Head')
    }

seam = max((poses[61][name] - poses[1][name]).length for name in poses[1])
left_step = max((poses[frame]['mixamorig:LeftFoot'] -
                 poses[1]['mixamorig:LeftFoot']).length
                for frame in (16, 31, 46))
right_step = max((poses[frame]['mixamorig:RightFoot'] -
                  poses[1]['mixamorig:RightFoot']).length
                 for frame in (16, 31, 46))
hand_motion = {side: max((poses[frame][f'mixamorig:{side}Hand'] -
                          poses[1][f'mixamorig:{side}Hand']).length
                         for frame in (16, 31, 46))
               for side in ('Left', 'Right')}
assert seam < 0.03, ('loop seam', seam)
assert left_step > 0.08 and right_step > 0.08, ('feet too still', left_step, right_step)
assert min(hand_motion.values()) > 0.01, ('hands too still', hand_motion)
print('GUARD_WALK_VALIDATION', json.dumps({
    'frames': [1, 61], 'seam_m': round(seam, 5),
    'left_step_m': round(left_step, 4),
    'right_step_m': round(right_step, 4),
    'hand_motion_m': {k: round(v, 4) for k, v in hand_motion.items()}
}))
