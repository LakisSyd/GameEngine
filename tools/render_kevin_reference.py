import bpy
from pathlib import Path
from mathutils import Vector

output = Path(__file__).resolve().parent / 'qa_kevin'
output.mkdir(exist_ok=True)

scene=bpy.context.scene
scene.render.engine='BLENDER_WORKBENCH'
scene.render.resolution_x=1024
scene.render.resolution_y=1024
scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG'
scene.render.film_transparent=False
scene.display.shading.light='STUDIO'
scene.display.shading.color_type='SINGLE'
scene.display.shading.single_color=(0.67,0.72,0.78)
scene.display.shading.show_shadows=True
scene.display.shading.show_cavity=True
scene.display.shading.cavity_type='BOTH'
scene.display.shading.background_type='WORLD'
scene.world.color=(0.18,0.20,0.23)

def point(obj, target):
    obj.rotation_euler=(Vector(target)-obj.location).to_track_quat('-Z','Y').to_euler()

shots=[
    ('Kevin_Aim_Rifle',1,'quarter',(3.5,-4.5,2.3)),
    ('Kevin_Idle',1,'quarter',(3.5,-4.5,2.3)),
    ('Kevin_Rifle_Idle',1,'front',(0,-5.5,2.1)),
    ('Kevin_Rifle_Idle',1,'quarter',(3.5,-4.5,2.3)),
    ('Kevin_Rifle_Idle',16,'quarter',(3.5,-4.5,2.3)),
    ('Kevin_Rifle_Gun',1,'quarter',(3.5,-4.5,2.3)),
    ('Kevin_Rifle_Gun',5,'quarter',(3.5,-4.5,2.3)),
    ('Kevin_Rifle_Gun',12,'quarter',(3.5,-4.5,2.3)),
]
for action_name, frame, angle, pos in shots:
    camdata=bpy.data.cameras.new('QA_Camera_'+angle)
    camera=bpy.data.objects.new('QA_Camera_'+angle,camdata)
    scene.collection.objects.link(camera)
    camera.location=pos
    point(camera,(0,0,1.15))
    camdata.type='ORTHO'
    camdata.ortho_scale=3.0
    scene.camera=camera
    lightdata=bpy.data.lights.new('QA_Key_'+angle,'AREA')
    light=bpy.data.objects.new('QA_Key_'+angle,lightdata)
    scene.collection.objects.link(light)
    light.location=(3,-4,6)
    lightdata.energy=1200
    lightdata.shape='DISK'
    lightdata.size=5
    point(light,(0,0,1))
    arm=bpy.data.objects['Kevin_Game']
    act=bpy.data.actions.get(action_name)
    arm.animation_data.action=act
    arm.animation_data.action_slot=act.slots[0]
    scene.frame_set(frame)
    scene.render.filepath=str(output / (action_name+'_'+str(frame)+'_'+angle+'.png'))
    bpy.ops.render.render(write_still=True)
    bpy.data.objects.remove(light,do_unlink=True)
    bpy.data.objects.remove(camera,do_unlink=True)
