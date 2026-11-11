"""Author editable pistol and crouch actions on the switchable Kevin master.

Input: Kevin_Main_Switchable_GuardWalk_v2.blend. Original actions are retained.
Run before build_kevin_combat_game.py to export world and FP variants.
"""
import math
from pathlib import Path
import bpy
from mathutils import Vector

rig = bpy.data.objects['Armature']
scene = bpy.context.scene
anim = rig.animation_data_create()
for track in anim.nla_tracks:
    track.mute = True
initial = {b.name: b.matrix_basis.copy() for b in rig.pose.bones}
follow = [c for c in rig.pose.bones['CTRL_Hand_IK.L'].constraints if c.type == 'CHILD_OF']
for c in follow:
    c.mute = True

def activate(name, frame):
    anim.action = None
    for b in rig.pose.bones:
        b.matrix_basis = initial[b.name]
    anim.action = bpy.data.actions[name]
    anim.action_slot = anim.action.slots[0]
    scene.frame_set(int(frame), subframe=frame % 1)
    for c in follow:
        c.mute = True
    bpy.context.view_layer.update()

def move(name, offset):
    bone = rig.pose.bones[name]
    mat = bone.matrix.copy()
    mat.translation += rig.matrix_world.inverted().to_3x3() @ Vector(offset)
    bone.matrix = mat
    bpy.context.view_layer.update()

def set_world(name, position):
    bone = rig.pose.bones[name]
    mat = bone.matrix.copy()
    mat.translation = rig.matrix_world.inverted() @ Vector(position)
    bone.matrix = mat
    bpy.context.view_layer.update()

# Capture a two-handed handgun stance relative to the chest. Keep the rifle
# wrist orientation as a starting point for a separately calibrated pistol grip.
activate('Kevin_Aim_Rifle', 0)
chest = rig.matrix_world @ rig.pose.bones['CTRL_Chest'].matrix
center = chest.translation.copy()
for side, offset in [('R',(-0.14,-1.45,0.13)),('L',(0.02,-1.34,0.11))]:
    set_world('CTRL_Hand_IK.'+side, center + Vector(offset))
for side, offset in [('R',(-0.68,-0.70,-0.25)),('L',(0.58,-0.68,-0.23))]:
    set_world('CTRL_Elbow_Pole.'+side, center + Vector(offset))
pistol_targets = {n: rig.pose.bones['CTRL_Chest'].matrix.inverted() @ rig.pose.bones[n].matrix
    for n in ['CTRL_Hand_IK.L','CTRL_Hand_IK.R','CTRL_Elbow_Pole.L','CTRL_Elbow_Pole.R']}
pistol_fingers = {b.name:b.matrix_basis.copy() for b in rig.pose.bones
    if any(s in b.name for s in ['HandThumb','HandIndex','HandMiddle','HandRing','HandPinky','CTRL_Fist'])}

specs = [('Kevin_Pistol_Idle','Kevin_Idle',False,'pistol',False),
         ('Kevin_Pistol_Walk','Kevin_Walk',False,'pistol',True),
         ('Kevin_Pistol_Shot','Kevin_Idle',False,'shot',False)]
for pose, source in [('Unarmed','Kevin_Idle'),('Pistol','Kevin_Idle'),('Rifle','Kevin_Idle')]:
    prefix = 'Kevin_Crouch' if pose == 'Unarmed' else 'Kevin_Crouch_'+pose
    specs += [(prefix+'_Idle',source,True,pose.lower(),False),
              (prefix+'_Walk','Kevin_Walk',True,pose.lower(),True)]

activate('Kevin_Aim_Rifle',0)
rifle_upper = {b.name:b.matrix_basis.copy() for b in rig.pose.bones
    if b.name.startswith(('CTRL_Shoulder','CTRL_UpperArm','CTRL_Forearm','CTRL_Hand_FK'))}
rifle_targets = {n: rig.pose.bones['CTRL_Chest'].matrix.inverted() @ rig.pose.bones[n].matrix
    for n in pistol_targets}
activate('Kevin_Melee_Guard',1)
guard_targets = {n: rig.pose.bones['CTRL_Chest'].matrix.inverted() @ rig.pose.bones[n].matrix
    for n in pistol_targets}

for name, source, crouch, style, walking in specs:
    samples = []
    end = 13 if style == 'shot' else 61
    for frame in range(1,end+1):
        phase = (frame-1)/(end-1)
        activate(source, 1+(frame-1)*59/60 if walking and frame < end else 1)
        anim.action = None
        if crouch:
            # Keep both foot contacts and poles in place while lowering the
            # pelvis: IK bends the knees rather than shortening the skeleton.
            if walking:
                for side in ['L','R']:
                    foot = rig.pose.bones['CTRL_Foot_IK.'+side]
                    p = (rig.matrix_world @ foot.matrix).translation
                    move(foot.name,(0,-p.y*0.65,0))
            for side in ['L','R']:
                p=(rig.matrix_world @ rig.pose.bones['CTRL_Foot_IK.'+side].matrix).translation
                set_world('CTRL_Knee_Pole.'+side,p+Vector((0,-1.3,1.4)))
            move('CTRL_COG',(0,-0.12,-0.80))
        if style in ['pistol','shot','rifle','unarmed']:
            targets = rifle_targets if style == 'rifle' else guard_targets if style == 'unarmed' else pistol_targets
            if style == 'rifle':
                for n,basis in rifle_upper.items():
                    rig.pose.bones[n].matrix_basis=basis
                bpy.context.view_layer.update()
            else:
                for n,basis in pistol_fingers.items():
                    rig.pose.bones[n].matrix_basis=basis
            chest_pose=rig.pose.bones['CTRL_Chest'].matrix.copy()
            for n,relative in targets.items():
                rig.pose.bones[n].matrix=chest_pose @ relative
                bpy.context.view_layer.update()
            if style == 'shot':
                kick = math.sin(math.pi*min(1,phase/0.25)) if phase < 0.25 else 0
                for side in ['L','R']:
                    move('CTRL_Hand_IK.'+side,(0,0.09*kick,0.07*kick))
        samples.append({b.name:b.matrix_basis.copy() for b in rig.pose.bones})
    if walking:
        for i in range(53,61):
            t=(i-52)/8; t=t*t*(3-2*t)
            samples[i]={n:m.lerp(samples[0][n],t) for n,m in samples[i].items()}
    samples[-1] = {n:m.copy() for n,m in samples[0].items()}
    action=bpy.data.actions.new(name)
    action.use_fake_user=True
    anim.action=action
    for frame,pose in enumerate(samples,1):
        scene.frame_set(frame)
        for n,basis in pose.items():
            b=rig.pose.bones[n]; b.matrix_basis=basis
            for path in ['location','rotation_quaternion' if b.rotation_mode=='QUATERNION' else 'rotation_euler','scale']:
                b.keyframe_insert(data_path=path,frame=frame,group=n)
    anim.action_slot=action.slots[0]
    print('AUTHORED',name,tuple(action.frame_range))
activate('Kevin_Pistol_Idle',1)
scene.frame_start,scene.frame_end=1,61
scene.render.fps=30
out=Path(r'C:\Users\karin\OneDrive\Desktop\Kevin_Main_Switchable_Combat_v3.blend')
bpy.context.preferences.filepaths.save_version=0
bpy.ops.wm.save_as_mainfile(filepath=str(out),compress=True)
print('MASTER_SAVED',out)
