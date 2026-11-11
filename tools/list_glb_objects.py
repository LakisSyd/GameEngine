import bpy
import sys
from mathutils import Vector


def clear():
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete()


def bounds(obj):
    mins = Vector((float("inf"), float("inf"), float("inf")))
    maxs = Vector((float("-inf"), float("-inf"), float("-inf")))
    for corner in obj.bound_box:
        world = obj.matrix_world @ Vector(corner)
        mins.x = min(mins.x, world.x)
        mins.y = min(mins.y, world.y)
        mins.z = min(mins.z, world.z)
        maxs.x = max(maxs.x, world.x)
        maxs.y = max(maxs.y, world.y)
        maxs.z = max(maxs.z, world.z)
    size = maxs - mins
    return mins, maxs, size


path = sys.argv[sys.argv.index("--") + 1]
clear()
bpy.ops.import_scene.gltf(filepath=path)
print("FILE", path)
for obj in bpy.context.scene.objects:
    if obj.type == "MESH":
        mn, mx, sz = bounds(obj)
        print(
            "MESH",
            obj.name,
            "verts", len(obj.data.vertices),
            "polys", len(obj.data.polygons),
            "min", tuple(round(v, 6) for v in mn),
            "max", tuple(round(v, 6) for v in mx),
            "size", tuple(round(v, 6) for v in sz),
        )
    else:
        print("OBJ", obj.name, obj.type)
