import bpy
from mathutils import Vector

rig = bpy.data.objects['Armature']
scene = bpy.context.scene
animation = rig.animation_data_create()
for name, frame in [('Kevin_Melee_Guard', 1), ('Kevin_Melee_Jab_L', 10)]:
    animation.action = bpy.data.actions[name]
    scene.frame_set(frame)
    bpy.context.view_layer.update()
    for bone_name in ['CTRL_Hand_IK.L', 'CTRL_Hand_IK.R', 'mixamorig:LeftHand', 'mixamorig:RightHand']:
        bone = rig.pose.bones.get(bone_name)
        if bone:
            p = rig.matrix_world @ bone.head
            print('POSE', name, frame, bone_name, tuple(round(v, 3) for v in p))
