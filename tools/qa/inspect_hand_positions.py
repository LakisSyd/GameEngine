import bpy
from mathutils import Vector
rig=bpy.data.objects['Kevin_Game']; ac=bpy.data.actions['Kevin_Aim_Rifle'];rig.animation_data.action=ac;rig.animation_data.action_slot=ac.slots[0];bpy.context.scene.frame_set(0)
for side in ('Right','Left'):
 for part in ('Hand','HandMiddle1','HandIndex1','HandThumb1','ForeArm','Arm'):
  name='mixamorig:'+side+part
  if name in rig.pose.bones:
   b=rig.pose.bones[name];print(name,'HEAD',tuple(round(v,5) for v in b.head),'TAIL',tuple(round(v,5) for v in b.tail))
