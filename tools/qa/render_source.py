import bpy
from mathutils import Vector
from pathlib import Path
scene=bpy.context.scene
rig=bpy.data.objects['Armature'];mesh=bpy.data.objects['MaleBaseMesh LowPoly']
print('SOURCE FRAME',scene.frame_current)
for o in bpy.data.objects:
    o.hide_render=o not in (rig,mesh)
scene.render.engine='BLENDER_WORKBENCH';scene.display.shading.light='STUDIO';scene.display.shading.color_type='SINGLE';scene.display.shading.single_color=(.65,.67,.7)
scene.render.resolution_x=800;scene.render.resolution_y=800;scene.render.resolution_percentage=100
bpy.context.view_layer.update();e=mesh.evaluated_get(bpy.context.evaluated_depsgraph_get());pts=[e.matrix_world@v.co for v in e.data.vertices]
center=sum(pts,Vector())/len(pts);size=max(p.z for p in pts)-min(p.z for p in pts)
bpy.ops.object.camera_add(location=center+Vector((1.3,-2,0.6))*size);camera=bpy.context.object
camera.rotation_euler=(center-camera.location).to_track_quat('-Z','Y').to_euler();scene.camera=camera
scene.render.filepath=str(Path(__file__).parent/'weapon_source.png');bpy.ops.render.render(write_still=True)
