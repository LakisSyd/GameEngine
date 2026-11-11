"""Build a compact rifle animation rig from Kevin_Main.blend.

The script runs with the source file open and writes a separate result.
"""

from pathlib import Path

import bpy
from mathutils import Matrix, Vector


OUTPUT = Path(r"C:\Users\karin\OneDrive\Desktop\Kevin_Rifle_Anim.blend")
scene = bpy.context.scene
rig = bpy.data.objects["Armature"]
mesh = bpy.data.objects["MaleBaseMesh LowPoly"]
gun = bpy.data.objects["REF_TestRifle"]
gun_original_rotation = gun.matrix_world.to_quaternion().copy()


def pose_matrix(name):
    return rig.pose.bones[name].matrix.copy()


def matrix_delta(a, b):
    return max(abs(x - y) for ar, br in zip(a, b) for x, y in zip(ar, br))


original = {
    name: pose_matrix(name)
    for name in (
        "mixamorig:LeftShoulder", "mixamorig:LeftArm", "mixamorig:LeftForeArm",
        "mixamorig:LeftHand", "mixamorig:RightShoulder", "mixamorig:RightArm",
        "mixamorig:RightForeArm", "mixamorig:RightHand",
    )
}

# The original reference rig supplied the transferred animation. Its retarget
# constraints are all disabled; remove those links before cleaning the scene.
for bone in rig.pose.bones:
    for constraint in list(bone.constraints):
        if constraint.name.startswith("Retarget") and constraint.influence == 0.0:
            bone.constraints.remove(constraint)

for side in ("L", "R"):
    for part in ("Arm", "ForeArm", "Hand"):
        deform = rig.pose.bones[
            f"mixamorig:{'Left' if side == 'L' else 'Right'}{part}"
        ]
        for constraint in list(deform.constraints):
            if constraint.name == "KevinRig • Arm FK":
                deform.constraints.remove(constraint)

bpy.ops.object.mode_set(mode="OBJECT") if bpy.context.object and bpy.context.object.mode != "OBJECT" else None
bpy.ops.object.select_all(action="DESELECT")
rig.select_set(True)
bpy.context.view_layer.objects.active = rig
bpy.ops.object.mode_set(mode="EDIT")
groups = rig.data.collections.get("Kevin • Arm Groups")
if groups is None:
    groups = rig.data.collections.new("Kevin • Arm Groups")
for side in ("L", "R"):
    shoulder = rig.data.edit_bones[f"CTRL_Shoulder.{side}"]
    group = rig.data.edit_bones.new(f"CTRL_Arm_Group.{side}")
    group.head = shoulder.head.copy()
    group.tail = shoulder.head + Vector((0.0, 100.0, 0.0))
    group.parent = rig.data.edit_bones["CTRL_Chest"]
    group.use_deform = False
    groups.assign(group)
bpy.ops.object.mode_set(mode="POSE")

for side in ("L", "R"):
    group = rig.pose.bones[f"CTRL_Arm_Group.{side}"]
    group.custom_shape = bpy.data.objects["WGT_Kevin_Cube"]
    group.custom_shape_scale_xyz = (0.45, 0.45, 0.45)
    for name in (
        f"CTRL_Shoulder.{side}",
        f"CTRL_Hand_IK.{side}",
        f"CTRL_Elbow_Pole.{side}",
    ):
        control = rig.pose.bones[name]
        constraint = control.constraints.new("CHILD_OF")
        constraint.name = "KevinRig • Arm Group Follow"
        constraint.target = rig
        constraint.subtarget = group.name
        with bpy.context.temp_override(
            active_object=rig,
            object=rig,
            active_pose_bone=control,
            selected_pose_bones=[control],
        ):
            bpy.ops.constraint.childof_set_inverse(constraint=constraint.name, owner="BONE")
        bpy.context.view_layer.update()

changed = max(matrix_delta(original[name], pose_matrix(name)) for name in original)
print("GROUP_POSE_DELTA", changed)
if changed > 0.02:
    raise RuntimeError("Adding shoulder groups changed the saved pose")

# Test that lifting a group carries the hand IK target along with the shoulder.
for side in ("L", "R"):
    group = rig.pose.bones[f"CTRL_Arm_Group.{side}"]
    shoulder = rig.pose.bones[f"mixamorig:{'Left' if side == 'L' else 'Right'}Shoulder"]
    hand = rig.pose.bones[f"mixamorig:{'Left' if side == 'L' else 'Right'}Hand"]
    old_shoulder = shoulder.head.copy()
    old_hand = hand.head.copy()
    group.location.y += 50.0
    bpy.context.view_layer.update()
    print("GROUP_LIFT", side, (shoulder.head - old_shoulder).length,
          (hand.head - old_hand).length)
    group.location.y -= 50.0
    bpy.context.view_layer.update()

print("GROUPS_CREATED", [b.name for b in rig.pose.bones if b.name.startswith("CTRL_Arm_Group")])


def world_head(name):
    return rig.matrix_world @ rig.pose.bones[name].head


def move_control_world(name, target, observed_name=None):
    control = rig.pose.bones[name]
    observed_name = observed_name or name
    target = Vector(target)
    for _ in range(10):
        current = world_head(observed_name)
        error = target - current
        if error.length < 0.005:
            break
        base = control.location.copy()
        columns = []
        for axis in range(3):
            control.location[axis] = base[axis] + 10.0
            bpy.context.view_layer.update()
            columns.append((world_head(observed_name) - current) / 10.0)
            control.location = base
            bpy.context.view_layer.update()
        jacobian = Matrix(tuple(
            tuple(columns[column][row] for column in range(3))
            for row in range(3)
        ))
        adjustment = jacobian.inverted_safe() @ error
        if adjustment.length > 250.0:
            adjustment.normalize()
            adjustment *= 250.0
        control.location = base + adjustment
        bpy.context.view_layer.update()
    print("MOVED", name, "observed", observed_name,
          tuple(round(v, 3) for v in world_head(observed_name)),
          "error", round((target - world_head(observed_name)).length, 4))


# Straighten the torso and neck, keeping the rifle and both hand goals with
# the chest. Rotate the clavicles gently instead of folding them into the ribs.
pose_rotations = {
    "CTRL_Spine_01": (0.05, 0.0, 0.0),
    "CTRL_Chest": (0.11, 0.03, 0.0),
    "CTRL_Neck": (-0.04, 0.0, 0.0),
    "CTRL_Head": (0.0, 0.0, 0.0),
    "CTRL_Shoulder.L": (-0.04, -0.05, 0.03),
    "CTRL_Shoulder.R": (-0.15, -0.02, 0.06),
}
for name, angles in pose_rotations.items():
    rig.pose.bones[name].rotation_euler = angles
    bpy.context.view_layer.update()

# Bring the support palm under the fore-end. Read the gun position *after* the
# chest and right shoulder are adjusted, since the gun follows the right hand.
print("GUN_BOUNDS", [
    tuple(round((gun.matrix_world @ Vector(c))[i], 3) for i in range(3))
    for c in gun.bound_box
])
right_hand = world_head("mixamorig:RightHand")
left_hand = world_head("mixamorig:LeftHand")
print("HANDS_BEFORE", tuple(round(v, 3) for v in right_hand),
      tuple(round(v, 3) for v in left_hand))

# Keep the gun level after straightening the torso. This mesh is a posing
# reference and retains its attachment to the right hand.
gun_world = gun.matrix_world.copy()
gun.matrix_world = Matrix.LocRotScale(
    gun_world.translation, gun_original_rotation, gun_world.to_scale()
)
bpy.context.view_layer.update()
move_control_world(
    "CTRL_Hand_IK.L",
    right_hand + Vector((0.35, -0.70, 0.0)),
    "mixamorig:LeftHand",
)

# The pole controls only choose the elbow bend direction. Keep both elbows
# below and outside the gun so they do not flip behind the shoulders.
left_shoulder = world_head("mixamorig:LeftArm")
right_shoulder = world_head("mixamorig:RightArm")
move_control_world("CTRL_Elbow_Pole.L", left_shoulder + Vector((0.65, -0.65, -0.65)))
move_control_world("CTRL_Elbow_Pole.R", right_shoulder + Vector((-0.65, -0.65, -0.65)))

# A relaxed grip reads better than the imported flat palms. Curl each joint
# separately so the fingers remain editable with the existing finger controls.
for side in ("L", "R"):
    for finger in ("Index", "Middle", "Ring", "Pinky"):
        angles = (0.75, 0.70, 0.40) if side == "L" else (0.60, 0.65, 0.38)
        for segment, angle in enumerate(angles, start=1):
            bone = rig.pose.bones[f"CTRL_{finger}_{segment:02d}.{side}"]
            bone.rotation_euler.z = angle
    rig.pose.bones[f"CTRL_Thumb_01.{side}"].rotation_euler.z = 0.38
    rig.pose.bones[f"CTRL_Thumb_02.{side}"].rotation_euler.z = 0.42
    rig.pose.bones[f"CTRL_Thumb_03.{side}"].rotation_euler.z = 0.25
bpy.context.view_layer.update()

# Keep the hand goals visible but remove the direct clavicle controls from the
# everyday selection set. CTRL_Arm_Group handles shoulder lift without IK crush.
main_collection = rig.data.collections["Kevin • Main"]
mechanism_collection = rig.data.collections["Kevin • Mechanism"]
for side in ("L", "R"):
    shoulder_bone = rig.data.bones[f"CTRL_Shoulder.{side}"]
    main_collection.unassign(shoulder_bone)
    mechanism_collection.assign(shoulder_bone)
rig.data.collections["Kevin • FK"].is_visible = False

print("POSED_HANDS", [(side, tuple(round(v, 3) for v in world_head(f"mixamorig:{side}Hand")))
                       for side in ("Left", "Right")])

# Remove the duplicate transfer mannequin and unrelated default scene lights.
for name in ("MaleBaseMesh LowPoly.001", "Armature.001", "Camera", "Camera.002", "Light"):
    obj = bpy.data.objects.get(name)
    if obj:
        bpy.data.objects.remove(obj, do_unlink=True)

# The new Action is a complete still rifle hold, ready for the user to add a
# breathing cycle. Key every visible pose control at frame 1 to prevent the
# previous running Action from leaking into this clip.
scene.frame_set(1)
action = bpy.data.actions.new("Rifle_Idle")
action.use_fake_user = True
rig.animation_data_create()
rig.animation_data.action = action
key_names = [pb.name for pb in rig.pose.bones if pb.name.startswith("CTRL_")]
for name in key_names:
    pb = rig.pose.bones[name]
    pb.keyframe_insert(data_path="location", frame=1, group=name)
    pb.keyframe_insert(data_path="rotation_euler", frame=1, group=name)
    pb.keyframe_insert(data_path="scale", frame=1, group=name)
    pb.keyframe_insert(data_path="location", frame=40, group=name)
    pb.keyframe_insert(data_path="rotation_euler", frame=40, group=name)
    pb.keyframe_insert(data_path="scale", frame=40, group=name)
scene.frame_start = 1
scene.frame_end = 40
scene.frame_set(1)

rig["Kevin_Rig_Note"] = (
    "Rifle rig: move CTRL_Arm_Group.L/R to lift a complete shoulder and arm; "
    "move CTRL_Hand_IK.L/R to place each hand; use CTRL_Elbow_Pole.L/R only "
    "to choose bend direction. Rifle_Idle is keyed on frames 1 and 40."
)
bpy.ops.object.mode_set(mode="OBJECT")
bpy.ops.object.select_all(action="DESELECT")
rig.select_set(True)
bpy.context.view_layer.objects.active = rig
bpy.context.preferences.filepaths.save_version = 0
bpy.ops.wm.save_as_mainfile(filepath=str(OUTPUT), compress=True, check_existing=False)
print("SAVED", OUTPUT)
