import bpy
import math
from pathlib import Path
from mathutils import Matrix, Vector

root=Path(__file__).resolve().parents[2]
rig=bpy.data.objects['Armature']
mesh=bpy.data.objects['MaleBaseMesh LowPoly']
gun=bpy.data.objects['Rifle_M4_CC0']
anim=rig.animation_data_create()
for track in anim.nla_tracks: track.mute=True
anim.action=bpy.data.actions['Kevin_Aim_Rifle']
anim.action_slot=anim.action.slots[0]
bpy.context.scene.frame_set(0)
bpy.context.view_layer.update()
ev=rig.evaluated_get(bpy.context.evaluated_depsgraph_get())
right=rig.matrix_world@ev.pose.bones['mixamorig:RightHand'].matrix
world=mesh.matrix_world.copy()
points=[world@v.co for v in mesh.data.vertices]
height=max(p.z for p in points)-min(p.z for p in points)
factor=2.2/height
center=Vector(((max(p.x for p in points)+min(p.x for p in points))/2,
               (max(p.y for p in points)+min(p.y for p in points))/2,
               min(p.z for p in points)))
normal=Matrix.Scale(factor,4)@Matrix.Translation(-center)
loc,rot,_=(normal@right).decompose()
right_game=Matrix.LocRotScale(loc,rot,Vector((1,1,1)))
gun_game=normal@gun.matrix_world
grip=right_game.inverted()@gun_game
convert=Matrix(((1,0,0,0),(0,0,1,0),(0,1,0,0),(0,0,0,1)))
engine_col=convert@grip@convert
row=engine_col.transposed()
scale=(Vector(row[0][:3]).length,Vector(row[1][:3]).length,Vector(row[2][:3]).length)
r=[[row[i][j]/scale[i] for j in range(3)] for i in range(3)]
pitch=math.asin(max(-1,min(1,-r[2][1])))
yaw=math.atan2(r[2][0],r[2][2])
roll=math.atan2(r[0][1],r[1][1])
print('NORMAL',factor,tuple(center))
print('GUN_OBJECT',gun.name,'verts',len(gun.data.vertices),
      'dimensions',tuple(gun.dimensions),'scale',tuple(gun.scale),
      'parent',gun.parent.name if gun.parent else None,'bone',gun.parent_bone)
print('GAME_RIGHT',right_game)
print('GAME_GUN',gun_game)
print('GRIP_ENGINE',
      'position',tuple(round(row[3][i],7) for i in range(3)),
      'rotation',tuple(round(v,7) for v in (pitch,yaw,roll)),
      'scale',tuple(round(v,7) for v in scale))
print('GRIP_ROW',row)
before=set(bpy.data.objects)
bpy.ops.import_scene.gltf(filepath=str(root/'assets/model/Rifle_M4_CC0.glb'))
for obj in set(bpy.data.objects)-before:
    if obj.type=='MESH':
        print('IMPORTED_GUN',obj.name,'verts',len(obj.data.vertices),
              'dimensions',tuple(obj.dimensions),'scale',tuple(obj.scale))
        if len(obj.data.vertices)==len(gun.data.vertices):
            maxdiff=max((obj.data.vertices[i].co-gun.data.vertices[i].co).length
                        for i in range(len(obj.data.vertices)))
            print('GUN_MESH_LOCAL_VERTEX_MAXDIFF',maxdiff)
