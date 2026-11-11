import bpy
from collections import Counter


def point_segment_distance(point, start, end):
    segment = end - start
    if segment.length_squared < 1e-10:
        return (point - start).length
    factor = max(0.0, min(1.0, (point - start).dot(segment) / segment.length_squared))
    return (point - (start + segment * factor)).length


armature = next(obj for obj in bpy.data.objects if obj.type == "ARMATURE")
mesh_obj = next(obj for obj in bpy.data.objects if obj.type == "MESH")
modifier = next(modifier for modifier in mesh_obj.modifiers if modifier.type == "ARMATURE")

transforms = {
    "world": armature.matrix_world.inverted() @ mesh_obj.matrix_world,
    "parent_inverse": mesh_obj.matrix_parent_inverse.inverted(),
    "modifier_bind": armature.matrix_world.inverted() @ mesh_obj.matrix_world,
}

print("MATRICES")
print("mesh_parent_inverse", [tuple(round(value, 5) for value in row) for row in mesh_obj.matrix_parent_inverse])
for name, matrix in transforms.items():
    print(name, [tuple(round(value, 5) for value in row) for row in matrix])

for side, side_sign in (("Left", 1.0), ("Right", -1.0)):
    hand = armature.data.bones[f"mixamorig:{side}Hand"]
    hand_inverse = hand.matrix_local.inverted()
    print("\nSIDE", side)
    for transform_name, transform in transforms.items():
        points = [hand_inverse @ (transform @ vertex.co) for vertex in mesh_obj.data.vertices]
        nearby = [point for point in points if -5.0 <= point.y <= 30.0 and abs(point.x) <= 15.0 and abs(point.z) <= 15.0]
        candidates = [point for point in nearby if point.y >= 7.5 and side_sign * point.x >= 0.8]
        if nearby:
            print(
                transform_name,
                "nearby", len(nearby),
                "candidates", len(candidates),
                "bounds",
                tuple(round(min(point[axis] for point in nearby), 4) for axis in range(3)),
                tuple(round(max(point[axis] for point in nearby), 4) for axis in range(3)),
            )

    print("NEW_BONES")
    chains = {}
    for finger in ("Middle", "Ring", "Pinky"):
        chain = [armature.data.bones.get(f"mixamorig:{side}Hand{finger}{number}") for number in range(1, 5)]
        points = [hand_inverse @ bone.head_local for bone in chain if bone]
        if chain[-1]:
            points.append(hand_inverse @ chain[-1].tail_local)
        chains[finger] = points
        print(
            finger,
            [tuple(round(value, 4) for value in point) for point in points],
        )
    local_points = [hand_inverse @ (transforms["world"] @ vertex.co) for vertex in mesh_obj.data.vertices]
    local_points = [point for point in local_points if point.y >= 7.5 and side_sign * point.x >= 0.8 and abs(point.z) <= 15.0]
    print("LATERAL_HISTOGRAM", sorted(Counter(round(side_sign * point.x) for point in local_points).items()))
    distances = []
    nearest_counts = {finger: 0 for finger in chains}
    for point in local_points:
        choices = {}
        for finger, chain in chains.items():
            choices[finger] = min(
                point_segment_distance(point, chain[index], chain[index + 1])
                for index in range(len(chain) - 1)
            )
        nearest = min(choices, key=choices.get)
        nearest_counts[nearest] += 1
        distances.append(choices[nearest])
    distances.sort()
    print("NEAREST_COUNTS", nearest_counts)
    for percentile in (0.1, 0.25, 0.5, 0.75, 0.9):
        index = int((len(distances) - 1) * percentile)
        print("DISTANCE", percentile, round(distances[index], 5))
