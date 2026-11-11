"""Reimport the shipped GLB and check its skeleton, clips, loop seam and skin."""
import bpy
import math
from pathlib import Path
from mathutils import Vector

root=Path(__file__).resolve().parents[1]
scene=bpy.context.scene
for obj in list(bpy.data.objects):
    bpy.data.objects.remove(obj,do_unlink=True)
scene.render.fps=30
bpy.ops.import_scene.gltf(filepath=str(root/'assets/model/Kevin_Combat_Game.glb'))
arm=next(o for o in bpy.data.objects if o.type=='ARMATURE')
mesh=next(o for o in bpy.data.objects if o.type=='MESH' and len(o.data.vertices)>1000)
print('IMPORTED',arm.name,len(arm.data.bones),mesh.name,len(mesh.data.vertices))
print('ACTIONS',[(a.name,tuple(round(v,2) for v in a.frame_range)) for a in bpy.data.actions])
assert len(arm.data.bones)==65
for n in ('Kevin_Idle','Kevin_Walk','Kevin_Run','Kevin_Walk_R_I',
          'Kevin_Aim_Rifle','Kevin_Rifle_Idle','Kevin_Rifle_Gun'):
    assert bpy.data.actions.get(n),n

def activate(name,frame):
    action=bpy.data.actions[name]
    arm.animation_data.action=action
    arm.animation_data.action_slot=action.slots[0]
    scene.frame_set(frame)
    bpy.context.view_layer.update()

poses={}
for name,frame in [('Kevin_Rifle_Idle',1),('Kevin_Rifle_Idle',61),
                   ('Kevin_Rifle_Gun',1),('Kevin_Rifle_Gun',5),('Kevin_Rifle_Gun',16)]:
    activate(name,frame)
    poses[(name,frame)]={p.name:(arm.matrix_world@p.matrix).copy() for p in arm.pose.bones}
    ev=mesh.evaluated_get(bpy.context.evaluated_depsgraph_get())
    pts=[ev.matrix_world@v.co for v in ev.data.vertices]
    print('POSE_BOUNDS',name,frame,'min',tuple(round(min(p[i] for p in pts),4) for i in range(3)),
          'max',tuple(round(max(p[i] for p in pts),4) for i in range(3)))
base=poses[('Kevin_Rifle_Idle',1)]
for label,key in [('idle_loop',('Kevin_Rifle_Idle',61)),
                  ('gun_start',('Kevin_Rifle_Gun',1)),('gun_end',('Kevin_Rifle_Gun',16))]:
    delta=max((base[n].translation-poses[key][n].translation).length for n in base)
    rot=max(base[n].to_quaternion().rotation_difference(poses[key][n].to_quaternion()).angle for n in base)
    print('SEAM',label,'max_pos_m',round(delta,8),'max_rot_deg',round(math.degrees(rot),6))
    assert delta<0.0001 and rot<0.0001

hips=[]
for frame in range(1,49):
    activate('Kevin_Run',frame)
    h=arm.matrix_world@arm.pose.bones['mixamorig:Hips'].head
    hips.append(h)
print('RUN_HIPS_HORIZONTAL_MEAN',round(sum(p.x for p in hips)/len(hips),5),
      round(sum(p.z for p in hips)/len(hips),5))

scene.render.engine='BLENDER_WORKBENCH'
scene.display.shading.light='STUDIO'
scene.display.shading.color_type='SINGLE'
scene.display.shading.single_color=(0.67,0.72,0.78)
scene.display.shading.background_type='WORLD'
scene.world.color=(0.18,0.20,0.23)
scene.render.resolution_x=900
scene.render.resolution_y=900
scene.render.resolution_percentage=100
camera_data=bpy.data.cameras.new('QA_Camera')
camera=bpy.data.objects.new('QA_Camera',camera_data)
scene.collection.objects.link(camera)
camera_data.type='ORTHO'
camera_data.ortho_scale=3.0
camera.location=(3.5,-4.5,2.3)
camera.rotation_euler=(Vector((0,0,1.15))-camera.location).to_track_quat('-Z','Y').to_euler()
scene.camera=camera
for name,frame in [('Kevin_Rifle_Idle',1),('Kevin_Rifle_Gun',5)]:
    activate(name,frame)
    scene.render.filepath=str(root/f'tools/qa_kevin/imported_{name}_{frame}.png')
    bpy.ops.render.render(write_still=True)
