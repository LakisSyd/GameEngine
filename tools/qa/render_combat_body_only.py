import bpy
from mathutils import Vector
rig=bpy.data.objects['Kevin_Game']; rig.animation_data.action=bpy.data.actions['Kevin_Aim_Rifle']; bpy.context.scene.frame_set(0)
sc=bpy.context.scene;sc.render.engine='BLENDER_WORKBENCH';sc.render.resolution_x=900;sc.render.resolution_y=900;sc.render.resolution_percentage=100
sc.display.shading.light='STUDIO';sc.display.shading.color_type='MATERIAL';sc.display.shading.background_type='WORLD';sc.world.color=(.4,.4,.4)
camd=bpy.data.cameras.new('QA');cam=bpy.data.objects.new('QA',camd);sc.collection.objects.link(cam);sc.camera=cam;camd.type='ORTHO';camd.ortho_scale=2.3
cam.location=(2.4,-3.8,2.6);target=Vector((0,-.5,1.5));cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler()
sc.render.filepath='C:/Users/karin/OneDrive/Desktop/DX_FrameWork/DX_FrameWork/tools/qa/kevin_combat_body_only.png';bpy.ops.render.render(write_still=True)
