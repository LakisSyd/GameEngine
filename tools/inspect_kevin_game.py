import bpy

print('BLENDER', bpy.app.version_string)
print('SCENE', bpy.context.scene.frame_start, bpy.context.scene.frame_end)
for obj in bpy.data.objects:
    print('OBJ', obj.name, obj.type, 'parent', obj.parent.name if obj.parent else None,
          'loc', tuple(round(x,4) for x in obj.location),
          'rot', tuple(round(x,4) for x in obj.rotation_euler),
          'scale', tuple(round(x,4) for x in obj.scale),
          'dim', tuple(round(x,4) for x in obj.dimensions))
    if obj.type == 'ARMATURE':
        print('BONES', len(obj.data.bones), [b.name for b in obj.data.bones])
        print('ANIM_DATA', obj.animation_data.action.name if obj.animation_data and obj.animation_data.action else None)
        print('CUSTOM', list(obj.keys()))
for a in bpy.data.actions:
    print('ACTION', a.name, tuple(a.frame_range), len(a.fcurves) if hasattr(a, 'fcurves') else 'layered', 'slots', [(s.identifier,s.target_id_type) for s in a.slots])
    if hasattr(a,'fcurves'):
        print('  PATHS', sorted({fc.data_path for fc in a.fcurves})[:30])
print('COLLECTIONS',[(c.name,[o.name for o in c.objects]) for c in bpy.data.collections])
