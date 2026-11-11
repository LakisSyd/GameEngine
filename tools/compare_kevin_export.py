import bpy
import numpy as np
from pathlib import Path
from mathutils import Matrix, Vector

root=Path(__file__).resolve().parents[1]
rig=bpy.data.objects['Armature'];mesh=bpy.data.objects['MaleBaseMesh LowPoly'];scene=bpy.context.scene
if bpy.context.object and bpy.context.object.mode!='OBJECT':bpy.ops.object.mode_set(mode='OBJECT')
anim=rig.animation_data_create()
for t in anim.nla_tracks:t.mute=True
rig.data.pose_position='REST';bpy.context.view_layer.update()
world=mesh.matrix_world.copy();points=[world@v.co for v in mesh.data.vertices]
height=max(p.z for p in points)-min(p.z for p in points)
factor=2.2/height
center=Vector(((max(p.x for p in points)+min(p.x for p in points))/2,
               (max(p.y for p in points)+min(p.y for p in points))/2,min(p.z for p in points)))
normal=Matrix.Scale(factor,4)@Matrix.Translation(-center)
print('NORMAL',factor,list(center))
source_rest_points=np.array([tuple(normal@world@v.co) for v in mesh.data.vertices])
source_rest_bones={n:(normal@rig.matrix_world@rig.data.bones[n].matrix_local).copy() for n in ('mixamorig:Hips','mixamorig:Spine2','mixamorig:LeftHand','mixamorig:RightHand','mixamorig:LeftFoot')}
source_tails={n:(normal@rig.matrix_world@rig.data.bones[n].tail_local).copy() for n in source_rest_bones}
source_groups=[g.name for g in mesh.vertex_groups]
source_weight_counts=np.array([len(v.groups) for v in mesh.data.vertices])
rig.data.pose_position='POSE'
source_points={};source_bones={}
for name in ('Kevin_Idle','Kevin_Aim_Rifle'):
    anim.action=None
    for pb in rig.pose.bones:pb.matrix_basis=Matrix.Identity(4)
    action=bpy.data.actions[name];anim.action=action
    anim.action_slot=next((s for s in action.slots if s.target_id_type=='OBJECT'),action.slots[0])
    scene.frame_set(1);bpy.context.view_layer.update()
    ev=mesh.evaluated_get(bpy.context.evaluated_depsgraph_get())
    source_points[name]=np.array([tuple(normal@ev.matrix_world@v.co) for v in ev.data.vertices])
    evrig=rig.evaluated_get(bpy.context.evaluated_depsgraph_get())
    source_bones[name]={n:(normal@rig.matrix_world@evrig.pose.bones[n].matrix).copy() for n in ('mixamorig:Hips','mixamorig:Spine2','mixamorig:LeftHand','mixamorig:RightHand','mixamorig:LeftFoot')}

bpy.ops.wm.open_mainfile(filepath=str(root/'assets/model/Kevin_Combat_Game.blend'))
rig=bpy.data.objects['Kevin_Game'];mesh=bpy.data.objects['Kevin_Body'];scene=bpy.context.scene
game_rest_points=np.array([tuple(mesh.matrix_world@v.co) for v in mesh.data.vertices])
rest_errors=np.linalg.norm(game_rest_points-source_rest_points,axis=1)
print('REST_VERTEX_ERROR',rest_errors.mean(),rest_errors.max())
print('GROUP_MATCH',source_groups==[g.name for g in mesh.vertex_groups],len(source_groups),len(mesh.vertex_groups))
print('GROUP_NAMES',source_groups[:8],[g.name for g in mesh.vertex_groups][:10])
print('WEIGHT_COUNT_DIFF',int(np.count_nonzero(source_weight_counts!=np.array([len(v.groups) for v in mesh.data.vertices]))))
for n,m in source_rest_bones.items():
    gm=rig.data.bones[n].matrix_local
    print('REST_BONE',n,'src_head',tuple(round(v,4) for v in m.translation),'game_head',tuple(round(v,4) for v in gm.translation),
          'orientation_difference_deg',round(m.decompose()[1].rotation_difference(gm.decompose()[1]).angle*180/np.pi,3),
          'src_scale',tuple(round(v,5) for v in m.decompose()[2]),'game_scale',tuple(round(v,5) for v in gm.decompose()[2]))
    src_y=m.decompose()[1]@Vector((0,1,0))
    src_tail_dir=(source_tails[n]-m.translation).normalized()
    game_y=gm.decompose()[1]@Vector((0,1,0))
    print(' DIR',n,'src_y',tuple(round(v,3) for v in src_y),'src_tail_dir',tuple(round(v,3) for v in src_tail_dir),'game_y',tuple(round(v,3) for v in game_y))
for name in ('Kevin_Idle','Kevin_Aim_Rifle'):
    action=bpy.data.actions[name]
    rig.animation_data.action=action;rig.animation_data.action_slot=action.slots[0]
    scene.frame_set(1);bpy.context.view_layer.update()
    ev=mesh.evaluated_get(bpy.context.evaluated_depsgraph_get())
    game_points=np.array([tuple(ev.matrix_world@v.co) for v in ev.data.vertices])
    errors=np.linalg.norm(game_points-source_points[name],axis=1)
    print('COMPARE',name,'vertices',len(game_points),'mean',round(float(errors.mean()),6),'p95',round(float(np.percentile(errors,95)),6),'max',round(float(errors.max()),6))
    for i in np.argsort(errors)[-5:]:
        print(' V',i,'err',round(float(errors[i]),6),'source',source_points[name][i].round(4),'game',game_points[i].round(4))
    for n,m in source_bones[name].items():
        gm=rig.pose.bones[n].matrix
        print(' B',n,'src_head',tuple(round(v,4) for v in m.translation),'game_head',tuple(round(v,4) for v in gm.translation),
              'orientation_difference_deg',round(m.decompose()[1].rotation_difference(gm.decompose()[1]).angle*180/np.pi,3),
              'src_scale',tuple(round(v,5) for v in m.decompose()[2]),'game_scale',tuple(round(v,5) for v in gm.decompose()[2]))
