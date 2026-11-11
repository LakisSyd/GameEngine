import os
import sys
import bpy
from mathutils import Vector


# Player.glb has 2.710948 final imported height. Blender's glTF skin export
# expands this model's final posed bounds by ~4.3%, so we compensate here and
# verify the exported GLB afterwards.
TARGET_HEIGHT = 2.599515
TARGET_POLYS = 50000


def log(*args):
    print("[optimize-mainplayer]", *args)


def scene_mesh_bounds():
    mins = Vector((float("inf"), float("inf"), float("inf")))
    maxs = Vector((float("-inf"), float("-inf"), float("-inf")))
    found = False
    for obj in bpy.context.scene.objects:
        if obj.type != "MESH":
            continue
        found = True
        for corner in obj.bound_box:
            world = obj.matrix_world @ Vector(corner)
            mins.x = min(mins.x, world.x)
            mins.y = min(mins.y, world.y)
            mins.z = min(mins.z, world.z)
            maxs.x = max(maxs.x, world.x)
            maxs.y = max(maxs.y, world.y)
            maxs.z = max(maxs.z, world.z)
    if not found:
        return Vector((0, 0, 0)), Vector((0, 0, 0)), Vector((0, 0, 0))
    return mins, maxs, maxs - mins


def total_mesh_stats():
    verts = 0
    polys = 0
    for obj in bpy.context.scene.objects:
        if obj.type == "MESH":
            verts += len(obj.data.vertices)
            polys += len(obj.data.polygons)
    return verts, polys


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
                bag = action.channelbag(slot)
            except Exception:
                bag = None
            if bag:
                for fc in getattr(bag, "fcurves", []):
                    if id(fc) not in yielded:
                        yielded.add(id(fc))
                        yield fc


def remove_scene_junk():
    keep_types = {"MESH", "ARMATURE"}
    for obj in list(bpy.context.scene.objects):
        if obj.type not in keep_types:
            bpy.data.objects.remove(obj, do_unlink=True)
    for image in list(bpy.data.images):
        if image.name in {"Render Result", "Viewer Node"}:
            bpy.data.images.remove(image)


def scale_to_target_height():
    mins, maxs, size = scene_mesh_bounds()
    if size.z <= 0:
        raise RuntimeError("Cannot scale: zero mesh height")
    scale = TARGET_HEIGHT / size.z
    roots = [obj for obj in bpy.context.scene.objects if obj.parent is None]
    for obj in roots:
        obj.scale = (obj.scale.x * scale, obj.scale.y * scale, obj.scale.z * scale)
    bpy.context.view_layer.update()
    mins2, maxs2, size2 = scene_mesh_bounds()
    # Move bottom to z = -1.0, matching the old Player.glb vertical convention.
    dz = -1.0 - mins2.z
    for obj in roots:
        obj.location.z += dz
    bpy.context.view_layer.update()
    return scale, scene_mesh_bounds()


def decimate_meshes():
    _, polys_before = total_mesh_stats()
    if polys_before <= TARGET_POLYS:
        return 1.0
    ratio = max(0.05, min(1.0, TARGET_POLYS / float(polys_before)))
    for obj in bpy.context.scene.objects:
        if obj.type != "MESH":
            continue
        bpy.context.view_layer.objects.active = obj
        obj.select_set(True)
        dec = obj.modifiers.new("Game_Optimize_Decimate", "DECIMATE")
        dec.ratio = ratio
        dec.use_collapse_triangulate = True
        try:
            obj.modifiers.move(len(obj.modifiers) - 1, 0)
        except Exception:
            pass
        try:
            bpy.ops.object.modifier_apply(modifier=dec.name)
        except Exception as exc:
            log("WARNING: could not apply decimate on", obj.name, exc)
        obj.select_set(False)
    bpy.context.view_layer.update()
    return ratio


def simplify_actions():
    # Removes excessive nearly-redundant points. The tolerances are conservative:
    # small enough to keep poses recognizable, but good at deleting static channels.
    before = 0
    removed = 0
    for action in bpy.data.actions:
        for fc in iter_action_fcurves(action):
            points = fc.keyframe_points
            before += len(points)
            if len(points) <= 2:
                continue
            # First pass: remove exact/near-exact middle points.
            # Iterate backwards so indices stay stable.
            for i in range(len(points) - 2, 0, -1):
                prev = points[i - 1].co
                cur = points[i].co
                nxt = points[i + 1].co
                span = nxt.x - prev.x
                if abs(span) < 1e-6:
                    continue
                t = (cur.x - prev.x) / span
                linear_y = prev.y + (nxt.y - prev.y) * t
                tolerance = 0.0005
                if "rotation_quaternion" in fc.data_path:
                    tolerance = 0.0002
                elif "location" in fc.data_path:
                    tolerance = 0.001
                if abs(cur.y - linear_y) <= tolerance:
                    points.remove(points[i])
                    removed += 1
            try:
                fc.update()
            except Exception:
                pass
    return before, before - removed


def export_glb(path):
    bpy.ops.object.select_all(action="DESELECT")
    for obj in bpy.context.scene.objects:
        if obj.type in {"MESH", "ARMATURE"}:
            obj.select_set(True)

    kwargs = dict(
        filepath=path,
        export_format="GLB",
        use_selection=True,
        export_apply=True,
        export_animations=True,
        export_frame_range=False,
        export_force_sampling=False,
        export_yup=True,
        export_rest_position_armature=False,
    )

    # Blender versions differ here; only pass animation-size optimization if present.
    prop_names = {p.identifier for p in bpy.ops.export_scene.gltf.get_rna_type().properties}
    if "export_optimize_animation_size" in prop_names:
        kwargs["export_optimize_animation_size"] = True
    if "export_optimize_animation_keep_anim_armature" in prop_names:
        kwargs["export_optimize_animation_keep_anim_armature"] = True

    bpy.ops.export_scene.gltf(**kwargs)


def main():
    if "--" not in sys.argv:
        raise RuntimeError("Usage: blender --background file.blend --python script.py -- output.glb")
    output = sys.argv[sys.argv.index("--") + 1]
    remove_scene_junk()
    verts0, polys0 = total_mesh_stats()
    mins0, maxs0, size0 = scene_mesh_bounds()
    log("before", "verts", verts0, "polys", polys0, "height", round(size0.z, 6))

    decimate_ratio = decimate_meshes()
    verts1, polys1 = total_mesh_stats()
    log("decimate_ratio", round(decimate_ratio, 6), "verts", verts1, "polys", polys1)

    scale, (_, _, size_scaled) = scale_to_target_height()
    log("scale", round(scale, 6), "height", round(size_scaled.z, 6))

    keys_before, keys_after = simplify_actions()
    log("keys", keys_before, "->", keys_after)

    export_glb(output)
    log("exported", output, "bytes", os.path.getsize(output))


main()
