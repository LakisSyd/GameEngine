import bpy
from mathutils import Vector

rig = bpy.data.objects['Armature']
scene = bpy.context.scene
anim = rig.animation_data_create()
for track in anim.nla_tracks:
    track.mute = True
base = {bone.name: bone.matrix_basis.copy() for bone in rig.pose.bones}
names = ('mixamorig:Hips', 'mixamorig:Spine', 'mixamorig:Spine1',
         'mixamorig:Spine2', 'mixamorig:Neck', 'mixamorig:Head',
         'mixamorig:LeftHand', 'mixamorig:RightHand',
         'CTRL_Hand_IK.L', 'CTRL_Hand_IK.R', 'CTRL_COG', 'CTRL_Chest')
print('RELEVANT_BONES', [bone.name for bone in rig.pose.bones
                         if bone.name in names or bone.name.startswith('CTRL_')])
for action_name in ('Kevin_Walk', 'Kevin_Melee_Guard'):
    action = bpy.data.actions[action_name]
    for frame in (1, 16, 31, 46, 60):
        anim.action = None
        for bone in rig.pose.bones: bone.matrix_basis = base[bone.name]
        anim.action = action
        anim.action_slot = action.slots[0]
        scene.frame_set(frame)
        bpy.context.view_layer.update()
        values = {}
        for name in names:
            bone = rig.pose.bones.get(name)
            if bone:
                values[name] = tuple(round(v, 3) for v in
                                     (rig.matrix_world @ bone.head))
        print('POSE', action_name, frame, values)
