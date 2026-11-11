import bpy
from mathutils import Matrix, Vector
from pathlib import Path
root=Path(__file__).resolve().parents[2]
rig=bpy.data.objects['Kevin_Game'];ac=bpy.data.actions['Kevin_Aim_Rifle'];rig.animation_data.action=ac;rig.animation_data.action_slot=ac.slots[0];bpy.context.scene.frame_set(0)
before=set(bpy.data.objects);bpy.ops.import_scene.gltf(filepath=str(root/'assets/model/Rifle_M4_CC0.glb'));gun=next(o for o in set(bpy.data.objects)-before if o.type=='MESH')
body=bpy.data.objects['Kevin_Body'];body.color=(.68,.7,.72,1);gun.color=(.15,.18,.22,1)
sc=bpy.context.scene;sc.render.engine='BLENDER_WORKBENCH';sc.render.resolution_x=1100;sc.render.resolution_y=900;sc.render.resolution_percentage=100;sc.display.shading.light='STUDIO';sc.display.shading.color_type='OBJECT';sc.display.shading.show_cavity=True;sc.display.shading.cavity_type='BOTH'
camd=bpy.data.cameras.new('QA');cam=bpy.data.objects.new('QA',camd);sc.collection.objects.link(cam);sc.camera=cam;camd.type='ORTHO';camd.ortho_scale=1.55
target=Vector((0,-.4,1.82))
for label,scale in [('authored',(-1.6687,-.7436,.8412)),('uniform',(-.8,-.8,.8))]:
 gun.matrix_world=Matrix(((scale[0],0,0,-.096),(0,scale[1],0,-.339),(0,0,scale[2],1.8903),(0,0,0,1)))
 for view,loc in [('quarter',Vector((2.1,-3.1,2.5))),('side',Vector((2.8,-.4,1.95)))]:
  cam.location=loc;cam.rotation_euler=(target-loc).to_track_quat('-Z','Y').to_euler();sc.render.filepath=str(root/f'tools/qa/grip_{label}_{view}.png');bpy.ops.render.render(write_still=True)
