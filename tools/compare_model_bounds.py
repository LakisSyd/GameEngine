import bpy
import os
import sys
from mathutils import Vector


def clear_scene():
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete()


def import_model(path):
    ext = os.path.splitext(path)[1].lower()
    if ext == ".glb" or ext == ".gltf":
        bpy.ops.import_scene.gltf(filepath=path)
    elif ext == ".fbx":
        bpy.ops.import_scene.fbx(filepath=path)
    elif ext == ".blend":
        # Open blend as current file instead of appending.
        bpy.ops.wm.open_mainfile(filepath=path)
    else:
        raise RuntimeError(f"Unsupported file extension: {ext}")


def scene_bounds():
    mins = Vector((float("inf"), float("inf"), float("inf")))
    maxs = Vector((float("-inf"), float("-inf"), float("-inf")))
    mesh_count = 0
    verts = 0
    polys = 0
    for obj in bpy.context.scene.objects:
        if obj.type != "MESH":
            continue
        mesh_count += 1
        verts += len(obj.data.vertices)
        polys += len(obj.data.polygons)
        for corner in obj.bound_box:
            world = obj.matrix_world @ Vector(corner)
            mins.x = min(mins.x, world.x)
            mins.y = min(mins.y, world.y)
            mins.z = min(mins.z, world.z)
            maxs.x = max(maxs.x, world.x)
            maxs.y = max(maxs.y, world.y)
            maxs.z = max(maxs.z, world.z)
    if mesh_count == 0:
        mins = Vector((0, 0, 0))
        maxs = Vector((0, 0, 0))
    size = maxs - mins
    return mesh_count, verts, polys, mins, maxs, size


for path in sys.argv[sys.argv.index("--") + 1:]:
    clear_scene()
    import_model(path)
    mesh_count, verts, polys, mins, maxs, size = scene_bounds()
    print("MODEL", path)
    print("  meshes", mesh_count, "verts", verts, "polys", polys)
    print("  min", tuple(round(v, 6) for v in mins))
    print("  max", tuple(round(v, 6) for v in maxs))
    print("  size", tuple(round(v, 6) for v in size), "height_z", round(size.z, 6))
