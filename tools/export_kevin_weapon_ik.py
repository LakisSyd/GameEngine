"""Export Kevin's evaluated deform poses; never overwrite the authoring file.
Run: blender --background Kevin_Main_Switchable.blend --python tools/export_kevin_weapon_ik.py
"""
import bpy, json
from pathlib import Path
from mathutils import Matrix, Vector
ROOT = Path(__file__).resolve().parents[1]
rig = bpy.data.objects['Armature']
mesh = bpy.data.objects['MaleBaseMesh LowPoly']
scene = bpy.context.scene
if bpy.context.object and bpy.context.object.mode != 'OBJECT': bpy.ops.object.mode_set(mode='OBJECT')
anim = rig.animation_data_create()
for t in anim.nla_tracks: t.mute = True
names = [b.name for b in rig.data.bones if b.use_deform]
def depth(name):
    b=rig.data.bones[name]; d=0
    while b.parent: d+=1; b=b.parent
    return d
names.sort(key=depth)
# Reset unkeyed controls when switching actions (no pose leaking between clips).
def action_set(name):
    anim.action = None
    for pb in rig.pose.bones: pb.matrix_basis = Matrix.Identity(4)
    action=bpy.data.actions[name]
    anim.action=action
    slots=list(action.slots)
    if slots: anim.action_slot=next((s for s in slots if s.target_id_type=='OBJECT'),slots[0])
    return action
samples={}
for name in ('Kevin_Idle','Kevin_Walk','Kevin_Run','Kevin_Aim_Rifle'):
    action=action_set(name)
    lo,hi=map(lambda x:int(round(x)),action.frame_range)
    print('SAMPLE',name,lo,hi,'slot',anim.action_slot.identifier if anim.action_slot else None)
    frames={}
    for frame in range(lo,hi+1):
        scene.frame_set(frame); bpy.context.view_layer.update()
        evaluated=rig.evaluated_get(bpy.context.evaluated_depsgraph_get())
        frames[frame]={n:rig.matrix_world @ evaluated.pose.bones[n].matrix for n in names}
    samples[name]=frames
# Source mesh in REST, for stable bind geometry.
rig.data.pose_position='REST'; bpy.context.view_layer.update()
world=mesh.matrix_world.copy()
points=[world@v.co for v in mesh.data.vertices]
height=max(p.z for p in points)-min(p.z for p in points)
factor=2.2/height
center=Vector(((max(p.x for p in points)+min(p.x for p in points))/2,
               (max(p.y for p in points)+min(p.y for p in points))/2,min(p.z for p in points)))
normal=Matrix.Scale(factor,4) @ Matrix.Translation(-center)
print('NORMAL',factor,list(center))
# EditBone matrices contain rotation and length, not the source object's scale.
# Match sampled matrices to this normalized rest skeleton.
for frames in samples.values():
    for poses in frames.values():
        for n,m in poses.items():
            loc,rot,_=(normal@m).decompose()
            poses[n]=Matrix.LocRotScale(loc,rot,Vector((1,1,1)))
newdata=bpy.data.armatures.new('Kevin_Game_Skeleton')
game=bpy.data.objects.new('Kevin_Game',newdata); scene.collection.objects.link(game)
bpy.ops.object.select_all(action='DESELECT');game.select_set(True);bpy.context.view_layer.objects.active=game
bpy.ops.object.mode_set(mode='EDIT')
for n in names:
    b=rig.data.bones[n]; eb=newdata.edit_bones.new(n)
    eb.matrix=normal@rig.matrix_world@b.matrix_local
    eb.length=(normal@rig.matrix_world@b.tail_local-normal@rig.matrix_world@b.head_local).length
    if b.parent and b.parent.name in names: eb.parent=newdata.edit_bones[b.parent.name]
bpy.ops.object.mode_set(mode='OBJECT')
newmesh=bpy.data.objects.new('Kevin_Body',mesh.data.copy());scene.collection.objects.link(newmesh)
newmesh.data.transform(normal@world)
for group in mesh.vertex_groups: newmesh.vertex_groups.new(name=group.name)
newmesh.parent=game
modifier=newmesh.modifiers.new('Skin','ARMATURE');modifier.object=game
newanim=game.animation_data_create();baked=[]
for name,frames in samples.items():
    ac=bpy.data.actions.new('__game_'+name);ac.use_fake_user=True;newanim.action=ac
    for frame,poses in frames.items():
        for n in names:
            pb=game.pose.bones[n]; target=poses[n]
            kwargs={}
            if pb.parent:
                kwargs={'parent_matrix':poses[pb.parent.name], 'parent_matrix_local':pb.parent.bone.matrix_local}
            pb.rotation_mode='QUATERNION'
            pb.matrix_basis=pb.bone.convert_local_to_pose(target,pb.bone.matrix_local,invert=True,**kwargs)
            for prop in ('location','rotation_quaternion','scale'): pb.keyframe_insert(prop,frame=frame,group=n)
    baked.append((name,ac))
# The old rig and reference objects are removed only in this derived file.
for obj in list(bpy.data.objects):
    if obj not in (game,newmesh): bpy.data.objects.remove(obj,do_unlink=True)
newanim.action=None
for ac in list(bpy.data.actions):
    if ac not in [a for _,a in baked]: bpy.data.actions.remove(ac)
for name,ac in baked: ac.name=name
newanim.action=bpy.data.actions['Kevin_Aim_Rifle'];scene.frame_set(1);bpy.context.view_layer.update()
# Rifle convention is read from the actual GLB: forward is -Z, up is +Y (glTF).
# Calibrate a weapon-to-right-wrist transform from the authored two-hand pose.
# Save hand transforms for deterministic profile calibration outside Blender.
report={'height':2.2,'bones':len(names),'actions':[n for n,_ in baked],
        'hands':{n:[list(row) for row in game.pose.bones[n].matrix] for n in
        ['mixamorig:RightHand','mixamorig:LeftHand','mixamorig:LeftArm','mixamorig:LeftForeArm']}}
(ROOT/'tools/qa/kevin_weapon_export.json').write_text(json.dumps(report,indent=2))
bpy.context.preferences.filepaths.save_version=0
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'assets/model/Kevin_Weapon_Game.blend'),compress=True)
bpy.ops.object.select_all(action='DESELECT');game.select_set(True);newmesh.select_set(True)
bpy.ops.export_scene.gltf(filepath=str(ROOT/'assets/model/Kevin_Weapon_Game.glb'),export_format='GLB',
    use_selection=True,export_animations=True,export_frame_range=False,export_force_sampling=True,
    export_animation_mode='ACTIONS',export_yup=True,export_rest_position_armature=True)
print('EXPORTED',report)

