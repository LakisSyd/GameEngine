import bpy
from pathlib import Path
root=Path(__file__).resolve().parents[1]
before=set(bpy.data.objects)
bpy.ops.import_scene.gltf(filepath=str(root/'assets/model/Rifle_M4_CC0.glb'))
for obj in set(bpy.data.objects)-before:
    print('RIFLE_OBJ',obj.name,obj.type,'parent',obj.parent.name if obj.parent else None,
          'loc',tuple(round(x,4) for x in obj.location),
          'rot',tuple(round(x,4) for x in obj.rotation_euler),
          'scale',tuple(round(x,4) for x in obj.scale),
          'bounds',tuple(round(x,4) for x in obj.dimensions))
