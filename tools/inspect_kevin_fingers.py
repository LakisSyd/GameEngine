import bpy
arm=bpy.data.objects['Kevin_Game'];act=bpy.data.actions['Kevin_Rifle_Idle']
arm.animation_data.action=act;arm.animation_data.action_slot=act.slots[0]
bpy.context.scene.frame_set(1);bpy.context.view_layer.update()
for side in ('Left','Right'):
    for n in (side+'Hand',*[side+'Hand'+finger+str(j) for finger in ('Thumb','Index','Middle','Ring','Pinky') for j in (1,2,3)]):
        pb=arm.pose.bones.get('mixamorig:'+n)
        if pb:
            print('BONE',n,'head',tuple(round(v,4) for v in pb.head),'tail',tuple(round(v,4) for v in pb.tail),
                  'local_x',tuple(round(v,3) for v in (pb.matrix.to_3x3()@__import__('mathutils').Vector((1,0,0))).normalized()),
                  'local_y',tuple(round(v,3) for v in (pb.matrix.to_3x3()@__import__('mathutils').Vector((0,1,0))).normalized()),
                  'local_z',tuple(round(v,3) for v in (pb.matrix.to_3x3()@__import__('mathutils').Vector((0,0,1))).normalized()))
