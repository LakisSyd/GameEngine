"""Check authored melee clips and support-hand motion in either Kevin rig."""
import json
import bpy
from mathutils import Matrix

rig = bpy.data.objects['Armature']
scene = bpy.context.scene
animation = rig.animation_data_create()
initial = {bone.name: bone.matrix_basis.copy() for bone in rig.pose.bones}
clips = {
    'Kevin_Melee_Guard': (1, 25, 49, None),
    'Kevin_Melee_Jab_L': (1, 10, 22, 'Right'),
    'Kevin_Melee_Cross_R': (1, 11, 24, 'Left'),
    'Kevin_Melee_Hook_L': (1, 12, 28, 'Right'),
    'Kevin_Melee_Uppercut_R': (1, 13, 30, 'Left'),
}
report = {}
for name, (start, contact, end, support) in clips.items():
    action = bpy.data.actions[name]
    animation.action = None
    for bone in rig.pose.bones:
        bone.matrix_basis = initial[bone.name]
    animation.action = action
    animation.action_slot = action.slots[0]
    poses = {}
    for frame in (start, contact, end):
        scene.frame_set(frame)
        bpy.context.view_layer.update()
        poses[frame] = {
            side: (rig.matrix_world @ rig.pose.bones[f'mixamorig:{side}Hand'].head).copy()
            for side in ('Left', 'Right')
        }
    seam = max((poses[end][side] - poses[start][side]).length
               for side in ('Left', 'Right'))
    assert seam < 0.015, (name, 'seam', seam)
    values = {'frames': [start, end], 'seam_m': round(seam, 4)}
    if support:
        motion = (poses[contact][support] - poses[start][support]).length
        attacking = 'Right' if support == 'Left' else 'Left'
        attack_motion = (poses[contact][attacking] - poses[start][attacking]).length
        assert motion > 0.018, (name, 'support too still', motion)
        assert attack_motion > motion, (name, 'attack too small', attack_motion, motion)
        values['support_motion_m'] = round(motion, 4)
        values['attack_motion_m'] = round(attack_motion, 4)
    report[name] = values
print('MELEE_VALIDATION', json.dumps(report, ensure_ascii=False))
