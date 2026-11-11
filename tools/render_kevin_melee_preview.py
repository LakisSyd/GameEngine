"""Render contact frames from an authoring Kevin file for animation QA."""
import bpy
import sys
from pathlib import Path
from mathutils import Vector, Matrix

out = Path(sys.argv[sys.argv.index('--')+1]) if '--' in sys.argv else Path('melee_preview')
out.mkdir(parents=True, exist_ok=True)
scene=bpy.context.scene
scene.render.engine='BLENDER_WORKBENCH'
scene.render.resolution_x=640
scene.render.resolution_y=640
scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG'
scene.display.shading.light='STUDIO'
scene.display.shading.color_type='SINGLE'
scene.display.shading.single_color=(0.67,0.71,0.77)
scene.display.shading.show_shadows=True
scene.display.shading.show_cavity=True
scene.display.shading.cavity_type='BOTH'
scene.display.shading.background_type='WORLD'
scene.world.color=(0.15,0.17,0.2)
rig=bpy.data.objects['Armature']
rig.hide_render=True
mesh=bpy.data.objects['MaleBaseMesh LowPoly']
mesh.hide_render=False
for obj in bpy.data.objects:
    if obj.type=='MESH' and obj!=mesh: obj.hide_render=True
    if obj.type=='ARMATURE': obj.hide_render=True
anim=rig.animation_data_create()
for track in anim.nla_tracks:track.mute=True
def aim(cam,target): cam.rotation_euler=(Vector(target)-cam.location).to_track_quat('-Z','Y').to_euler()
cams={
 'front':((0,-13,5.1),(0,0,3.05),8.0),
 'quarter':((9,-12,6.1),(0,0,3.1),8.0),
 'side':((13,0,5.1),(0,0,3.05),8.0),
 'fp':((-0.24,0.8,5.25),(-0.24,-5,4.35),None),
}
shots=[('Kevin_Punch.1',[0,20]),('Kevin_Idle',[1])]
shots += [(name,frames) for name,frames in (
 ('Kevin_Melee_Guard',[1,13]),
 ('Kevin_Melee_Guard_Walk',[1,16,31,46]),
 ('Kevin_Melee_Jab_L',[1,10]),
 ('Kevin_Melee_Cross_R',[1,11]),
 ('Kevin_Melee_Hook_L',[1,12]),
 ('Kevin_Melee_Uppercut_R',[1,13]),
) if name in bpy.data.actions]
for action_name,frames in shots:
 action=bpy.data.actions[action_name]
 anim.action=action
 if action.slots: anim.action_slot=next((s for s in action.slots if s.target_id_type=='OBJECT'),action.slots[0])
 for frame in frames:
  scene.frame_set(frame)
  for label,(location,target,scale) in cams.items():
   if label=='fp' and action_name=='Kevin_Idle':continue
   data=bpy.data.cameras.new('QA')
   cam=bpy.data.objects.new('QA',data)
   scene.collection.objects.link(cam)
   cam.location=location
   aim(cam,target)
   if scale:
    data.type='ORTHO';data.ortho_scale=scale
   else:data.lens=32
   scene.camera=cam
   scene.render.filepath=str(out/f'{action_name}_{frame}_{label}.png')
   bpy.ops.render.render(write_still=True)
   bpy.data.objects.remove(cam,do_unlink=True)
   bpy.data.cameras.remove(data)
