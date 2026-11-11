import bpy,math
from mathutils import Matrix,Vector
rig=bpy.data.objects['Kevin_Game'];ac=bpy.data.actions['Kevin_Aim_Rifle'];rig.animation_data.action=ac;rig.animation_data.action_slot=ac.slots[0];bpy.context.scene.frame_set(0)
gun=Matrix(((-.8,0,0,-.096),(0,-.8,0,-.339),(0,0,.8,1.8903),(0,0,0,1)))
left=rig.pose.bones['mixamorig:LeftHand'].matrix
local=gun.inverted()@left
c=Matrix(((1,0,0,0),(0,0,1,0),(0,1,0,0),(0,0,0,1)))
row=(c@local@c).transposed();scale=[Vector(row[i][:3]).length for i in range(3)];r=[[row[i][j]/scale[i] for j in range(3)] for i in range(3)]
p=math.asin(max(-1,min(1,-r[2][1])));y=math.atan2(r[2][0],r[2][2]);z=math.atan2(r[0][1],r[1][1])
print('LEFT_TARGET_ENGINE', [round(row[3][i],7) for i in range(3)],[round(v,7) for v in (p,y,z)],[round(v,7) for v in scale])
