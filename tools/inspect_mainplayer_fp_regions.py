import bpy
from collections import Counter, defaultdict


mesh_obj = next(obj for obj in bpy.data.objects if obj.type == "MESH")
arm_obj = next(obj for obj in bpy.data.objects if obj.type == "ARMATURE")

print("MESH", mesh_obj.name, "dimensions", tuple(round(v, 5) for v in mesh_obj.dimensions))
print("MESH_MATRIX", [tuple(round(v, 5) for v in row) for row in mesh_obj.matrix_world])
print("ARM_MATRIX", [tuple(round(v, 5) for v in row) for row in arm_obj.matrix_world])

print("\nBONES")
for bone in arm_obj.data.bones:
    print(
        bone.name,
        "head", tuple(round(v, 5) for v in bone.head_local),
        "tail", tuple(round(v, 5) for v in bone.tail_local),
        "parent", bone.parent.name if bone.parent else "-",
    )

group_names = {group.index: group.name for group in mesh_obj.vertex_groups}
dominant_counts = Counter()
weighted_counts = Counter()
bounds = defaultdict(lambda: [[float("inf")] * 3, [float("-inf")] * 3])

for vertex in mesh_obj.data.vertices:
    if not vertex.groups:
        dominant_counts["<unweighted>"] += 1
        continue

    dominant = max(vertex.groups, key=lambda item: item.weight)
    dominant_name = group_names.get(dominant.group, "<unknown>")
    dominant_counts[dominant_name] += 1
    for item in vertex.groups:
        if item.weight > 0.01:
            weighted_counts[group_names.get(item.group, "<unknown>")] += 1

    low, high = bounds[dominant_name]
    for axis in range(3):
        low[axis] = min(low[axis], vertex.co[axis])
        high[axis] = max(high[axis], vertex.co[axis])

print("\nVERTEX_GROUPS")
for name, count in dominant_counts.most_common():
    low, high = bounds[name]
    print(
        name,
        "dominant", count,
        "weighted", weighted_counts[name],
        "min", tuple(round(v, 5) for v in low),
        "max", tuple(round(v, 5) for v in high),
    )

print("\nMATERIAL_POLYGONS")
material_counts = Counter(poly.material_index for poly in mesh_obj.data.polygons)
for index, count in sorted(material_counts.items()):
    material = mesh_obj.data.materials[index] if index < len(mesh_obj.data.materials) else None
    print(index, material.name if material else "<none>", count)
