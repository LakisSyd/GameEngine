"""Render the authored one-shot with the M4 attached to the right hand."""
import bpy
from math import radians
from pathlib import Path
from mathutils import Matrix, Vector

root=Path(__file__).resolve().parents[1]
out=root/'tools/qa_kevin'
out.mkdir(exist_ok=True)
scene=bpy.context.scene
arm=bpy.data.objects['Kevin_Game']
idle=bpy.data.actions['Kevin_Rifle_Idle']
shot=bpy.data.actions['Kevin_Rifle_Gun']
arm.animation_data.action=idle
arm.animation_data.action_slot=idle.slots[0]
scene.frame_set(1)
bpy.context.view_layer.update()
before=set(bpy.data.objects)
bpy.ops.import_scene.gltf(filepath=str(root/'assets/model/Rifle_M4_CC0.glb'))
gun=next(o for o in bpy.data.objects if o not in before and o.type=='MESH')
hand=arm.pose.bones['mixamorig:RightHand']
bone_world=arm.matrix_world@hand.matrix
world_rotation=Matrix.Rotation(radians(195.0),4,'Z')
grip_rotation=(bone_world.to_3x3().normalized().inverted()@world_rotation.to_3x3()).to_4x4()
grip=Matrix.Translation(Vector((0.03,0.08,0)))@grip_rotation

scene.render.engine='BLENDER_WORKBENCH'
scene.display.shading.light='STUDIO'
scene.display.shading.color_type='MATERIAL'
scene.display.shading.background_type='WORLD'
scene.world.color=(0.35,0.35,0.35)
scene.render.resolution_x=1100
scene.render.resolution_y=1000
scene.render.resolution_percentage=100
for material in bpy.data.objects['Kevin_Body'].data.materials:
    material.diffuse_color=(0.38,0.39,0.40,1.0)
camera_data=bpy.data.cameras.new('QA_Camera')
camera=bpy.data.objects.new('QA_Camera',camera_data)
scene.collection.objects.link(camera)
camera_data.type='ORTHO'
camera_data.ortho_scale=1.65
camera.location=(2.4,-3.8,2.6)
camera.rotation_euler=(Vector((0,-0.5,1.75))-camera.location).to_track_quat('-Z','Y').to_euler()
scene.camera=camera

for label,action,frame in [('idle',idle,1),('gun',shot,1),('gun',shot,3),('gun',shot,5),
                           ('gun',shot,7),('gun',shot,12),('gun',shot,16)]:
    arm.animation_data.action=action
    arm.animation_data.action_slot=action.slots[0]
    scene.frame_set(frame)
    bpy.context.view_layer.update()
    gun.matrix_world=arm.matrix_world@arm.pose.bones['mixamorig:RightHand'].matrix@grip
    bpy.context.view_layer.update()
    muzzle=gun.matrix_world@Vector((0,0.607,0.11))
    butt=gun.matrix_world@Vector((0,-0.293,0.13))
    print('SHOT_QA',label,frame,'muzzle',tuple(round(v,4) for v in muzzle),
          'butt',tuple(round(v,4) for v in butt))
    scene.render.filepath=str(out/f'combat_{label}_{frame:02d}.png')
    bpy.ops.render.render(write_still=True)
