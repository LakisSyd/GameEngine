"""Reimport a Kevin GLB and check the shipped melee animation clips."""
import json
import sys
from pathlib import Path
import bpy

path = Path(sys.argv[sys.argv.index('--') + 1])
bpy.ops.import_scene.gltf(filepath=str(path))
rig = next(obj for obj in bpy.data.objects if obj.type == 'ARMATURE'
           and len(obj.data.bones) == 65)
mesh = max((obj for obj in bpy.data.objects if obj.type == 'MESH'),
           key=lambda obj: len(obj.data.vertices))
clips = {
    'Kevin_Melee_Guard': (1, 25, 49, None),
    'Kevin_Melee_Jab_L': (1, 10, 22, 'Right'),
    'Kevin_Melee_Cross_R': (1, 11, 24, 'Left'),
    'Kevin_Melee_Hook_L': (1, 12, 28, 'Right'),
    'Kevin_Melee_Uppercut_R': (1, 13, 30, 'Left'),
}
report = {'file': str(path), 'bones': len(rig.data.bones),
          'vertices': len(mesh.data.vertices), 'clips': {}}
assert len(mesh.data.vertices) > 20000
animation = rig.animation_data_create()
for name, (start, contact, end, support) in clips.items():
    action = bpy.data.actions.get(name)
    assert action, f'{name} missing in {path}'
    animation.action = action
    animation.action_slot = action.slots[0]
    poses = {}
    for frame in (start, contact, end):
        bpy.context.scene.frame_set(frame)
        bpy.context.view_layer.update()
        poses[frame] = {
            side: (rig.matrix_world @ rig.pose.bones[f'mixamorig:{side}Hand'].head).copy()
            for side in ('Left', 'Right')
        }
    seam = max((poses[end][side] - poses[start][side]).length
               for side in ('Left', 'Right'))
    assert seam < 0.015, (name, 'seam', seam)
    entry = {'frames': [start, end], 'seam_m': round(seam, 5)}
    if support:
        distance = (poses[contact][support] - poses[start][support]).length
        assert distance > 0.005, (name, 'support hand still', distance)
        entry['support_motion_m'] = round(distance, 5)
    report['clips'][name] = entry
print('MELEE_GLB_VALIDATION', json.dumps(report, ensure_ascii=False))
