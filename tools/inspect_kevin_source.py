import bpy
from mathutils import Vector
rig=bpy.data.objects['Armature']
mesh=bpy.data.objects['MaleBaseMesh LowPoly']
print('FILE',bpy.data.filepath)
for obj in (rig,mesh):
    print('OBJ',obj.name,'parent',obj.parent.name if obj.parent else None,'matrix',*[list(row) for row in obj.matrix_world],
          'scale',list(obj.scale),'dim',list(obj.dimensions))
    print('MODS',[(m.name,m.type,getattr(m,'object',None).name if getattr(m,'object',None) else None,
                   getattr(m,'use_deform_preserve_volume',None)) for m in obj.modifiers])
print('VERTS',len(mesh.data.vertices),'GROUPS',len(mesh.vertex_groups),'BONES',len(rig.data.bones))
print('WEIGHTS',[(v.index,[(e.group,round(e.weight,3)) for e in v.groups]) for v in list(mesh.data.vertices)[:6]])
for aname in ('Kevin_Idle','Kevin_Aim_Rifle'):
    action=bpy.data.actions[aname]
    rig.animation_data.action=action
    rig.animation_data.action_slot=next((s for s in action.slots if s.target_id_type=='OBJECT'),action.slots[0])
    bpy.context.scene.frame_set(1)
    bpy.context.view_layer.update()
    evalrig=rig.evaluated_get(bpy.context.evaluated_depsgraph_get())
    print('ACTION',aname)
    for n in ['mixamorig:Hips','mixamorig:Spine2','mixamorig:LeftArm','mixamorig:LeftForeArm','mixamorig:LeftHand','mixamorig:RightArm','mixamorig:RightForeArm','mixamorig:RightHand','mixamorig:LeftLeg','mixamorig:LeftFoot']:
        pb=evalrig.pose.bones[n]
        print('BONE',n,'rest scale',tuple(round(v,3) for v in rig.data.bones[n].matrix_local.decompose()[2]),
              'pose scale',tuple(round(v,3) for v in (rig.matrix_world@pb.matrix).decompose()[2]),
              'basis scale',tuple(round(v,3) for v in pb.scale))
    evalmesh=mesh.evaluated_get(bpy.context.evaluated_depsgraph_get())
    pts=[evalmesh.matrix_world@v.co for v in evalmesh.data.vertices]
    print('BOUNDS',tuple(round(min(p[i] for p in pts),3) for i in range(3)),tuple(round(max(p[i] for p in pts),3) for i in range(3)))
