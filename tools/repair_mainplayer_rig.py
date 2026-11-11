import math
import sys
from argparse import ArgumentParser
from pathlib import Path

import bmesh
import bpy
from mathutils import Vector
from mathutils.kdtree import KDTree


DEFAULT_OUTPUT = Path(r"C:\Users\Lakis\Desktop\MainPlayer_FP_Rigged.blend")
ARMATURE_NAME = "Main"
MESH_NAME = "Suit_Suit_0.003"
CONTROL_PREFIX = "CTRL_"
MAX_DEFORM_INFLUENCES = 4
DEFAULT_WELD_DISTANCE = 1e-4


arguments = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
parser = ArgumentParser()
parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
parser.add_argument("--weld-distance", type=float, default=DEFAULT_WELD_DISTANCE)
parser.add_argument("--mirror-distance", type=float, default=0.004)
parser.add_argument("--no-mirror-left-wrist", action="store_true")
parser.add_argument("--no-clean-wrist-weights", action="store_true")
options = parser.parse_args(arguments)
OUTPUT = options.output.resolve()
WELD_DISTANCE = options.weld_distance


def ensure_object_mode():
    if bpy.context.object and bpy.context.object.mode != "OBJECT":
        bpy.ops.object.mode_set(mode="OBJECT")


def make_wire_shape(name, vertices, edges):
    obj = bpy.data.objects.get(name)
    if obj:
        return obj

    mesh = bpy.data.meshes.new(f"{name}_Mesh")
    mesh.from_pydata(vertices, edges, [])
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    shape_collection.objects.link(obj)
    obj.hide_render = True
    obj.hide_set(True)
    return obj


def circle_shape(name, radius=1.0, segments=24):
    vertices = []
    edges = []
    for index in range(segments):
        angle = 2.0 * math.pi * index / segments
        vertices.append((radius * math.cos(angle), radius * math.sin(angle), 0.0))
        edges.append((index, (index + 1) % segments))
    return make_wire_shape(name, vertices, edges)


def box_shape(name):
    vertices = [
        (-1, -1, -1), (1, -1, -1), (1, 1, -1), (-1, 1, -1),
        (-1, -1, 1), (1, -1, 1), (1, 1, 1), (-1, 1, 1),
    ]
    edges = [
        (0, 1), (1, 2), (2, 3), (3, 0),
        (4, 5), (5, 6), (6, 7), (7, 4),
        (0, 4), (1, 5), (2, 6), (3, 7),
    ]
    return make_wire_shape(name, vertices, edges)


def diamond_shape(name):
    vertices = [(0, 0, 1), (1, 0, 0), (0, 1, 0), (-1, 0, 0), (0, -1, 0), (0, 0, -1)]
    edges = [(0, 1), (0, 2), (0, 3), (0, 4), (5, 1), (5, 2), (5, 3), (5, 4)]
    return make_wire_shape(name, vertices, edges)


def set_color(data_bone, palette):
    color = getattr(data_bone, "color", None)
    if color is not None:
        color.palette = palette


def set_custom_shape(pose_bone, shape, scale):
    pose_bone.custom_shape = shape
    pose_bone.use_custom_shape_bone_size = False
    pose_bone.custom_shape_scale_xyz = (scale, scale, scale)


def add_influence_driver(constraint, armature, control_bone_name):
    fcurve = constraint.driver_add("influence")
    driver = fcurve.driver
    driver.type = "SCRIPTED"
    variable = driver.variables.new()
    variable.name = "ik_fk"
    variable.type = "SINGLE_PROP"
    target = variable.targets[0]
    target.id = armature
    target.data_path = f'pose.bones["{control_bone_name}"]["IK_FK"]'
    driver.expression = "ik_fk"


armature = bpy.data.objects.get(ARMATURE_NAME)
mesh_obj = bpy.data.objects.get(MESH_NAME)
if not armature or armature.type != "ARMATURE":
    raise RuntimeError(f"Armature {ARMATURE_NAME!r} was not found")
if not mesh_obj or mesh_obj.type != "MESH":
    raise RuntimeError(f"Mesh {MESH_NAME!r} was not found")

armature_modifier = next(
    (modifier for modifier in mesh_obj.modifiers if modifier.type == "ARMATURE" and modifier.object == armature),
    None,
)
if not armature_modifier:
    raise RuntimeError("Expected Armature modifier was not found")

# Repair the backwards object hierarchy while preserving the world transform.
if armature.parent == mesh_obj:
    world_matrix = armature.matrix_world.copy()
    armature.parent = None
    armature.matrix_world = world_matrix

# Normalize deform weights and keep the four strongest influences expected by
# common real-time skinning paths. The broken left wrist retains its authored
# blend ratios; only the missing normalization is restored.
deform_bone_names = {bone.name for bone in armature.data.bones if bone.use_deform}
deform_group_indices = {
    group.index for group in mesh_obj.vertex_groups if group.name in deform_bone_names
}
group_names = {group.index: group.name for group in mesh_obj.vertex_groups}
groups_by_name = {group.name: group for group in mesh_obj.vertex_groups}

mirrored_vertices = 0
if not options.no_mirror_left_wrist:
    left_joint = mesh_obj.matrix_world.inverted() @ (
        armature.matrix_world @ armature.data.bones["mixamorig:LeftHand"].head_local
    )
    right_joint = mesh_obj.matrix_world.inverted() @ (
        armature.matrix_world @ armature.data.bones["mixamorig:RightHand"].head_local
    )
    center_x = (left_joint.x + right_joint.x) * 0.5
    wrist_radius = 0.075
    right_indices = [
        vertex.index
        for vertex in mesh_obj.data.vertices
        if (vertex.co - right_joint).length <= wrist_radius
    ]
    tree = KDTree(len(right_indices))
    for index in right_indices:
        tree.insert(mesh_obj.data.vertices[index].co, index)
    tree.balance()

    for target_vertex in mesh_obj.data.vertices:
        if (target_vertex.co - left_joint).length > wrist_radius:
            continue
        mirrored_position = Vector((2.0 * center_x - target_vertex.co.x, target_vertex.co.y, target_vertex.co.z))
        _, source_index, distance = tree.find(mirrored_position)
        if source_index is None or distance > options.mirror_distance:
            continue

        source_weights = []
        for item in mesh_obj.data.vertices[source_index].groups:
            source_name = group_names.get(item.group)
            if not source_name or source_name not in deform_bone_names or item.weight <= 1e-8:
                continue
            target_name = source_name.replace("mixamorig:Right", "mixamorig:Left", 1)
            if target_name in groups_by_name:
                source_weights.append((groups_by_name[target_name], item.weight))
        if not source_weights:
            continue

        for item in list(target_vertex.groups):
            if item.group in deform_group_indices:
                mesh_obj.vertex_groups[item.group].remove([target_vertex.index])
        for group, weight in source_weights:
            group.add([target_vertex.index], weight, "REPLACE")
        mirrored_vertices += 1

cleaned_wrist_vertices = 0
if not options.no_clean_wrist_weights:
    blend_half_width = 0.022
    blend_radius = 0.052
    for side in ("Left", "Right"):
        forearm_bone = armature.data.bones[f"mixamorig:{side}ForeArm"]
        hand_bone = armature.data.bones[f"mixamorig:{side}Hand"]
        joint = mesh_obj.matrix_world.inverted() @ (armature.matrix_world @ hand_bone.head_local)
        direction = mesh_obj.matrix_world.inverted().to_3x3() @ (
            armature.matrix_world.to_3x3() @ (hand_bone.tail_local - forearm_bone.head_local)
        )
        direction.normalize()
        forearm_group = groups_by_name[forearm_bone.name]
        hand_group = groups_by_name[hand_bone.name]

        for vertex in mesh_obj.data.vertices:
            offset = vertex.co - joint
            axial = offset.dot(direction)
            radial = (offset - direction * axial).length
            if abs(axial) > blend_half_width or radial > blend_radius:
                continue

            factor = (axial + blend_half_width) / (2.0 * blend_half_width)
            factor = max(0.0, min(1.0, factor))
            hand_weight = factor * factor * (3.0 - 2.0 * factor)
            forearm_weight = 1.0 - hand_weight

            for item in list(vertex.groups):
                if item.group in deform_group_indices:
                    mesh_obj.vertex_groups[item.group].remove([vertex.index])
            if forearm_weight > 1e-8:
                forearm_group.add([vertex.index], forearm_weight, "REPLACE")
            if hand_weight > 1e-8:
                hand_group.add([vertex.index], hand_weight, "REPLACE")
            cleaned_wrist_vertices += 1

bm = bmesh.new()
bm.from_mesh(mesh_obj.data)
deform_layer = bm.verts.layers.deform.verify()
normalized_vertices = 0
limited_vertices = 0

for vertex in bm.verts:
    deform = vertex[deform_layer]
    weights = [
        (group_index, weight)
        for group_index, weight in deform.items()
        if group_index in deform_group_indices and weight > 1e-8
    ]
    if not weights:
        continue

    total_before = sum(weight for _, weight in weights)
    weights.sort(key=lambda item: item[1], reverse=True)
    kept = weights[:MAX_DEFORM_INFLUENCES]
    if len(weights) > MAX_DEFORM_INFLUENCES:
        limited_vertices += 1
    if abs(total_before - 1.0) > 1e-5 or len(weights) > MAX_DEFORM_INFLUENCES:
        normalized_vertices += 1

    for group_index, _ in weights:
        del deform[group_index]
    kept_total = sum(weight for _, weight in kept)
    if kept_total > 1e-8:
        for group_index, weight in kept:
            deform[group_index] = weight / kept_total

# The imported mesh contains thousands of split vertices along face and UV
# boundaries. Average the weights of coincident points before welding them so
# neighboring faces cannot separate when the wrist bends.
merge_result = bmesh.ops.find_doubles(bm, verts=list(bm.verts), dist=WELD_DISTANCE)
target_map = merge_result.get("targetmap", {})
merge_clusters = {}
for duplicate, target in target_map.items():
    merge_clusters.setdefault(target, [target]).append(duplicate)

for members in merge_clusters.values():
    averaged = {}
    for member in members:
        deform = member[deform_layer]
        for group_index, weight in deform.items():
            if group_index in deform_group_indices and weight > 1e-8:
                averaged[group_index] = averaged.get(group_index, 0.0) + weight / len(members)

    kept = sorted(averaged.items(), key=lambda item: item[1], reverse=True)[:MAX_DEFORM_INFLUENCES]
    kept_total = sum(weight for _, weight in kept)
    for member in members:
        deform = member[deform_layer]
        for group_index in [index for index in deform.keys() if index in deform_group_indices]:
            del deform[group_index]
        if kept_total > 1e-8:
            for group_index, weight in kept:
                deform[group_index] = weight / kept_total

vertex_count_before_weld = len(bm.verts)
bmesh.ops.remove_doubles(bm, verts=list(bm.verts), dist=WELD_DISTANCE)
bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
vertex_count_after_weld = len(bm.verts)

bm.to_mesh(mesh_obj.data)
bm.free()
mesh_obj.data.update()

# Match Blender's preview to the engine's linear blend skinning.
armature_modifier.use_deform_preserve_volume = False

ensure_object_mode()
bpy.ops.object.select_all(action="DESELECT")
armature.select_set(True)
bpy.context.view_layer.objects.active = armature

game_collection = armature.data.collections.get("Game Bones") or armature.data.collections.new("Game Bones")
control_collection = armature.data.collections.get("Controllers") or armature.data.collections.new("Controllers")

for bone in armature.data.bones:
    if not bone.name.startswith(CONTROL_PREFIX):
        game_collection.assign(bone)

bpy.ops.object.mode_set(mode="EDIT")
for edit_bone in list(armature.data.edit_bones):
    if edit_bone.name.startswith(CONTROL_PREFIX):
        armature.data.edit_bones.remove(edit_bone)

hips = armature.data.edit_bones["mixamorig:Hips"]
master = armature.data.edit_bones.new("CTRL_Master")
master.head = Vector((hips.head.x, hips.head.y, 0.0))
master.tail = master.head + Vector((0.0, 0.0, 0.12))
master.use_deform = False

control_specs = {}
for side, suffix in (("Left", "L"), ("Right", "R")):
    upper = armature.data.edit_bones[f"mixamorig:{side}Arm"]
    forearm = armature.data.edit_bones[f"mixamorig:{side}ForeArm"]
    hand = armature.data.edit_bones[f"mixamorig:{side}Hand"]

    hand_control = armature.data.edit_bones.new(f"CTRL_Hand.{suffix}")
    hand_control.head = hand.head.copy()
    hand_control.tail = hand.tail.copy()
    hand_control.roll = hand.roll
    hand_control.parent = master
    hand_control.use_deform = False

    shoulder_position = upper.head.copy()
    elbow_position = forearm.head.copy()
    wrist_position = hand.head.copy()
    shoulder_to_wrist = wrist_position - shoulder_position
    projected = shoulder_position + shoulder_to_wrist * (
        (elbow_position - shoulder_position).dot(shoulder_to_wrist)
        / max(shoulder_to_wrist.length_squared, 1e-8)
    )
    pole_direction = elbow_position - projected
    if pole_direction.length < 0.01:
        pole_direction = Vector((0.0, -1.0, 0.0))
    else:
        pole_direction.normalize()

    pole_control = armature.data.edit_bones.new(f"CTRL_Elbow.{suffix}")
    pole_control.head = elbow_position + pole_direction * 0.24
    pole_control.tail = pole_control.head + Vector((0.0, 0.0, 0.055))
    pole_control.parent = master
    pole_control.use_deform = False

    control_specs[side] = (hand_control.name, pole_control.name)

bpy.ops.object.mode_set(mode="POSE")

for bone in armature.data.bones:
    if bone.name.startswith(CONTROL_PREFIX):
        control_collection.assign(bone)
        set_color(bone, "THEME03")

shape_collection = bpy.data.collections.get("RIG_Shapes")
if not shape_collection:
    shape_collection = bpy.data.collections.new("RIG_Shapes")
    bpy.context.scene.collection.children.link(shape_collection)
shape_collection.hide_render = True

root_shape = circle_shape("WGT_Root", 1.0, 32)
fk_shape = circle_shape("WGT_FK", 1.0, 20)
hand_shape = box_shape("WGT_Hand")
pole_shape = diamond_shape("WGT_Pole")

set_custom_shape(armature.pose.bones["CTRL_Master"], root_shape, 0.13)
armature.pose.bones["CTRL_Master"].lock_scale = (True, True, True)

for side, suffix in (("Left", "L"), ("Right", "R")):
    hand_control_name, pole_control_name = control_specs[side]
    hand_control = armature.pose.bones[hand_control_name]
    pole_control = armature.pose.bones[pole_control_name]

    set_custom_shape(hand_control, hand_shape, 0.035)
    set_custom_shape(pole_control, pole_shape, 0.028)
    hand_control.lock_scale = (True, True, True)
    pole_control.lock_rotation = (True, True, True)
    pole_control.lock_scale = (True, True, True)

    hand_control["IK_FK"] = 0.0
    hand_control.id_properties_ui("IK_FK").update(
        min=0.0,
        max=1.0,
        soft_min=0.0,
        soft_max=1.0,
        description="0 = existing/FK animation, 1 = hand IK controller",
    )

    forearm_pose = armature.pose.bones[f"mixamorig:{side}ForeArm"]
    hand_pose = armature.pose.bones[f"mixamorig:{side}Hand"]
    for pose_bone in (forearm_pose, hand_pose):
        for constraint in list(pose_bone.constraints):
            if constraint.name.startswith("CTRL_"):
                pose_bone.constraints.remove(constraint)

    ik_constraint = forearm_pose.constraints.new("IK")
    ik_constraint.name = "CTRL_Arm_IK"
    ik_constraint.target = armature
    ik_constraint.subtarget = hand_control_name
    ik_constraint.pole_target = armature
    ik_constraint.pole_subtarget = pole_control_name
    ik_constraint.chain_count = 2
    ik_constraint.use_tail = True
    ik_constraint.influence = 0.0

    rotation_constraint = hand_pose.constraints.new("COPY_ROTATION")
    rotation_constraint.name = "CTRL_Hand_Rotation"
    rotation_constraint.target = armature
    rotation_constraint.subtarget = hand_control_name
    rotation_constraint.target_space = "WORLD"
    rotation_constraint.owner_space = "WORLD"
    rotation_constraint.influence = 0.0

    add_influence_driver(ik_constraint, armature, hand_control_name)
    add_influence_driver(rotation_constraint, armature, hand_control_name)

# The original Mixamo deform bones remain direct FK controls, so their keyed
# animation exports without a bake. Shapes only improve selection and clarity.
fk_bones = [
    "mixamorig:Hips",
    "mixamorig:Spine",
    "mixamorig:Spine1",
    "mixamorig:Spine2",
    "mixamorig:LeftShoulder",
    "mixamorig:LeftArm",
    "mixamorig:LeftForeArm",
    "mixamorig:LeftHand",
    "mixamorig:RightShoulder",
    "mixamorig:RightArm",
    "mixamorig:RightForeArm",
    "mixamorig:RightHand",
]
finger_bones = [
    bone.name
    for bone in armature.data.bones
    if "Hand" in bone.name and bone.name not in fk_bones and not bone.name.startswith(CONTROL_PREFIX)
]

for name in fk_bones:
    pose_bone = armature.pose.bones.get(name)
    if not pose_bone:
        continue
    set_custom_shape(pose_bone, fk_shape, 0.032 if "Hand" not in name else 0.026)
    pose_bone.lock_location = (True, True, True)
    pose_bone.lock_scale = (True, True, True)
    set_color(pose_bone.bone, "THEME04")

hips_pose = armature.pose.bones["mixamorig:Hips"]
hips_pose.lock_location = (False, False, False)
set_custom_shape(hips_pose, root_shape, 0.09)

for name in finger_bones:
    pose_bone = armature.pose.bones[name]
    set_custom_shape(pose_bone, fk_shape, 0.012)
    pose_bone.lock_location = (True, True, True)
    pose_bone.lock_scale = (True, True, True)
    set_color(pose_bone.bone, "THEME05")

armature.show_in_front = True
armature.data.display_type = "BBONE"
armature["Rig_Type"] = "Mixamo Game Rig - FK export bones + optional arm IK"
armature["Export_Note"] = "Bake IK to deform bones; export deformation bones only"

bpy.ops.object.mode_set(mode="OBJECT")
ensure_object_mode()

for action in bpy.data.actions:
    action.use_fake_user = True

bpy.ops.wm.save_as_mainfile(filepath=str(OUTPUT), compress=True)

print("OUTPUT", OUTPUT)
print("NORMALIZED_VERTICES", normalized_vertices)
print("LIMITED_VERTICES", limited_vertices)
print("MIRRORED_LEFT_WRIST_VERTICES", mirrored_vertices)
print("CLEANED_WRIST_VERTICES", cleaned_wrist_vertices)
print("WELD_DISTANCE", WELD_DISTANCE)
print("WELDED_VERTICES", vertex_count_before_weld - vertex_count_after_weld)
print("CONTROL_BONES", sorted(name for name in armature.data.bones.keys() if name.startswith(CONTROL_PREFIX)))
print("TOTAL_BONES", len(armature.data.bones))
print("ACTIONS", sorted(action.name for action in bpy.data.actions))
