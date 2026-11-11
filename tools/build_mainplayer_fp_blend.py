import bpy
from pathlib import Path


OUTPUT = Path(r"C:\Users\Lakis\Desktop\MainPlayer_FP.blend")
ARM_WEIGHT_THRESHOLD = 0.45


mesh_obj = next(obj for obj in bpy.data.objects if obj.type == "MESH")
arm_obj = next(obj for obj in bpy.data.objects if obj.type == "ARMATURE")
original_vertex_count = len(mesh_obj.data.vertices)
original_polygon_count = len(mesh_obj.data.polygons)

group_names = {group.index: group.name for group in mesh_obj.vertex_groups}


def is_arm_group(name):
    if not (name.startswith("mixamorig:Left") or name.startswith("mixamorig:Right")):
        return False
    lower = name.lower()
    return not any(part in lower for part in ("upleg", "leg", "foot", "toe"))


keep_vertices = set()
for vertex in mesh_obj.data.vertices:
    arm_weight = sum(
        item.weight
        for item in vertex.groups
        if is_arm_group(group_names.get(item.group, ""))
    )
    if arm_weight >= ARM_WEIGHT_THRESHOLD:
        keep_vertices.add(vertex.index)

if not keep_vertices:
    raise RuntimeError("First-person selection is empty")

bpy.ops.object.mode_set(mode="OBJECT") if bpy.context.object and bpy.context.object.mode != "OBJECT" else None
bpy.ops.object.select_all(action="DESELECT")
mesh_obj.select_set(True)
bpy.context.view_layer.objects.active = mesh_obj

for vertex in mesh_obj.data.vertices:
    vertex.select = vertex.index not in keep_vertices

bpy.ops.object.mode_set(mode="EDIT")
bpy.ops.mesh.delete(type="VERT")
bpy.ops.mesh.select_all(action="SELECT")
bpy.ops.mesh.delete_loose(use_verts=True, use_edges=True, use_faces=False)
bpy.ops.mesh.normals_make_consistent(inside=False)
bpy.ops.object.mode_set(mode="OBJECT")

remaining_vertex_count = len(mesh_obj.data.vertices)
remaining_polygon_count = len(mesh_obj.data.polygons)
if remaining_vertex_count < 1000 or remaining_polygon_count < 1000:
    raise RuntimeError(
        f"Unexpectedly small FP mesh: {remaining_vertex_count} vertices, "
        f"{remaining_polygon_count} polygons"
    )

# Leave only the actual export pair. Both objects already have identity world
# transforms, so removing the disconnected import hierarchy is safe.
for obj in list(bpy.data.objects):
    if obj not in {mesh_obj, arm_obj}:
        bpy.data.objects.remove(obj, do_unlink=True)

# Remove material slots no longer referenced by the trimmed mesh. The skin and
# suit slots remain; hair and eyes disappear with the head geometry.
used_material_indices = {poly.material_index for poly in mesh_obj.data.polygons}
for slot_index in reversed(range(len(mesh_obj.data.materials))):
    if slot_index not in used_material_indices:
        mesh_obj.data.materials.pop(index=slot_index)

# Keep every animation explicitly, even when no action is currently active.
def iter_action_fcurves(action):
    yielded = set()
    if hasattr(action, "fcurves"):
        for curve in action.fcurves:
            yielded.add(id(curve))
            yield curve
    for layer in getattr(action, "layers", []):
        for strip in getattr(layer, "strips", []):
            channelbag = getattr(strip, "channelbag", None)
            if not callable(channelbag):
                continue
            for slot in getattr(action, "slots", []):
                try:
                    bag = channelbag(slot)
                except Exception:
                    bag = None
                if not bag:
                    continue
                for curve in getattr(bag, "fcurves", []):
                    if id(curve) not in yielded:
                        yielded.add(id(curve))
                        yield curve


hips_location_path = 'pose.bones["mixamorig:Hips"].location'
for action in bpy.data.actions:
    action.use_fake_user = True
    for curve in iter_action_fcurves(action):
        if curve.data_path != hips_location_path:
            continue
        for key in curve.keyframe_points:
            key.co.y = 0.0
            key.handle_left.y = 0.0
            key.handle_right.y = 0.0
        curve.update()

for _ in range(3):
    bpy.ops.outliner.orphans_purge(do_local_ids=True, do_linked_ids=True, do_recursive=True)

bpy.ops.wm.save_as_mainfile(filepath=str(OUTPUT), compress=True)

print("OUTPUT", OUTPUT)
print("VERTICES", original_vertex_count, "->", remaining_vertex_count)
print("POLYGONS", original_polygon_count, "->", remaining_polygon_count)
print("BONES", len(arm_obj.data.bones))
print("ACTIONS", sorted(action.name for action in bpy.data.actions))
print("OBJECTS", [(obj.name, obj.type) for obj in bpy.data.objects])
print("MATERIALS", [mat.name for mat in mesh_obj.data.materials])
