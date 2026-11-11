import bpy
from mathutils import Matrix

rig=bpy.data.objects['Armature']
anim=rig.animation_data_create()
scene=bpy.context.scene
for track in anim.nla_tracks: track.mute=True
names=['CTRL_Root','CTRL_COG','CTRL_Spine','CTRL_Spine_01','CTRL_Chest',
       'CTRL_Head','CTRL_Neck','CTRL_Shoulder.L','CTRL_Shoulder.R',
       'CTRL_Hand_IK.L','CTRL_Hand_IK.R','CTRL_Elbow_Pole.L',
       'mixamorig:RightHand','mixamorig:LeftHand']
for n in names:
    p=rig.pose.bones.get(n)
    if p:
        print('CONTROL',n,'parent',p.parent.name if p.parent else None,
              'rotation_mode',p.rotation_mode,
              'constraints',[(c.type,round(c.influence,2)) for c in p.constraints])
for action_name,frames in [('Kevin_Aim_Rifle',[0]),('Kevin_Aim_Shoot_R',[0,2,4,8])]:
    anim.action=None
    for pb in rig.pose.bones: pb.matrix_basis=Matrix.Identity(4)
    ac=bpy.data.actions[action_name];anim.action=ac;anim.action_slot=ac.slots[0]
    for frame in frames:
        scene.frame_set(frame);bpy.context.view_layer.update()
        evaluated=rig.evaluated_get(bpy.context.evaluated_depsgraph_get())
        print('FRAME',action_name,frame)
        for n in names:
            p=rig.pose.bones.get(n)
            ep=evaluated.pose.bones.get(n)
            if not p or not ep: continue
            print(' ',n,'local',tuple(round(v,4) for v in p.location),
                  'euler',tuple(round(v,4) for v in p.rotation_euler),
                  'head',tuple(round(v,4) for v in ep.head),
                  'tail',tuple(round(v,4) for v in ep.tail))
