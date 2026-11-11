import math
import sys
from argparse import ArgumentParser
from pathlib import Path

import bmesh
import bpy
from mathutils import Vector


DEFAULT_OUTPUT = Path(r"C:\Users\Lakis\Desktop\MainPlayer_Rigged.blend")
MAX_INFLUENCES = 4
WELD_DISTANCE = 1e-7
CONTROL_PREFIX = "CTRL_"

arguments = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
parser = ArgumentParser()
parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
options = parser.parse_args(arguments)
output_path = options.output.resolve()


def ensure_object_mode():
    if bpy.context.object and bpy.context.object.mode != "OBJECT":
        bpy.ops.object.mode_set(mode="OBJECT")


def closest_point_on_segment(point, start, end):
    segment = end - start
    length_squared = segment.length_squared
    if length_squared < 1e-10:
        return start.copy(), 0.0
    factor = max(0.0, min(1.0, (point - start).dot(segment) / length_squared))
    return start + segment * factor, factor


def polyline_position(point, points):
    lengths = [(points[index + 1] - points[index]).length for index in range(len(points) - 1)]
    cumulative = [0.0]
    for length in lengths:
        cumulative.append(cumulative[-1] + length)

    best_distance = float("inf")
    best_position = 0.0
    for index, length in enumerate(lengths):
        closest, factor = closest_point_on_segment(point, points[index], points[index + 1])
        distance = (point - closest).length
        if distance < best_distance:
            best_distance = distance
            best_position = cumulative[index] + length * factor
    return best_distance, best_position, cumulative


def interpolate_centers(position, centers):
    if position <= centers[0][0]:
        return {centers[0][1]: 1.0}
    if position >= centers[-1][0]:
        return {centers[-1][1]: 1.0}
    for index in range(len(centers) - 1):
        left_position, left_name = centers[index]
        right_position, right_name = centers[index + 1]
        if left_position <= position <= right_position:
            factor = (position - left_position) / max(right_position - left_position, 1e-8)
            return {left_name: 1.0 - factor, right_name: factor}
    return {centers[-1][1]: 1.0}


def make_wire_shape(collection, name, vertices, edges):
    obj = bpy.data.objects.get(name)
    if obj:
        return obj
    mesh = bpy.data.meshes.new(f"{name}_Mesh")
    mesh.from_pydata(vertices, edges, [])
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    collection.objects.link(obj)
    obj.hide_render = True
    obj.hide_set(True)
    return obj


def circle_shape(collection, name, segments=24):
    vertices = []
    edges = []
    for index in range(segments):
        angle = 2.0 * math.pi * index / segments
        vertices.append((math.cos(angle), math.sin(angle), 0.0))
        edges.append((index, (index + 1) % segments))
    return make_wire_shape(collection, name, vertices, edges)


def box_shape(collection, name):
    vertices = [
        (-1, -1, -1), (1, -1, -1), (1, 1, -1), (-1, 1, -1),
        (-1, -1, 1), (1, -1, 1), (1, 1, 1), (-1, 1, 1),
    ]
    edges = [
        (0, 1), (1, 2), (2, 3), (3, 0),
        (4, 5), (5, 6), (6, 7), (7, 4),
        (0, 4), (1, 5), (2, 6), (3, 7),
    ]
    return make_wire_shape(collection, name, vertices, edges)


def diamond_shape(collection, name):
    vertices = [(0, 0, 1), (1, 0, 0), (0, 1, 0), (-1, 0, 0), (0, -1, 0), (0, 0, -1)]
    edges = [(0, 1), (0, 2), (0, 3), (0, 4), (5, 1), (5, 2), (5, 3), (5, 4)]
    return make_wire_shape(collection, name, vertices, edges)


def set_color(data_bone, palette):
    color = getattr(data_bone, "color", None)
    if color is not None:
        color.palette = palette


def set_custom_shape(pose_bone, shape, scale):
    pose_bone.custom_shape = shape
    pose_bone.use_custom_shape_bone_size = False
    pose_bone.custom_shape_scale_xyz = (scale, scale, scale)


def add_influence_driver(constraint, armature, control_name):
    fcurve = constraint.driver_add("influence")
    driver = fcurve.driver
    driver.type = "SCRIPTED"
    variable = driver.variables.new()
    variable.name = "ik_fk"
    variable.type = "SINGLE_PROP"
    variable.targets[0].id = armature
    variable.targets[0].data_path = f'pose.bones["{control_name}"]["IK_FK"]'
    driver.expression = "ik_fk"


armature = next(obj for obj in bpy.data.objects if obj.type == "ARMATURE")
mesh_obj = next(obj for obj in bpy.data.objects if obj.type == "MESH")
armature_modifier = next(
    (modifier for modifier in mesh_obj.modifiers if modifier.type == "ARMATURE" and modifier.object == armature),
    None,
)
if not armature_modifier:
    raise RuntimeError("The character mesh has no matching Armature modifier")

# Weld exact export duplicates while averaging their skin weights. UV data is
# stored per loop, so UV seams remain intact after the vertices are shared.
bone_names_before = {bone.name for bone in armature.data.bones}
deform_group_indices = {
    group.index for group in mesh_obj.vertex_groups if group.name in bone_names_before
}
bm = bmesh.new()
bm.from_mesh(mesh_obj.data)
deform_layer = bm.verts.layers.deform.verify()
merge_result = bmesh.ops.find_doubles(bm, verts=list(bm.verts), dist=WELD_DISTANCE)
target_map = merge_result.get("targetmap", {})
clusters = {}
for duplicate, target in target_map.items():
    clusters.setdefault(target, [target]).append(duplicate)

for members in clusters.values():
    averaged = {}
    for member in members:
        for group_index, weight in member[deform_layer].items():
            if group_index in deform_group_indices and weight > 1e-8:
                averaged[group_index] = averaged.get(group_index, 0.0) + weight / len(members)
    kept = sorted(averaged.items(), key=lambda item: item[1], reverse=True)[:MAX_INFLUENCES]
    total = sum(weight for _, weight in kept)
    for member in members:
        deform = member[deform_layer]
        for group_index in [index for index in deform.keys() if index in deform_group_indices]:
            del deform[group_index]
        if total > 1e-8:
            for group_index, weight in kept:
                deform[group_index] = weight / total

vertices_before_weld = len(bm.verts)
bmesh.ops.remove_doubles(bm, verts=list(bm.verts), dist=WELD_DISTANCE)
bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
vertices_after_weld = len(bm.verts)
bm.to_mesh(mesh_obj.data)
bm.free()
mesh_obj.data.update()

# Capture the existing index-finger chains in each hand's local coordinates.
finger_specs = {
    "Middle": (1.55, 1.08, 0.15),
    "Ring": (3.15, 0.98, -0.20),
    "Pinky": (4.65, 0.82, -0.75),
}
finger_points = {}
for side, side_sign in (("Left", 1.0), ("Right", -1.0)):
    hand_bone = armature.data.bones[f"mixamorig:{side}Hand"]
    hand_inverse = hand_bone.matrix_local.inverted()
    index_bones = [armature.data.bones[f"mixamorig:{side}HandIndex{number}"] for number in range(1, 5)]
    reference_armature = [
        index_bones[0].head_local.copy(),
        (index_bones[0].tail_local + index_bones[1].head_local) * 0.5,
        (index_bones[1].tail_local + index_bones[2].head_local) * 0.5,
        (index_bones[2].tail_local + index_bones[3].head_local) * 0.5,
        index_bones[3].tail_local.copy(),
    ]
    reference = [hand_inverse @ point for point in reference_armature]
    base_reference = reference[0]
    total_forward = max(reference[-1].y - base_reference.y, 1e-8)

    for finger, (lateral_offset, length_scale, forward_offset) in finger_specs.items():
        points_hand = []
        for point in reference:
            relative = point - base_reference
            forward_factor = relative.y / total_forward
            points_hand.append(
                base_reference
                + Vector((side_sign * lateral_offset, forward_offset, 0.0))
                + Vector((relative.x, relative.y * length_scale, relative.z * length_scale))
                + Vector((side_sign * 0.18 * forward_factor, 0.0, 0.0))
            )
        finger_points[(side, finger)] = {
            "hand": points_hand,
            "armature": [hand_bone.matrix_local @ point for point in points_hand],
        }

ensure_object_mode()
bpy.ops.object.select_all(action="DESELECT")
armature.select_set(True)
bpy.context.view_layer.objects.active = armature
bpy.ops.object.mode_set(mode="EDIT")

for edit_bone in list(armature.data.edit_bones):
    if edit_bone.name.startswith(CONTROL_PREFIX):
        armature.data.edit_bones.remove(edit_bone)
    elif any(f"Hand{finger}" in edit_bone.name for finger in finger_specs):
        armature.data.edit_bones.remove(edit_bone)

new_finger_bones = []
for side in ("Left", "Right"):
    hand_parent = armature.data.edit_bones[f"mixamorig:{side}Hand"]
    index_rolls = [armature.data.edit_bones[f"mixamorig:{side}HandIndex{number}"].roll for number in range(1, 5)]
    for finger in finger_specs:
        points = finger_points[(side, finger)]["armature"]
        parent = hand_parent
        for number in range(1, 5):
            name = f"mixamorig:{side}Hand{finger}{number}"
            edit_bone = armature.data.edit_bones.new(name)
            edit_bone.head = points[number - 1]
            edit_bone.tail = points[number]
            edit_bone.roll = index_rolls[number - 1]
            edit_bone.parent = parent
            edit_bone.use_connect = False
            edit_bone.use_deform = number < 4
            parent = edit_bone
            new_finger_bones.append(name)

# Add non-deforming IK controls in the same armature.
hips = armature.data.edit_bones["mixamorig:Hips"]
head = armature.data.edit_bones["mixamorig:Head"]
up_direction = (head.tail - hips.head).normalized()
master = armature.data.edit_bones.new("CTRL_Master")
master.head = hips.head.copy()
master.tail = master.head + up_direction * 12.0
master.use_deform = False

control_specs = {}
for side, suffix in (("Left", "L"), ("Right", "R")):
    upper_arm = armature.data.edit_bones[f"mixamorig:{side}Arm"]
    forearm = armature.data.edit_bones[f"mixamorig:{side}ForeArm"]
    hand = armature.data.edit_bones[f"mixamorig:{side}Hand"]
    hand_control = armature.data.edit_bones.new(f"CTRL_Hand.{suffix}")
    hand_control.head = hand.head.copy()
    hand_control.tail = hand.tail.copy()
    hand_control.roll = hand.roll
    hand_control.parent = master
    hand_control.use_deform = False

    shoulder = upper_arm.head.copy()
    elbow = forearm.head.copy()
    wrist = hand.head.copy()
    shoulder_to_wrist = wrist - shoulder
    projection = shoulder + shoulder_to_wrist * (
        (elbow - shoulder).dot(shoulder_to_wrist) / max(shoulder_to_wrist.length_squared, 1e-8)
    )
    pole_direction = elbow - projection
    if pole_direction.length < 1.0:
        pole_direction = Vector((0.0, 0.0, -1.0))
    else:
        pole_direction.normalize()
    elbow_control = armature.data.edit_bones.new(f"CTRL_Elbow.{suffix}")
    elbow_control.head = elbow + pole_direction * 35.0
    elbow_control.tail = elbow_control.head + up_direction * 5.0
    elbow_control.parent = master
    elbow_control.use_deform = False

    upper_leg = armature.data.edit_bones[f"mixamorig:{side}UpLeg"]
    lower_leg = armature.data.edit_bones[f"mixamorig:{side}Leg"]
    foot = armature.data.edit_bones[f"mixamorig:{side}Foot"]
    foot_control = armature.data.edit_bones.new(f"CTRL_Foot.{suffix}")
    foot_control.head = foot.head.copy()
    foot_control.tail = foot.tail.copy()
    foot_control.roll = foot.roll
    foot_control.parent = master
    foot_control.use_deform = False

    hip = upper_leg.head.copy()
    knee = lower_leg.head.copy()
    ankle = foot.head.copy()
    hip_to_ankle = ankle - hip
    projection = hip + hip_to_ankle * (
        (knee - hip).dot(hip_to_ankle) / max(hip_to_ankle.length_squared, 1e-8)
    )
    pole_direction = knee - projection
    if pole_direction.length < 1.0:
        pole_direction = Vector((0.0, 0.0, -1.0))
    else:
        pole_direction.normalize()
    knee_control = armature.data.edit_bones.new(f"CTRL_Knee.{suffix}")
    knee_control.head = knee + pole_direction * 45.0
    knee_control.tail = knee_control.head + up_direction * 6.0
    knee_control.parent = master
    knee_control.use_deform = False

    control_specs[side] = {
        "hand": hand_control.name,
        "elbow": elbow_control.name,
        "foot": foot_control.name,
        "knee": knee_control.name,
    }

bpy.ops.object.mode_set(mode="OBJECT")

# Create missing vertex groups and skin the new fingers by proximity to their
# generated centerlines. Existing thumb and index weights are preserved.
groups_by_name = {group.name: group for group in mesh_obj.vertex_groups}
for side in ("Left", "Right"):
    for finger in finger_specs:
        for number in range(1, 4):
            name = f"mixamorig:{side}Hand{finger}{number}"
            groups_by_name[name] = groups_by_name.get(name) or mesh_obj.vertex_groups.new(name=name)

armature_inverse_world = armature.matrix_world.inverted()
mesh_to_armature = armature_inverse_world @ mesh_obj.matrix_world
assigned_counts = {(side, finger): 0 for side in ("Left", "Right") for finger in finger_specs}

for side, side_sign in (("Left", 1.0), ("Right", -1.0)):
    hand_bone = armature.data.bones[f"mixamorig:{side}Hand"]
    hand_inverse = hand_bone.matrix_local.inverted()
    hand_group_name = hand_bone.name
    thumb_prefix = f"mixamorig:{side}HandThumb"
    index_prefix = f"mixamorig:{side}HandIndex"

    for vertex in mesh_obj.data.vertices:
        armature_position = mesh_to_armature @ vertex.co
        hand_position = hand_inverse @ armature_position
        lateral = side_sign * hand_position.x
        if hand_position.y < 7.5 or lateral < 0.8:
            continue

        thumb_weight = 0.0
        index_weight = 0.0
        for item in vertex.groups:
            group_name = mesh_obj.vertex_groups[item.group].name
            if group_name.startswith(thumb_prefix):
                thumb_weight += item.weight
            elif group_name.startswith(index_prefix):
                index_weight += item.weight
        if thumb_weight > 0.1 or (index_weight > 0.1 and lateral < 1.2):
            continue

        nearest_finger = None
        nearest_distance = float("inf")
        nearest_position = 0.0
        nearest_cumulative = None
        for finger in finger_specs:
            points = finger_points[(side, finger)]["hand"]
            distance, position, cumulative = polyline_position(hand_position, points)
            if distance < nearest_distance:
                nearest_finger = finger
                nearest_distance = distance
                nearest_position = position
                nearest_cumulative = cumulative
        if nearest_finger is None or nearest_distance > 2.2:
            continue

        points = finger_points[(side, nearest_finger)]["hand"]
        first_length = (points[1] - points[0]).length
        centers = [
            (-first_length * 0.5, hand_group_name),
            ((nearest_cumulative[0] + nearest_cumulative[1]) * 0.5, f"mixamorig:{side}Hand{nearest_finger}1"),
            ((nearest_cumulative[1] + nearest_cumulative[2]) * 0.5, f"mixamorig:{side}Hand{nearest_finger}2"),
            ((nearest_cumulative[2] + nearest_cumulative[4]) * 0.5, f"mixamorig:{side}Hand{nearest_finger}3"),
        ]
        new_weights = interpolate_centers(nearest_position, centers)

        for item in list(vertex.groups):
            group_name = mesh_obj.vertex_groups[item.group].name
            if group_name in bone_names_before or any(f"Hand{finger}" in group_name for finger in finger_specs):
                mesh_obj.vertex_groups[item.group].remove([vertex.index])
        for group_name, weight in new_weights.items():
            groups_by_name[group_name].add([vertex.index], weight, "REPLACE")
        assigned_counts[(side, nearest_finger)] += 1

# Enforce the engine-friendly four-influence limit after adding finger weights.
bone_names_after = {bone.name for bone in armature.data.bones if bone.use_deform}
group_names = {group.index: group.name for group in mesh_obj.vertex_groups}
limited_vertices = 0
for vertex in mesh_obj.data.vertices:
    weights = [
        (item.group, item.weight)
        for item in vertex.groups
        if group_names.get(item.group) in bone_names_after and item.weight > 1e-8
    ]
    weights.sort(key=lambda item: item[1], reverse=True)
    kept = weights[:MAX_INFLUENCES]
    if len(weights) > MAX_INFLUENCES:
        limited_vertices += 1
    total = sum(weight for _, weight in kept)
    for group_index, _ in weights:
        mesh_obj.vertex_groups[group_index].remove([vertex.index])
    if total > 1e-8:
        for group_index, weight in kept:
            mesh_obj.vertex_groups[group_index].add([vertex.index], weight / total, "REPLACE")

ensure_object_mode()
bpy.ops.object.select_all(action="DESELECT")
armature.select_set(True)
bpy.context.view_layer.objects.active = armature
bpy.ops.object.mode_set(mode="POSE")

# Constraints and transform locks prevent accidental limb and mesh stretching.
for pose_bone in armature.pose.bones:
    if pose_bone.name.startswith(CONTROL_PREFIX):
        continue
    pose_bone.ik_stretch = 0.0
    pose_bone.lock_scale = (True, True, True)
    data_bone = pose_bone.bone
    if hasattr(data_bone, "inherit_scale"):
        data_bone.inherit_scale = "NONE"
    for constraint in list(pose_bone.constraints):
        if constraint.name.startswith("CTRL_"):
            pose_bone.constraints.remove(constraint)
    if data_bone.use_deform:
        scale_limit = pose_bone.constraints.new("LIMIT_SCALE")
        scale_limit.name = "CTRL_No_Stretch"
        scale_limit.owner_space = "LOCAL"
        scale_limit.use_min_x = scale_limit.use_min_y = scale_limit.use_min_z = True
        scale_limit.use_max_x = scale_limit.use_max_y = scale_limit.use_max_z = True
        scale_limit.min_x = scale_limit.min_y = scale_limit.min_z = 1.0
        scale_limit.max_x = scale_limit.max_y = scale_limit.max_z = 1.0

for side, suffix in (("Left", "L"), ("Right", "R")):
    specs = control_specs[side]
    for key in ("hand", "foot"):
        control = armature.pose.bones[specs[key]]
        control["IK_FK"] = 0.0
        control.id_properties_ui("IK_FK").update(
            min=0.0,
            max=1.0,
            soft_min=0.0,
            soft_max=1.0,
            description="0 = existing/FK animation, 1 = IK controller",
        )

    upper_arm = armature.pose.bones[f"mixamorig:{side}Arm"]
    forearm = armature.pose.bones[f"mixamorig:{side}ForeArm"]
    hand = armature.pose.bones[f"mixamorig:{side}Hand"]
    arm_ik = forearm.constraints.new("IK")
    arm_ik.name = "CTRL_Arm_IK"
    arm_ik.target = armature
    arm_ik.subtarget = specs["hand"]
    arm_ik.pole_target = armature
    arm_ik.pole_subtarget = specs["elbow"]
    arm_ik.chain_count = 2
    arm_ik.use_stretch = False
    add_influence_driver(arm_ik, armature, specs["hand"])

    hand_rotation = hand.constraints.new("COPY_ROTATION")
    hand_rotation.name = "CTRL_Hand_Rotation"
    hand_rotation.target = armature
    hand_rotation.subtarget = specs["hand"]
    hand_rotation.target_space = "WORLD"
    hand_rotation.owner_space = "WORLD"
    add_influence_driver(hand_rotation, armature, specs["hand"])

    hand_limit = armature.pose.bones[specs["hand"]].constraints.new("LIMIT_DISTANCE")
    hand_limit.name = "CTRL_Arm_Reach"
    hand_limit.target = armature
    hand_limit.subtarget = upper_arm.name
    hand_limit.distance = (upper_arm.bone.length + forearm.bone.length) * 1.005
    hand_limit.limit_mode = "LIMITDIST_INSIDE"

    upper_leg = armature.pose.bones[f"mixamorig:{side}UpLeg"]
    lower_leg = armature.pose.bones[f"mixamorig:{side}Leg"]
    foot = armature.pose.bones[f"mixamorig:{side}Foot"]
    leg_ik = lower_leg.constraints.new("IK")
    leg_ik.name = "CTRL_Leg_IK"
    leg_ik.target = armature
    leg_ik.subtarget = specs["foot"]
    leg_ik.pole_target = armature
    leg_ik.pole_subtarget = specs["knee"]
    leg_ik.chain_count = 2
    leg_ik.use_stretch = False
    add_influence_driver(leg_ik, armature, specs["foot"])

    foot_rotation = foot.constraints.new("COPY_ROTATION")
    foot_rotation.name = "CTRL_Foot_Rotation"
    foot_rotation.target = armature
    foot_rotation.subtarget = specs["foot"]
    foot_rotation.target_space = "WORLD"
    foot_rotation.owner_space = "WORLD"
    add_influence_driver(foot_rotation, armature, specs["foot"])

    foot_limit = armature.pose.bones[specs["foot"]].constraints.new("LIMIT_DISTANCE")
    foot_limit.name = "CTRL_Leg_Reach"
    foot_limit.target = armature
    foot_limit.subtarget = upper_leg.name
    foot_limit.distance = (upper_leg.bone.length + lower_leg.bone.length) * 1.005
    foot_limit.limit_mode = "LIMITDIST_INSIDE"

for control_name in [name for name in armature.pose.bones.keys() if name.startswith(CONTROL_PREFIX)]:
    control = armature.pose.bones[control_name]
    control.lock_scale = (True, True, True)
    if "Elbow" in control_name or "Knee" in control_name:
        control.lock_rotation = (True, True, True)

bpy.ops.object.mode_set(mode="OBJECT")

# Bone collections and custom shapes keep the rig readable without changing
# the export-bone names expected by the engine.
game_collection = armature.data.collections.get("Game Bones") or armature.data.collections.new("Game Bones")
control_collection = armature.data.collections.get("Controllers") or armature.data.collections.new("Controllers")
finger_collection = armature.data.collections.get("Finger FK") or armature.data.collections.new("Finger FK")
for bone in armature.data.bones:
    if bone.name.startswith(CONTROL_PREFIX):
        control_collection.assign(bone)
        set_color(bone, "THEME03")
    else:
        game_collection.assign(bone)
        if "Hand" in bone.name and bone.name != "mixamorig:LeftHand" and bone.name != "mixamorig:RightHand":
            finger_collection.assign(bone)
            set_color(bone, "THEME05")

shape_collection = bpy.data.collections.get("RIG_Shapes")
if not shape_collection:
    shape_collection = bpy.data.collections.new("RIG_Shapes")
    bpy.context.scene.collection.children.link(shape_collection)
shape_collection.hide_render = True
root_shape = circle_shape(shape_collection, "WGT_Root", 32)
fk_shape = circle_shape(shape_collection, "WGT_FK", 20)
hand_shape = box_shape(shape_collection, "WGT_Hand")
pole_shape = diamond_shape(shape_collection, "WGT_Pole")

set_custom_shape(armature.pose.bones["CTRL_Master"], root_shape, 12.0)
for side, suffix in (("Left", "L"), ("Right", "R")):
    set_custom_shape(armature.pose.bones[f"CTRL_Hand.{suffix}"], hand_shape, 3.4)
    set_custom_shape(armature.pose.bones[f"CTRL_Elbow.{suffix}"], pole_shape, 2.8)
    set_custom_shape(armature.pose.bones[f"CTRL_Foot.{suffix}"], hand_shape, 4.2)
    set_custom_shape(armature.pose.bones[f"CTRL_Knee.{suffix}"], pole_shape, 3.2)

fk_names = [
    "mixamorig:Hips", "mixamorig:Spine", "mixamorig:Spine1", "mixamorig:Spine2",
    "mixamorig:Neck", "mixamorig:Head",
    "mixamorig:LeftShoulder", "mixamorig:LeftArm", "mixamorig:LeftForeArm", "mixamorig:LeftHand",
    "mixamorig:RightShoulder", "mixamorig:RightArm", "mixamorig:RightForeArm", "mixamorig:RightHand",
    "mixamorig:LeftUpLeg", "mixamorig:LeftLeg", "mixamorig:LeftFoot",
    "mixamorig:RightUpLeg", "mixamorig:RightLeg", "mixamorig:RightFoot",
]
for name in fk_names:
    pose_bone = armature.pose.bones.get(name)
    if pose_bone:
        set_custom_shape(pose_bone, root_shape if name == "mixamorig:Hips" else fk_shape, 7.0 if name == "mixamorig:Hips" else 3.0)
        set_color(pose_bone.bone, "THEME04")

for pose_bone in armature.pose.bones:
    if "Hand" in pose_bone.name and any(char.isdigit() for char in pose_bone.name):
        set_custom_shape(pose_bone, fk_shape, 1.2)

armature.show_in_front = True
armature.data.display_type = "BBONE"
armature["Rig_Type"] = "Mixamo game rig with complete finger FK and non-stretch IK"
armature["Export_Note"] = "Bake IK to deform bones and export deformation bones only"
armature_modifier.use_deform_preserve_volume = False

for action in bpy.data.actions:
    action.use_fake_user = True

bpy.ops.wm.save_as_mainfile(filepath=str(output_path), compress=True)

print("OUTPUT", output_path)
print("WELDED_VERTICES", vertices_before_weld - vertices_after_weld)
print("ASSIGNED_FINGER_VERTICES", sorted((f"{side}_{finger}", count) for (side, finger), count in assigned_counts.items()))
print("NEW_FINGER_BONES", sorted(new_finger_bones))
print("LIMITED_VERTICES", limited_vertices)
print("CONTROL_BONES", sorted(name for name in armature.data.bones.keys() if name.startswith(CONTROL_PREFIX)))
print("TOTAL_BONES", len(armature.data.bones))
print("ACTIONS", sorted(action.name for action in bpy.data.actions))
