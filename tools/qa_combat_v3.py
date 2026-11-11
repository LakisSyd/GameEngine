"""Check shipped clips and render their evaluated skin after GLB reimport."""
import bpy
from pathlib import Path
from mathutils import Vector
root=Path(__file__).resolve().parents[1]
out=root/'tools/qa_combat_v3'
out.mkdir(exist_ok=True)
for o in list(bpy.data.objects): bpy.data.objects.remove(o,do_unlink=True)
bpy.context.scene.render.fps=30
bpy.ops.import_scene.gltf(filepath=str(root/'assets/model/Kevin_Combat_Game.glb'))
rig=next(o for o in bpy.data.objects if o.type=='ARMATURE')
assert len(rig.data.bones)==65
scene=bpy.context.scene
scene.render.fps=30
scene.render.engine='BLENDER_WORKBENCH'
scene.render.resolution_x=640;scene.render.resolution_y=640
scene.render.resolution_percentage=100
scene.display.shading.color_type='SINGLE'
scene.display.shading.single_color=(0.65,0.7,0.77)
scene.display.shading.show_cavity=True
data=bpy.data.cameras.new('QA');cam=bpy.data.objects.new('QA',data)
scene.collection.objects.link(cam);scene.camera=cam
data.type='ORTHO';data.ortho_scale=2.8
cam.location=(3,-4,2)
cam.rotation_euler=(Vector((0,0,1.1))-cam.location).to_track_quat('-Z','Y').to_euler()
required=['Kevin_Melee_Guard_Walk','Kevin_Pistol_Idle','Kevin_Pistol_Walk','Kevin_Pistol_Shot',
          'Kevin_Crouch_Idle','Kevin_Crouch_Walk','Kevin_Crouch_Pistol_Idle',
          'Kevin_Crouch_Pistol_Walk','Kevin_Crouch_Rifle_Idle','Kevin_Crouch_Rifle_Walk']
for name in required:
    action=bpy.data.actions.get(name)
    assert action,name
    rig.animation_data.action=action;rig.animation_data.action_slot=action.slots[0]
    poses=[]
    for frame in action.frame_range:
        scene.frame_set(int(frame));bpy.context.view_layer.update()
        poses.append({b.name:b.matrix.copy() for b in rig.pose.bones})
    seam=max((poses[0][n].translation-poses[1][n].translation).length for n in poses[0])
    assert seam < 0.001,(name,seam)
    print('CLIP_OK',name,'SEAM',seam)
    scene.frame_set(4 if name.endswith('Shot') else 16)
    scene.render.filepath=str(out/(name+'.png'))
    bpy.ops.render.render(write_still=True)
print('COMBAT_V3_QA_OK')
