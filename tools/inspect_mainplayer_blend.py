import os
import bpy


def safe_len(value):
    try:
        return len(value)
    except Exception:
        return 0


def iter_action_fcurves(action):
    yielded = set()
    if hasattr(action, "fcurves"):
        for fc in action.fcurves:
            yielded.add(id(fc))
            yield fc

    for layer in getattr(action, "layers", []):
        for strip in getattr(layer, "strips", []):
            channelbag = getattr(strip, "channelbag", None)
            if callable(channelbag):
                for slot in getattr(action, "slots", []):
                    try:
                        bag = channelbag(slot)
                    except Exception:
                        bag = None
                    if bag:
                        for fc in getattr(bag, "fcurves", []):
                            if id(fc) not in yielded:
                                yielded.add(id(fc))
                                yield fc
            elif channelbag:
                for fc in getattr(channelbag, "fcurves", []):
                    if id(fc) not in yielded:
                        yielded.add(id(fc))
                        yield fc

    if hasattr(action, "channelbag"):
        for slot in getattr(action, "slots", []):
            try:
                channelbag = action.channelbag(slot)
            except Exception:
                channelbag = None
            if channelbag:
                for fc in getattr(channelbag, "fcurves", []):
                    if id(fc) not in yielded:
                        yielded.add(id(fc))
                        yield fc


path = bpy.data.filepath
print("FILE", path)
print("SIZE_BYTES", os.path.getsize(path) if path else 0)
print(
    "COUNTS",
    "objects", len(bpy.data.objects),
    "meshes", len(bpy.data.meshes),
    "armatures", len(bpy.data.armatures),
    "materials", len(bpy.data.materials),
    "images", len(bpy.data.images),
    "actions", len(bpy.data.actions),
    "collections", len(bpy.data.collections),
)

print("\nOBJECTS")
for obj in bpy.data.objects:
    verts = safe_len(getattr(getattr(obj, "data", None), "vertices", []))
    polys = safe_len(getattr(getattr(obj, "data", None), "polygons", []))
    mats = safe_len(getattr(getattr(obj, "data", None), "materials", []))
    print(
        "OBJ",
        obj.name,
        obj.type,
        "loc", tuple(round(v, 4) for v in obj.location),
        "verts", verts,
        "polys", polys,
        "materials", mats,
        "modifiers", len(obj.modifiers),
        "hidden", obj.hide_get(),
    )

print("\nMESHES")
for mesh in bpy.data.meshes:
    users = mesh.users
    print(
        "MESH",
        mesh.name,
        "users", users,
        "verts", len(mesh.vertices),
        "edges", len(mesh.edges),
        "polys", len(mesh.polygons),
        "materials", len(mesh.materials),
        "shape_keys", bool(mesh.shape_keys),
    )

print("\nARMATURES")
for arm in bpy.data.armatures:
    print("ARMATURE", arm.name, "bones", len(arm.bones), "users", arm.users)

print("\nACTIONS")
for action in bpy.data.actions:
    total_keyframes = 0
    unique = list(iter_action_fcurves(action))
    for fc in unique:
        total_keyframes += len(fc.keyframe_points)
    print(
        "ACTION",
        action.name,
        "users", action.users,
        "fake", action.use_fake_user,
        "frame_range", tuple(round(v, 3) for v in action.frame_range),
        "fcurves", len(unique),
        "keyframes", total_keyframes,
        "slots", len(getattr(action, "slots", [])),
        "layers", len(getattr(action, "layers", [])),
    )

print("\nIMAGES")
for image in bpy.data.images:
    packed_size = image.packed_file.size if image.packed_file else 0
    print(
        "IMAGE",
        image.name,
        "users", image.users,
        "packed", bool(image.packed_file),
        "packed_size", packed_size,
        "size", tuple(image.size),
        "source", image.source,
        "filepath", image.filepath,
    )

print("\nMATERIALS")
for mat in bpy.data.materials:
    print("MAT", mat.name, "users", mat.users, "nodes", bool(mat.use_nodes))

print("\nORPHANS")
print(
    "unused_meshes", [m.name for m in bpy.data.meshes if m.users == 0],
    "unused_actions", [a.name for a in bpy.data.actions if a.users == 0 and not a.use_fake_user],
    "unused_images", [i.name for i in bpy.data.images if i.users == 0],
)
