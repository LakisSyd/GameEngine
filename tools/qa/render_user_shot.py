from pathlib import Path
import bpy
import sys
from mathutils import Vector

root = Path(__file__).resolve().parents[2]
out = root / 'tools' / 'qa' / 'user_shot_before'
if '--outdir' in sys.argv:
    out = Path(sys.argv[sys.argv.index('--outdir') + 1]).resolve()
out.mkdir(parents=True, exist_ok=True)
scene = bpy.context.scene
rig = bpy.data.objects['Armature']
anim = rig.animation_data_create()
for track in anim.nla_tracks:
    track.mute = True
for obj in bpy.data.objects:
    if obj.name in {'Armature.001', 'MaleBaseMesh LowPoly.001', 'magazine'} or obj.name.startswith('WGT_'):
        obj.hide_render = True
    if obj.type == 'EMPTY':
        obj.hide_render = True
scene.render.engine = 'BLENDER_WORKBENCH'
scene.render.resolution_x = 900
scene.render.resolution_y = 900
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = 'PNG'
scene.display.shading.light = 'STUDIO'
scene.display.shading.color_type = 'OBJECT'
scene.display.shading.show_cavity = True
scene.display.shading.cavity_type = 'BOTH'
bpy.data.objects['MaleBaseMesh LowPoly'].color = (0.67, 0.69, 0.72, 1.0)
bpy.data.objects['Rifle_M4_CC0'].color = (0.20, 0.22, 0.25, 1.0)
cam_data = bpy.data.cameras.new('QA Camera')
cam = bpy.data.objects.new('QA Camera', cam_data)
scene.collection.objects.link(cam)
scene.camera = cam
cam_data.type = 'ORTHO'
cam_data.ortho_scale = 3.7
target = Vector((0.25, -2.2, 5.5))
angles = {'quarter': Vector((5.5, -7.0, 5.65)),
          'side': Vector((8.0, -2.2, 5.65))}
for action_name, frames in [('Kevin_Aim_Rifle', [0]),
                            ('Kevin_Aim_Shoot_R', [0, 4, 10, 24])]:
    ac = bpy.data.actions[action_name]
    anim.action = ac
    anim.action_slot = ac.slots[0]
    for frame in frames:
        scene.frame_set(frame)
        bpy.context.view_layer.update()
        for label, position in angles.items():
            cam.location = position
            cam.rotation_euler = (target-position).to_track_quat('-Z','Y').to_euler()
            scene.render.filepath = str(out / f'{action_name}_{frame}_{label}.png')
            bpy.ops.render.render(write_still=True)
            print('RENDERED', scene.render.filepath)
