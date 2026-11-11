from collections import Counter

import bmesh
import bpy


armature = next(obj for obj in bpy.data.objects if obj.type == "ARMATURE")
mesh_obj = next(obj for obj in bpy.data.objects if obj.type == "MESH")
modifier = next(modifier for modifier in mesh_obj.modifiers if modifier.type == "ARMATURE")

print("OBJECTS")
for obj in (armature, mesh_obj):
    print(
        obj.name,
        obj.type,
        "parent", obj.parent.name if obj.parent else "-",
        "location", tuple(round(value, 6) for value in obj.location),
        "rotation", tuple(round(value, 6) for value in obj.rotation_euler),
        "scale", tuple(round(value, 6) for value in obj.scale),
        "dimensions", tuple(round(value, 6) for value in obj.dimensions),
        "matrix", [tuple(round(value, 6) for value in row) for row in obj.matrix_world],
    )
print("MODIFIER", modifier.object.name if modifier.object else "-", "preserve_volume", modifier.use_deform_preserve_volume)

print("\nBONES", len(armature.data.bones))
for bone in armature.data.bones:
    if any(token in bone.name for token in ("Hips", "Spine", "Shoulder", "Arm", "ForeArm", "Hand", "Leg", "Foot")):
        print(
            bone.name,
            "head", tuple(round(value, 6) for value in bone.head_local),
            "tail", tuple(round(value, 6) for value in bone.tail_local),
            "length", round(bone.length, 6),
            "parent", bone.parent.name if bone.parent else "-",
            "deform", bone.use_deform,
        )

bone_names = {bone.name for bone in armature.data.bones}
group_names = {group.index: group.name for group in mesh_obj.vertex_groups}
group_indices = {group.name: group.index for group in mesh_obj.vertex_groups}

weighted_counts = Counter()
dominant_counts = Counter()
weight_sums = []
influence_counts = Counter()
for vertex in mesh_obj.data.vertices:
    weights = [
        (group_names.get(item.group, "<unknown>"), item.weight)
        for item in vertex.groups
        if item.weight > 1e-8
    ]
    weight_sums.append(sum(weight for _, weight in weights))
    influence_counts[len(weights)] += 1
    if weights:
        dominant_counts[max(weights, key=lambda item: item[1])[0]] += 1
    for name, weight in weights:
        if weight > 0.001:
            weighted_counts[name] += 1

print("\nWEIGHTS")
print("vertices", len(mesh_obj.data.vertices), "polygons", len(mesh_obj.data.polygons))
print("sum_min", round(min(weight_sums), 7), "sum_max", round(max(weight_sums), 7))
print("sum_not_one", sum(abs(total - 1.0) > 1e-4 for total in weight_sums))
print("more_than_4", sum(count for influences, count in influence_counts.items() if influences > 4))
print("influence_histogram", sorted(influence_counts.items()))
print("groups_without_bones", sorted(name for name in group_indices if name not in bone_names))
print("bones_without_groups", sorted(name for name in bone_names if name not in group_indices and armature.data.bones[name].use_deform))

print("\nFINGER_GROUPS")
for name in sorted(name for name in group_indices if "Hand" in name):
    print(name, "weighted", weighted_counts[name], "dominant", dominant_counts[name])

print("\nPOSE_SETTINGS")
for pose_bone in armature.pose.bones:
    if any(token in pose_bone.name for token in ("Arm", "ForeArm", "Hand", "Leg", "Foot")):
        print(
            pose_bone.name,
            "ik_stretch", round(pose_bone.ik_stretch, 6),
            "lock_scale", tuple(pose_bone.lock_scale),
            "constraints", [(constraint.name, constraint.type) for constraint in pose_bone.constraints],
        )

print("\nMERGE_SIMULATION")
for distance in (1e-7, 1e-6, 1e-5, 5e-5, 1e-4):
    bm = bmesh.new()
    bm.from_mesh(mesh_obj.data)
    before = len(bm.verts)
    bmesh.ops.remove_doubles(bm, verts=list(bm.verts), dist=distance)
    after = len(bm.verts)
    bm.free()
    print(distance, "removed", before - after, "remaining", after)
