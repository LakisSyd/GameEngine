import bpy,json,math
from pathlib import Path
from mathutils import Matrix,Vector
root=Path(__file__).resolve().parents[2]
game=bpy.data.objects['Kevin_Game'];scene=bpy.context.scene
scene.frame_set(1);bpy.context.view_layer.update()
# Inspect imported rifle in Blender coordinates before choosing grip.
before=set(bpy.data.objects)
bpy.ops.import_scene.gltf(filepath=str(root/'assets/model/TestRifle.glb'))
objects=[o for o in bpy.data.objects if o not in before]
for o in objects:
    if o.type=='MESH':
        pts=[o.matrix_world@v.co for v in o.data.vertices]
        print('RIFLE',o.name,'min',[min(p[i] for p in pts) for i in range(3)],'max',[max(p[i] for p in pts) for i in range(3)])
# Render character and unmodified rifle side by side for QA.
for o in objects: o.location.x+=0.9
scene.render.engine='BLENDER_WORKBENCH';scene.display.shading.light='STUDIO';scene.display.shading.color_type='SINGLE';scene.display.shading.single_color=(0.65,0.67,0.7);scene.render.resolution_x=1000;scene.render.resolution_y=900;scene.render.resolution_percentage=100
scene.world.color=(0.15,0.15,0.15)
bpy.ops.object.camera_add(location=(3,-5,2.7));camera=bpy.context.object
camera.rotation_euler=(Vector((0,0,1.2))-camera.location).to_track_quat('-Z','Y').to_euler();scene.camera=camera
for p in [(2,-3,4),(-3,-1,3)]:
    bpy.ops.object.light_add(type='AREA',location=p);light=bpy.context.object;light.data.energy=450;light.data.shape='DISK';light.data.size=4
    light.rotation_euler=(Vector((0,0,1))-light.location).to_track_quat('-Z','Y').to_euler()
scene.render.filepath=str(root/'tools/qa/weapon_blender.png');bpy.ops.render.render(write_still=True)


