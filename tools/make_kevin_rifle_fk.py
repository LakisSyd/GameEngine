"""Build a rifle posing variant: left arm FK, right arm IK, no switch drivers."""

import math
from pathlib import Path

import bpy
from mathutils import Quaternion


OUTPUT = Path(r"C:\Users\karin\OneDrive\Desktop\Kevin_Main_Rifle_FK.blend")
armature = bpy.data.objects["Armature"]
deform_names = (
    "mixamorig:LeftArm", "mixamorig:LeftForeArm", "mixamorig:LeftHand",
    "mixamorig:RightArm", "mixamorig:RightForeArm", "mixamorig:RightHand",
)
before = {name: armature.pose.bones[name].matrix.copy() for name in deform_names}

for side, mixamo_side, ik_weight in (("L", "Left", 0.0), ("R", "Right", 1.0)):
    for part in ("Arm", "ForeArm", "Hand"):
        deform = armature.pose.bones[f"mixamorig:{mixamo_side}{part}"]
        for constraint in deform.constraints:
            if constraint.name not in {"KevinRig • Arm FK", "KevinRig • Arm IK"}:
                continue
            try:
                constraint.driver_remove("influence")
            except (TypeError, RuntimeError):
                pass
            constraint.influence = (
                ik_weight if constraint.name.endswith("Arm IK") else 1.0 - ik_weight
            )

for side in ("L", "R"):
    armature.pop(f"KevinRig_Arm_IK_{side}", None)

# In this variant, the left hand IK target and pole have no effect. Hide them
# to make the controls that do work clear in the viewport.
for name in ("CTRL_Hand_IK.L", "CTRL_Elbow_Pole.L"):
    armature.data.bones[name].hide = True

armature.data.collections["Kevin • FK"].is_visible = True
armature["Kevin_Rig_Note"] = (
    "Rifle posing: left arm uses FK (rotate CTRL_UpperArm_FK.L, "
    "CTRL_Forearm_FK.L, CTRL_Hand_FK.L); right arm uses IK "
    "(move CTRL_Hand_IK.R). No IK/FK slider is needed."
)
bpy.context.view_layer.update()

pose_delta = max(
    abs(old - new)
    for name in deform_names
    for old_row, new_row in zip(before[name], armature.pose.bones[name].matrix)
    for old, new in zip(old_row, new_row)
)
if pose_delta > 0.01:
    raise RuntimeError(f"Mode conversion changed the saved pose: {pose_delta}")

hand = armature.pose.bones["mixamorig:LeftHand"]
start_position = hand.matrix.translation.copy()
forearm = armature.pose.bones["CTRL_Forearm_FK.L"]
original_rotation_mode = forearm.rotation_mode
original_rotation = forearm.rotation_quaternion.copy()
forearm.rotation_mode = "QUATERNION"
forearm.rotation_quaternion = original_rotation @ Quaternion((1, 0, 0), math.radians(20))
bpy.context.view_layer.update()
movement = (hand.matrix.translation - start_position).length
forearm.rotation_quaternion = original_rotation
forearm.rotation_mode = original_rotation_mode
bpy.context.view_layer.update()
if movement < 1.0:
    raise RuntimeError(f"FK rotation did not move the hand: {movement}")

bpy.context.preferences.filepaths.save_version = 0
bpy.ops.wm.save_as_mainfile(filepath=str(OUTPUT), compress=True, check_existing=False)
print("SAVED", OUTPUT)
print("POSE_DELTA", pose_delta)
print("LEFT_HAND_TEST_MOVEMENT_ARMATURE_UNITS", movement)
print("LEFT", [(c.name, c.influence) for c in armature.pose.bones["mixamorig:LeftForeArm"].constraints])
print("RIGHT", [(c.name, c.influence) for c in armature.pose.bones["mixamorig:RightForeArm"].constraints])
