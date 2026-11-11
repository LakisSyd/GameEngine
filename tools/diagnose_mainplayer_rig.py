import math
from collections import Counter

import bmesh
import bpy
from mathutils import Quaternion, Vector
from mathutils.kdtree import KDTree


ARMATURE_NAME = "Main"
MESH_NAME = "Suit_Suit_0.003"
SIDES = ("Left", "Right")


def world_vertices(obj):
    depsgraph = bpy.context.evaluated_depsgraph_get()
    evaluated = obj.evaluated_get(depsgraph)
    mesh = evaluated.to_mesh()
    try:
        return [evaluated.matrix_world @ vertex.co for vertex in mesh.vertices]
    finally:
        evaluated.to_mesh_clear()


def normalized_weights(vertex, group_names):
    weights = []
    for item in vertex.groups:
        name = group_names.get(item.group)
        if name and item.weight > 1e-6:
            weights.append((name, item.weight))
    weights.sort(key=lambda pair: pair[1], reverse=True)
    return weights


armature = bpy.data.objects[ARMATURE_NAME]
mesh_obj = bpy.data.objects[MESH_NAME]
modifier = next(mod for mod in mesh_obj.modifiers if mod.type == "ARMATURE")
group_names = {group.index: group.name for group in mesh_obj.vertex_groups}
edge_face_counts = Counter()
edge_lookup = {tuple(sorted(edge.vertices)): edge.index for edge in mesh_obj.data.edges}
for polygon in mesh_obj.data.polygons:
    for edge_key in polygon.edge_keys:
        edge_face_counts[edge_lookup[tuple(sorted(edge_key))]] += 1

print("OBJECT_RELATIONSHIP")
print("mesh_parent", mesh_obj.parent.name if mesh_obj.parent else "-")
print("armature_parent", armature.parent.name if armature.parent else "-")
print("modifier_object", modifier.object.name if modifier.object else "-")
print("preserve_volume", modifier.use_deform_preserve_volume)
print("mesh_scale", tuple(round(value, 6) for value in mesh_obj.scale))
print("armature_scale", tuple(round(value, 6) for value in armature.scale))

armature.animation_data_clear()
bpy.context.scene.frame_set(1)
for pose_bone in armature.pose.bones:
    pose_bone.location = (0.0, 0.0, 0.0)
    pose_bone.rotation_mode = "QUATERNION"
    pose_bone.rotation_quaternion = Quaternion()
    pose_bone.scale = (1.0, 1.0, 1.0)
bpy.context.view_layer.update()

base_positions = world_vertices(mesh_obj)

print("\nGLOBAL_WEIGHTS")
weight_sums = []
influence_counts = Counter()
bad_sum_groups = Counter()
bad_sum_positions = []
for vertex in mesh_obj.data.vertices:
    weights = normalized_weights(vertex, group_names)
    total = sum(weight for _, weight in weights)
    weight_sums.append(total)
    influence_counts[len(weights)] += 1
    if abs(total - 1.0) > 1e-4:
        bad_sum_groups[weights[0][0] if weights else "<unweighted>"] += 1
        bad_sum_positions.append(mesh_obj.matrix_world @ vertex.co)
print("sum_min", round(min(weight_sums), 6), "sum_max", round(max(weight_sums), 6))
print("sum_not_one", sum(abs(total - 1.0) > 1e-4 for total in weight_sums))
print("more_than_4", sum(count for influences, count in influence_counts.items() if influences > 4))
print("influence_histogram", sorted(influence_counts.items()))
print("bad_sum_groups", bad_sum_groups.most_common())
if bad_sum_positions:
    print(
        "bad_sum_bounds",
        tuple(round(min(point[axis] for point in bad_sum_positions), 6) for axis in range(3)),
        tuple(round(max(point[axis] for point in bad_sum_positions), 6) for axis in range(3)),
    )

print("\nMERGE_DISTANCE_SIMULATION")
for distance in (1e-7, 1e-6, 1e-5, 5e-5, 1e-4, 2.5e-4):
    test_mesh = bmesh.new()
    test_mesh.from_mesh(mesh_obj.data)
    before = len(test_mesh.verts)
    bmesh.ops.remove_doubles(test_mesh, verts=list(test_mesh.verts), dist=distance)
    after = len(test_mesh.verts)
    test_mesh.free()
    print("merge_distance", distance, "removed", before - after, "remaining", after)

for side in SIDES:
    hand_name = f"mixamorig:{side}Hand"
    forearm_name = f"mixamorig:{side}ForeArm"
    hand_bone = armature.data.bones[hand_name]
    wrist_world = armature.matrix_world @ hand_bone.head_local
    radius = max(hand_bone.length * 0.72, 0.035)

    nearby = [
        vertex.index
        for vertex in mesh_obj.data.vertices
        if (mesh_obj.matrix_world @ vertex.co - wrist_world).length <= radius
    ]
    nearby_set = set(nearby)
    nearby_edges = [
        edge for edge in mesh_obj.data.edges
        if edge.vertices[0] in nearby_set and edge.vertices[1] in nearby_set
    ]
    boundary_edges = [edge for edge in nearby_edges if edge_face_counts[edge.index] == 1]
    position_buckets = {}
    for index in nearby:
        position = mesh_obj.data.vertices[index].co
        key = tuple(round(value, 5) for value in position)
        position_buckets.setdefault(key, []).append(index)
    coincident_groups = [indices for indices in position_buckets.values() if len(indices) > 1]
    coincident_mismatches = []
    for indices in coincident_groups:
        weight_maps = []
        for index in indices:
            weights = normalized_weights(mesh_obj.data.vertices[index], group_names)
            total = sum(weight for _, weight in weights)
            weight_maps.append({name: weight / total for name, weight in weights} if total else {})
        names = set().union(*(weight_map.keys() for weight_map in weight_maps))
        mismatch = max(
            sum(abs(first.get(name, 0.0) - second.get(name, 0.0)) for name in names)
            for first in weight_maps
            for second in weight_maps
        )
        coincident_mismatches.append((mismatch, indices, weight_maps))
    coincident_mismatches.sort(reverse=True, key=lambda item: item[0])

    wrong_side = "Right" if side == "Left" else "Left"
    cross_side = []
    suspicious = []
    for index in nearby:
        weights = normalized_weights(mesh_obj.data.vertices[index], group_names)
        wrong_weight = sum(
            weight for name, weight in weights if name.startswith(f"mixamorig:{wrong_side}")
        )
        if wrong_weight > 1e-4:
            cross_side.append((index, wrong_weight, weights[:5]))

        forearm_weight = sum(weight for name, weight in weights if name == forearm_name)
        hand_weight = sum(weight for name, weight in weights if name == hand_name)
        finger_weight = sum(
            weight for name, weight in weights
            if name.startswith(f"mixamorig:{side}Hand") and name != hand_name
        )
        if finger_weight > 0.15 and (mesh_obj.matrix_world @ mesh_obj.data.vertices[index].co - wrist_world).length < radius * 0.55:
            suspicious.append((index, forearm_weight, hand_weight, finger_weight, weights[:5]))

    print(f"\n{side.upper()}_WRIST")
    print("joint", tuple(round(value, 6) for value in wrist_world), "radius", round(radius, 6))
    print("nearby_vertices", len(nearby), "nearby_edges", len(nearby_edges))
    print("boundary_edges", len(boundary_edges), "coincident_groups", len(coincident_groups))
    print("coincident_mismatches", coincident_mismatches[:15])
    print("cross_side_vertices", len(cross_side), cross_side[:10])
    print("finger_weight_near_joint", len(suspicious), suspicious[:15])

    pose_bone = armature.pose.bones[hand_name]
    for axis_name, axis in (("X", Vector((1, 0, 0))), ("Y", Vector((0, 1, 0))), ("Z", Vector((0, 0, 1)))):
        pose_bone.rotation_quaternion = Quaternion(axis, math.radians(25.0))
        bpy.context.view_layer.update()
        posed_positions = world_vertices(mesh_obj)
        stretch = []
        for edge in nearby_edges:
            first, second = edge.vertices
            rest_length = (base_positions[first] - base_positions[second]).length
            posed_length = (posed_positions[first] - posed_positions[second]).length
            if rest_length > 1e-7:
                stretch.append((posed_length / rest_length, edge.index, first, second))
        stretch.sort(reverse=True)
        print("rotation", axis_name, "max_edge_ratio", round(stretch[0][0], 5), "top", [
            (round(ratio, 5), edge, first, second) for ratio, edge, first, second in stretch[:10]
        ])
        for ratio, edge_index, first, second in stretch[:3]:
            print(
                "edge_detail",
                axis_name,
                edge_index,
                "ratio", round(ratio, 5),
                "rest_length", round((base_positions[first] - base_positions[second]).length, 8),
                "posed_length", round((posed_positions[first] - posed_positions[second]).length, 8),
                "vertices", first, second,
                "distance_to_joint", round((base_positions[first] - wrist_world).length, 6), round((base_positions[second] - wrist_world).length, 6),
                "positions", tuple(round(value, 6) for value in base_positions[first]), tuple(round(value, 6) for value in base_positions[second]),
                "weights_a", normalized_weights(mesh_obj.data.vertices[first], group_names),
                "weights_b", normalized_weights(mesh_obj.data.vertices[second], group_names),
            )
        pose_bone.rotation_quaternion = Quaternion()
        bpy.context.view_layer.update()

print("\nBONE_CONSTRAINTS")
for pose_bone in armature.pose.bones:
    if pose_bone.constraints:
        print(pose_bone.name, [(constraint.name, constraint.type) for constraint in pose_bone.constraints])

left_joint = armature.matrix_world @ armature.data.bones["mixamorig:LeftHand"].head_local
right_joint = armature.matrix_world @ armature.data.bones["mixamorig:RightHand"].head_local
center_x = (left_joint.x + right_joint.x) * 0.5
right_indices = [
    vertex.index
    for vertex in mesh_obj.data.vertices
    if (mesh_obj.matrix_world @ vertex.co - right_joint).length <= 0.075
]
tree = KDTree(len(right_indices))
for index in right_indices:
    tree.insert(mesh_obj.matrix_world @ mesh_obj.data.vertices[index].co, index)
tree.balance()
mirror_distances = []
for vertex in mesh_obj.data.vertices:
    world = mesh_obj.matrix_world @ vertex.co
    if (world - left_joint).length > 0.075:
        continue
    mirrored = Vector((2.0 * center_x - world.x, world.y, world.z))
    _, _, distance = tree.find(mirrored)
    mirror_distances.append(distance)
mirror_distances.sort()
print("\nWRIST_SYMMETRY")
print("center_x", round(center_x, 7), "left_vertices", len(mirror_distances), "right_vertices", len(right_indices))
for percentile in (0.5, 0.75, 0.9, 0.95, 0.99, 1.0):
    index = min(len(mirror_distances) - 1, int((len(mirror_distances) - 1) * percentile))
    print("distance_percentile", percentile, round(mirror_distances[index], 7))
for threshold in (0.00025, 0.0005, 0.001, 0.002, 0.004):
    print("within", threshold, sum(distance <= threshold for distance in mirror_distances))
