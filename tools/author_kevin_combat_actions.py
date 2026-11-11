"""Author two rifle actions on the clean, normalized Kevin game rig.

Run after build_kevin_combat_game.py with Kevin_Combat_Game.blend loaded.
The master controller rig is never changed.
"""
import bpy
import math
from pathlib import Path
from mathutils import Matrix, Vector

ROOT=Path(__file__).resolve().parents[1]
GAME_BLEND=ROOT/'assets/model/Kevin_Combat_Game.blend'
GAME_GLB=ROOT/'assets/model/Kevin_Combat_Game.glb'
scene=bpy.context.scene
arm=bpy.data.objects['Kevin_Game']
mesh=bpy.data.objects['Kevin_Body']
anim=arm.animation_data_create()
assert not any(pb.constraints for pb in arm.pose.bones), 'Expected baked game rig without constraints'

aim=bpy.data.actions['Kevin_Aim_Rifle']
anim.action=aim
anim.action_slot=aim.slots[0]
scene.frame_set(1)
bpy.context.view_layer.update()
base={pb.name:(pb.location.copy(),pb.rotation_quaternion.copy(),pb.scale.copy()) for pb in arm.pose.bones}

def restore_base():
    for pb in arm.pose.bones:
        loc,rot,scale=base[pb.name]
        pb.rotation_mode='QUATERNION'
        pb.location=loc.copy()
        pb.rotation_quaternion=rot.copy()
        pb.scale=scale.copy()
    bpy.context.view_layer.update()

def rotate_world(bone_name, axis, degrees):
    pb=arm.pose.bones['mixamorig:'+bone_name]
    pivot=pb.head.copy()
    spin=Matrix.Rotation(math.radians(degrees),4,Vector(axis))
    pb.matrix=Matrix.Translation(pivot)@spin@Matrix.Translation(-pivot)@pb.matrix
    bpy.context.view_layer.update()

def align_segment(bone_name, target_direction):
    pb=arm.pose.bones['mixamorig:'+bone_name]
    pivot=pb.head.copy()
    old=(pb.tail-pb.head).normalized()
    delta=old.rotation_difference(target_direction.normalized()).to_matrix().to_4x4()
    pb.matrix=Matrix.Translation(pivot)@delta@Matrix.Translation(-pivot)@pb.matrix
    bpy.context.view_layer.update()

def solve_left_support(target):
    upper=arm.pose.bones['mixamorig:LeftArm']
    fore=arm.pose.bones['mixamorig:LeftForeArm']
    hand=arm.pose.bones['mixamorig:LeftHand']
    shoulder=upper.head.copy()
    elbow=fore.head.copy()
    wrist=hand.head.copy()
    l1=(elbow-shoulder).length
    l2=(wrist-elbow).length
    direction=(target-shoulder).normalized()
    distance=(target-shoulder).length
    if distance>l1+l2-0.002:
        raise RuntimeError(f'Left support unreachable: {distance:.4f} > {l1+l2:.4f}')
    along=(l1*l1-l2*l2+distance*distance)/(2*distance)
    height=math.sqrt(max(0.0,l1*l1-along*along))
    elbow_hint=elbow-shoulder-direction*((elbow-shoulder).dot(direction))
    elbow_hint.normalize()
    target_elbow=shoulder+direction*along+elbow_hint*height
    align_segment('LeftArm',target_elbow-shoulder)
    align_segment('LeftForeArm',target-arm.pose.bones['mixamorig:LeftForeArm'].head)
    final_wrist=arm.pose.bones['mixamorig:LeftHand'].head
    if (final_wrist-target).length>0.003:
        raise RuntimeError(f'Left support IK residual {(final_wrist-target).length:.4f}')

def close_grips():
    # The source rifle action kept the fingers fully open. Close the support
    # hand upward around the handguard and the firing hand down around the
    # pistol grip. Bone rotations are modest and independent of rifle recoil.
    for finger in ('Index','Middle','Ring','Pinky'):
        for phalanx,angle in ((1,-30.0),(2,-48.0),(3,-25.0)):
            rotate_world('LeftHand'+finger+str(phalanx),(1,0,0),angle)
        for phalanx,angle in ((1,64.0),(2,53.0),(3,24.0)):
            rotate_world('RightHand'+finger+str(phalanx),(1,0,0),angle)
    for phalanx,angle in ((1,12.0),(2,25.0),(3,12.0)):
        rotate_world('LeftHandThumb'+str(phalanx),(1,0,0),angle)
    for phalanx,angle in ((1,20.0),(2,28.0),(3,12.0)):
        rotate_world('RightHandThumb'+str(phalanx),(1,0,0),angle)

def pose_ready(breath=0.0, sway=0.0, kick=0.0):
    restore_base()
    # Lower the entire two-hand hold a little. The source action is a high
    # sighting pose; these matching shoulder adjustments give a ready stance.
    rotate_world('RightArm',(1,0,0),9.0)
    # Bring the support shoulder forward, then solve the two bones to the M4
    # handguard. Target the wrist behind the handguard contact point; the
    # palm/fingers extend around 10 cm beyond this bone origin.
    rotate_world('LeftShoulder',(0,0,1),-55.0)
    solve_left_support(Vector((-0.025,-0.530,1.870)))
    rotate_world('LeftHand',(1,0,0),-30.0)
    close_grips()
    # Breath and recoil travel through common spine ancestors, so both hands
    # follow the rifle together and the two-hand spacing stays stable.
    rotate_world('Spine1',(1,0,0),0.65*breath-2.5*kick)
    rotate_world('Spine2',(1,0,0),0.25*breath-1.0*kick)
    rotate_world('Spine1',(0,0,1),0.32*sway+0.25*kick)
    # Head resists the kick slightly, keeping eyes close to the sight line.
    rotate_world('Neck',(1,0,0),0.5*kick)

def key_all(frame):
    for pb in arm.pose.bones:
        for prop in ('location','rotation_quaternion','scale'):
            pb.keyframe_insert(data_path=prop,frame=frame,group=pb.name)

for name in ('Kevin_Rifle_Idle','Kevin_Rifle_Gun'):
    old=bpy.data.actions.get(name)
    if old:
        if anim.action==old:anim.action=None
        bpy.data.actions.remove(old)

# A two-second seamless breathing loop at 30 fps. The endpoints share the
# exact same pose; shoulders, hands, and rifle travel with the chest.
idle=bpy.data.actions.new('Kevin_Rifle_Idle')
idle.use_fake_user=True
anim.action=idle
for frame,breath,sway in ((1,0.0,0.0),(16,1.0,0.65),(31,0.0,0.0),
                         (46,-1.0,-0.65),(61,0.0,0.0)):
    scene.frame_set(frame)
    pose_ready(breath,sway,0.0)
    key_all(frame)
print('ACTION',idle.name,tuple(idle.frame_range),'slots',len(idle.slots))

# One shot: rapid back/up shoulder impulse, slight forward overshoot, complete
# recovery in half a second. Frame 1 and 16 match the rifle idle seam exactly.
gun=bpy.data.actions.new('Kevin_Rifle_Gun')
gun.use_fake_user=True
anim.action=gun
for frame,kick in ((1,0.0),(3,0.45),(5,1.0),(7,0.72),(10,0.12),(12,-0.12),(16,0.0)):
    scene.frame_set(frame)
    pose_ready(0.0,0.0,kick)
    key_all(frame)
print('ACTION',gun.name,tuple(gun.frame_range),'slots',len(gun.slots))

# The source Run contains a nearly constant backward pelvis offset. The
# engine rotates the visual about its origin, making that offset describe a
# circle on turns. Remove only the mean horizontal displacement, retaining
# the animated stride and vertical bounce.
run=bpy.data.actions['Kevin_Run']
if not run.get('Kevin_Run_InPlace'):
    anim.action=run
    anim.action_slot=run.slots[0]
    frames=range(int(run.frame_range[0]),int(run.frame_range[1])+1)
    hips=arm.pose.bones['mixamorig:Hips']
    assert hips.parent is None
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
    run['Kevin_Run_InPlace']=tuple(mean)
    print('RUN_RECENTER',tuple(round(v,5) for v in mean))

anim.action=idle
anim.action_slot=idle.slots[0]
scene.frame_set(1)
bpy.context.view_layer.update()
for n in ('RightHand','LeftHand'):
    p=arm.pose.bones['mixamorig:'+n]
    print('READY_HAND',n,tuple(round(v,4) for v in p.head))

scene.frame_start=1
scene.frame_end=61
scene.render.fps=30
bpy.context.preferences.filepaths.save_version=0
bpy.ops.wm.save_as_mainfile(filepath=str(GAME_BLEND),compress=True)

bpy.ops.object.select_all(action='DESELECT')
arm.select_set(True);mesh.select_set(True)
bpy.ops.export_scene.gltf(filepath=str(GAME_GLB),export_format='GLB',
    use_selection=True,export_animations=True,export_frame_range=False,
    export_force_sampling=True,export_animation_mode='ACTIONS',export_yup=True,
    export_rest_position_armature=True)
print('EXPORTED',GAME_BLEND,GAME_GLB)
