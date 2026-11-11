import bpy
from mathutils import Matrix

rig=bpy.data.objects['Armature']
gun=bpy.data.objects['Rifle_M4_CC0']
scene=bpy.context.scene
anim=rig.animation_data_create()
for track in anim.nla_tracks: track.mute=True
names=[b.name for b in rig.data.bones if b.use_deform]

def sample(action_name, frame):
    anim.action=None
    for pb in rig.pose.bones: pb.matrix_basis=Matrix.Identity(4)
    action=bpy.data.actions[action_name]
    anim.action=action
    anim.action_slot=action.slots[0]
    scene.frame_set(frame)
    bpy.context.view_layer.update()
    evaluated=rig.evaluated_get(bpy.context.evaluated_depsgraph_get())
    evaluated_gun=gun.evaluated_get(bpy.context.evaluated_depsgraph_get())
    bones={n:(rig.matrix_world@evaluated.pose.bones[n].matrix).copy() for n in names}
    hand=bones['mixamorig:LeftHand'].translation
    grip=evaluated_gun.matrix_world.inverted()@hand
    return bones,tuple(grip),evaluated_gun.matrix_world.copy()

stance,stance_grip,_=sample('Kevin_Aim_Rifle',0)
start,start_grip,_=sample('Kevin_Aim_Shoot_R',0)
peak,peak_grip,_=sample('Kevin_Aim_Shoot_R',4)
end,end_grip,_=sample('Kevin_Aim_Shoot_R',24)
def matrix_diff(a,b):
    return max(abs(a[n][i][j]-b[n][i][j]) for n in names for i in range(4) for j in range(4))
print('SEAM_START_MAX',matrix_diff(stance,start))
print('SEAM_END_MAX',matrix_diff(stance,end))
print('HAND_GRIP_STANCE',stance_grip)
print('HAND_GRIP_PEAK',peak_grip)
print('HAND_GRIP_END',end_grip)
print('RIGHT_HAND_TRAVEL',tuple(peak['mixamorig:RightHand'].translation-start['mixamorig:RightHand'].translation))
print('CHEST_TRAVEL',tuple(peak['mixamorig:Spine2'].translation-start['mixamorig:Spine2'].translation))
