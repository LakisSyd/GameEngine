import bpy
from mathutils import Matrix

rig = bpy.data.objects['Armature']
scene = bpy.context.scene
anim = rig.animation_data_create()
print('FILE', bpy.data.filepath, 'FPS', scene.render.fps)
print('ACTIVE', anim.action.name if anim.action else None)
for obj in bpy.data.objects:
    if obj.type in {'MESH', 'EMPTY'}:
        print('OBJECT', obj.name, obj.type, 'parent', obj.parent.name if obj.parent else None,
              'parent_type', obj.parent_type, 'parent_bone', obj.parent_bone)
for name in ['CTRL_Hand_IK.L', 'CTRL_Elbow_Pole.L', 'mixamorig:LeftHand',
             'mixamorig:RightHand', 'mixamorig:Spine', 'mixamorig:Spine1']:
    pb = rig.pose.bones.get(name)
    if pb:
        print('BONE', name, 'constraints', [(c.name, c.type,
              c.target.name if getattr(c, 'target', None) else None,
              getattr(c, 'subtarget', ''), round(c.influence, 3)) for c in pb.constraints])

for action_name in ['Kevin_Aim_Rifle', 'Kevin_Aim_Shoot_R']:
    action = bpy.data.actions[action_name]
    print('ACTION', action.name, 'range', tuple(action.frame_range),
          'slots', [(s.identifier, s.target_id_type) for s in action.slots])
    for layer in action.layers:
        for strip in layer.strips:
            for slot in action.slots:
                bag = strip.channelbag(slot)
                if bag:
                    paths = sorted({fc.data_path for fc in bag.fcurves})
                    print('PATHS', action_name, len(paths), paths)
    anim.action = None
    for pb in rig.pose.bones:
        pb.matrix_basis = Matrix.Identity(4)
    anim.action = action
    anim.action_slot = action.slots[0]
    for frame in range(int(action.frame_range[0]), int(action.frame_range[1]) + 1):
        scene.frame_set(frame)
        bpy.context.view_layer.update()
        eval_rig = rig.evaluated_get(bpy.context.evaluated_depsgraph_get())
        hands = []
        for n in ['mixamorig:RightHand', 'mixamorig:LeftHand', 'mixamorig:LeftForeArm',
                  'mixamorig:RightArm', 'mixamorig:Spine1']:
            p = eval_rig.pose.bones.get(n)
            if p:
                hands.append((n, tuple(round(x, 4) for x in (rig.matrix_world @ p.matrix).translation)))
        print('FRAME', action_name, frame, hands)
