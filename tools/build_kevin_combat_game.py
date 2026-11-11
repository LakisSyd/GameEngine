"""Export a normalized, correctly bound game rig from the untouched master.
Run: blender --background Kevin_Main_Switchable.blend --python tools/build_kevin_combat_game.py
"""
import bpy, json, runpy, sys
from pathlib import Path
from mathutils import Matrix, Vector
sys.path.insert(0, str(Path(__file__).resolve().parent))
from kevin_socket_bridge import write_socket_sidecar
ROOT = Path(__file__).resolve().parents[1]
source_blend = bpy.data.filepath
rig = bpy.data.objects['Armature']
mesh = bpy.data.objects['MaleBaseMesh LowPoly']
scene = bpy.context.scene
if bpy.context.object and bpy.context.object.mode != 'OBJECT': bpy.ops.object.mode_set(mode='OBJECT')
anim = rig.animation_data_create()
for t in anim.nla_tracks: t.mute = True
initial_basis = {pb.name: pb.matrix_basis.copy() for pb in rig.pose.bones}
independent_hand_actions = {
    'Kevin_Idle', 'Kevin_Walk', 'Kevin_Run', 'Kevin_Walk_R_I', 'Kevin_Rifle_Run',
    'Kevin_Punch.1'
}
independent_hand_actions.update(action.name for action in bpy.data.actions
                                if action.name.startswith('Kevin_Melee_'))
independent_hand_actions.update(action.name for action in bpy.data.actions
                                if action.name.startswith(('Kevin_Pistol_', 'Kevin_Crouch_')))
independent_hand_actions.update(action.name for action in bpy.data.actions
                                if action.name.startswith('Kevin_Rifle_Peek_'))
# Only gameplay clips are baked. Optional authored clips can be previewed in
# game without changing the weapon profile's current idle/shot pairing.
required_actions = ('Kevin_Idle', 'Kevin_Walk', 'Kevin_Run',
                    'Kevin_Walk_R_I', 'Kevin_Rifle_Run',
                    'Kevin_Aim_Rifle', 'Kevin_Aim_Shoot_R',
                    'Kevin_Punch.1')
optional_actions = ('Kevin_Idle_Gun_R_SoftSway',)
rifle_peek_actions = tuple(sorted(action.name for action in bpy.data.actions
                                  if action.name.startswith('Kevin_Rifle_Peek_')
                                  and not action.name.endswith('_Previous')))
melee_actions = tuple(sorted(action.name for action in bpy.data.actions
                             if action.name.startswith(('Kevin_Melee_', 'Kevin_Pistol_', 'Kevin_Crouch_'))
                             and not action.name.endswith('_Previous')))
export_actions = required_actions + tuple(name for name in optional_actions
                                          if name in bpy.data.actions) + rifle_peek_actions + melee_actions
# This authoring constraint makes the support hand follow the right hand while
# aiming. It must not be evaluated when baking unarmed locomotion: glTF does
# not export the constraint, but it *does* export its sampled result.
left_ik = rig.pose.bones.get('CTRL_Hand_IK.L')
left_hand_follow = [c for c in left_ik.constraints
                    if c.type == 'CHILD_OF' and c.target == rig
                    and c.subtarget == 'mixamorig:RightHand'] if left_ik else []
saved_follow_state = [(c, c.mute, c.inverse_matrix.copy()) for c in left_hand_follow]
print('LEFT_HAND_CHILD_OF', [c.name for c in left_hand_follow])
# Build the game skeleton from the original Mixamo deform bones only. New
# authoring controls can still have Deform enabled accidentally (for example
# CTRL_Fist.L/R); their evaluated effect is sampled into the finger bones
# below, but the controls themselves must never become game bones or tracks.
names = [b.name for b in rig.data.bones
         if b.use_deform and b.name.startswith('mixamorig:')]
ignored_deform_controls = [b.name for b in rig.data.bones
                           if b.use_deform and not b.name.startswith('mixamorig:')]
print('IGNORED_AUTHORING_BONES', ignored_deform_controls)
if len(names) != 65:
    raise RuntimeError(f'Expected 65 Mixamo deform bones for Kevin, found {len(names)}')
def depth(name):
    b=rig.data.bones[name]; d=0
    while b.parent: d+=1; b=b.parent
    return d
names.sort(key=depth)
# Reset unkeyed controls when switching actions (no pose leaking between clips).
def action_set(name):
    anim.action = None
    for pb in rig.pose.bones: pb.matrix_basis = initial_basis[pb.name]
    action=bpy.data.actions[name]
    anim.action=action
    slots=list(action.slots)
    if slots: anim.action_slot=next((s for s in slots if s.target_id_type=='OBJECT'),slots[0])
    return action
samples={}
socket_worlds={}
socket_aim_frame=None
socket_objects={
    'grip': bpy.data.objects.get('DX_SOCKET_WeaponGrip'),
    'left_hand': bpy.data.objects.get('DX_SOCKET_LeftHand'),
    'muzzle': bpy.data.objects.get('DX_SOCKET_Muzzle'),
}
socket_metadata=dict(socket_objects['grip'].items()) if socket_objects['grip'] else {}
for name in export_actions:
    action=action_set(name)
    lo,hi=map(lambda x:int(round(x)),action.frame_range)
    mute_follow = name in independent_hand_actions
    if name in {'Kevin_Aim_Rifle', 'Kevin_Aim_Shoot_R'} and left_hand_follow:
        # These older raised-weapon Actions predate the Child Of inverse used
        # by the low-ready idle. Use their own frame-zero right-hand transform
        # as the inverse so the authored left wrist stays in place at the
        # start, then follows recoil through the rest of the shot.
        for c in left_hand_follow: c.mute = True
        scene.frame_set(lo)
        bpy.context.view_layer.update()
        authored_left = (rig.matrix_world @ rig.pose.bones['mixamorig:LeftHand'].matrix).translation.copy()
        raised_inverse = rig.pose.bones['mixamorig:RightHand'].matrix.inverted()
        for c in left_hand_follow:
            c.inverse_matrix = raised_inverse
            c.mute = False
        bpy.context.view_layer.update()
        left_rest_error = ((rig.matrix_world @ rig.pose.bones['mixamorig:LeftHand'].matrix).translation
                           - authored_left).length
        if left_rest_error > 1e-4:
            raise RuntimeError(f'{name}: Child Of moves the authored left wrist by {left_rest_error:.5f} m')
        print('RAISED_HAND_REST_ERROR', name, left_rest_error)
    else:
        for c, _, inverse in saved_follow_state: c.inverse_matrix = inverse
    print('SAMPLE',name,lo,hi,'slot',anim.action_slot.identifier if anim.action_slot else None,
          'left_hand_child_of', 'OFF' if mute_follow else 'ON',
          'inverse', 'RAISED' if name in {'Kevin_Aim_Rifle', 'Kevin_Aim_Shoot_R'} else 'SAVED')
    frames={}
    for frame in range(lo,hi+1):
        scene.frame_set(frame)
        # Set after frame_set so an action key on the constraint cannot turn
        # the authoring link back on during an unarmed sample.
        for c in left_hand_follow: c.mute = mute_follow
        bpy.context.view_layer.update()
        depsgraph=bpy.context.evaluated_depsgraph_get()
        evaluated=rig.evaluated_get(depsgraph)
        frames[frame]={n:rig.matrix_world @ evaluated.pose.bones[n].matrix for n in names}
        if name == 'Kevin_Aim_Rifle' and frame == lo:
            socket_aim_frame=frame
            socket_worlds={role:obj.evaluated_get(depsgraph).matrix_world.copy()
                           for role,obj in socket_objects.items() if obj}
    samples[name]=frames
for c, mute, inverse in saved_follow_state:
    c.mute = mute
    c.inverse_matrix = inverse
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
write_socket_sidecar(
    ROOT/'assets/model/Kevin_Combat_Game.sockets.json', source_blend,
    samples['Kevin_Aim_Rifle'][socket_aim_frame]['mixamorig:RightHand'],
    normal, socket_worlds, socket_metadata, socket_aim_frame)
newdata=bpy.data.armatures.new('Kevin_Game_Skeleton')
game=bpy.data.objects.new('Kevin_Game',newdata); scene.collection.objects.link(game)
bpy.ops.object.select_all(action='DESELECT');game.select_set(True);bpy.context.view_layer.objects.active=game
bpy.ops.object.mode_set(mode='EDIT')
for n in names:
    b=rig.data.bones[n]; eb=newdata.edit_bones.new(n)
    # A newly created edit bone has zero length. Setting its matrix followed by
    # its length silently resets its direction to +Z in Blender 5.2. Set both
    # endpoints first, then align its roll to the source rest matrix.
    eb.head=normal@rig.matrix_world@b.head_local
    eb.tail=normal@rig.matrix_world@b.tail_local
    _,rot,_=(normal@rig.matrix_world@b.matrix_local).decompose()
    eb.align_roll(rot@Vector((0,0,1)))
    # Authoring controls can sit between deform bones. RightHand, for example,
    # is parented through CTRL_Fist.R to RightForeArm. Keep the nearest deform
    # ancestor or the hand becomes a separate root and misses the game's
    # upper-body animation layer.
    parent = b.parent
    while parent and parent.name not in names:
        parent = parent.parent
    if parent:
        eb.parent = newdata.edit_bones[parent.name]
bpy.ops.object.mode_set(mode='OBJECT')
roots = [bone.name for bone in newdata.bones if bone.parent is None]
if roots != ['mixamorig:Hips']:
    raise RuntimeError(f'Game skeleton has unexpected root bones: {roots}')
if newdata.bones['mixamorig:RightHand'].parent.name != 'mixamorig:RightForeArm':
    raise RuntimeError('RightHand must follow RightForeArm across CTRL_Fist.R')
newmesh=bpy.data.objects.new('Kevin_Body',mesh.data.copy());scene.collection.objects.link(newmesh)
newmesh.data.transform(normal@world)
# Mesh.copy() already carries the original 52 vertex groups and weights.
# Creating them again gave this object 104 groups with duplicate .001 names.
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
# The source Run has a persistent horizontal pelvis offset. The engine turns
# the player around the Transform origin, so remove that offset from the baked
# game clip while retaining its stride and vertical bounce.
run=bpy.data.actions['Kevin_Run']
newanim.action=run
newanim.action_slot=run.slots[0]
hips=game.pose.bones['mixamorig:Hips']
frames=range(int(run.frame_range[0]),int(run.frame_range[1])+1)
centers=[]
for frame in frames:
    scene.frame_set(frame)
    centers.append(hips.head.copy())
mean=Vector((sum(p.x for p in centers)/len(centers),
             sum(p.y for p in centers)/len(centers),0.0))
local_delta=hips.bone.matrix_local.to_quaternion().inverted()@(-mean)
for frame in frames:
    scene.frame_set(frame)
    hips.location+=local_delta
    hips.keyframe_insert(data_path='location',frame=frame,group=hips.name)
print('RUN_RECENTER',tuple(round(v,5) for v in mean))
newanim.action=bpy.data.actions['Kevin_Aim_Rifle'];scene.frame_set(1);bpy.context.view_layer.update()
# Rifle convention is read from the actual GLB: forward is -Z, up is +Y (glTF).
# Calibrate a weapon-to-right-wrist transform from the authored two-hand pose.
# Save hand transforms for deterministic profile calibration outside Blender.
report={'height':2.2,'bones':len(names),'actions':[n for n,_ in baked],
        'hands':{n:[list(row) for row in game.pose.bones[n].matrix] for n in
        ['mixamorig:RightHand','mixamorig:LeftHand','mixamorig:LeftArm','mixamorig:LeftForeArm']}}
(ROOT/'tools/qa/kevin_combat_export.json').write_text(json.dumps(report,indent=2))
bpy.context.preferences.filepaths.save_version=0
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'assets/model/Kevin_Combat_Game.blend'),compress=True)
bpy.ops.object.select_all(action='DESELECT');game.select_set(True);newmesh.select_set(True)
bpy.ops.export_scene.gltf(filepath=str(ROOT/'assets/model/Kevin_Combat_Game.glb'),export_format='GLB',
    use_selection=True,export_animations=True,export_frame_range=False,export_force_sampling=True,
    export_animation_mode='ACTIONS',export_yup=True,export_rest_position_armature=True)
print('EXPORTED',report)
runpy.run_path(str(ROOT/'tools/build_kevin_combat_fp.py'), run_name='__main__')

