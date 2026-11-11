import bpy
from mathutils import Vector

arm=bpy.data.objects['Kevin_Game']
names=['Hips','Spine','Spine1','Spine2','Head','LeftShoulder','LeftArm','LeftForeArm','LeftHand','RightShoulder','RightArm','RightForeArm','RightHand','LeftUpLeg','LeftLeg','LeftFoot','RightUpLeg','RightLeg','RightFoot']
for act in bpy.data.actions:
    arm.animation_data.action=act
    arm.animation_data.action_slot=act.slots[0]
    frames=[1,10,20,30,40] if act.name=='Kevin_Aim_Rifle' else [1]
    for f in frames:
        bpy.context.scene.frame_set(f)
        print('POSE',act.name,f)
        for n in names:
            p=arm.pose.bones.get('mixamorig:'+n)
            if p:
                h=arm.matrix_world @ p.head
                t=arm.matrix_world @ p.tail
                print(n,'h',tuple(round(v,3) for v in h),'t',tuple(round(v,3) for v in t),
                      'quat',tuple(round(v,3) for v in p.rotation_quaternion),
                      'eul',tuple(round(v,3) for v in p.rotation_euler))
