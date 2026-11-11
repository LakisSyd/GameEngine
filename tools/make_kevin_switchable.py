"""Create a switchable IK/FK Kevin rig from the current posed control rig.

Run with Kevin_Main_Rig_v2.blend open in Blender background mode. The mode
Empty's X and Y locations drive left/right arm IK, so editing Transform values
uses Blender's normal object transform dependency updates.
"""

from pathlib import Path

import bpy


OUTPUT = Path(r"C:\Users\karin\OneDrive\Desktop\Kevin_Main_Switchable.blend")
armature = bpy.data.objects["Armature"]
scene = bpy.context.scene

mode = bpy.data.objects.get("Kevin_Arm_Mode")
if mode is None:
    mode = bpy.data.objects.new("Kevin_Arm_Mode", None)
    scene.collection.objects.link(mode)
mode.empty_display_type = "PLAIN_AXES"
mode.empty_display_size = 0.05
mode.hide_render = True
mode.location = (1.0, 1.0, 0.0)
mode["Instructions"] = "Transform X: left arm, Y: right arm. 1=IK, 0=FK."

armature.data.collections["Kevin • FK"].is_visible = True
for name in ("CTRL_Hand_IK.L", "CTRL_Elbow_Pole.L"):
    armature.data.bones[name].hide = False

deform_names = (
    "mixamorig:LeftArm", "mixamorig:LeftForeArm", "mixamorig:LeftHand",
    "mixamorig:RightArm", "mixamorig:RightForeArm", "mixamorig:RightHand",
)
before = {name: armature.pose.bones[name].matrix.copy() for name in deform_names}

# Match the FK hierarchy to the evaluated IK pose at the saved frame. This is
# essential when the source file was re-saved after the first rig conversion.
for suffix, word in (("L", "Left"), ("R", "Right")):
    pairs = (
        (f"CTRL_UpperArm_FK.{suffix}", f"mixamorig:{word}Arm"),
        (f"CTRL_Forearm_FK.{suffix}", f"mixamorig:{word}ForeArm"),
        (f"CTRL_Hand_FK.{suffix}", f"mixamorig:{word}Hand"),
    )
    targets = {control: before[deform].copy() for control, deform in pairs}
    for control, _ in pairs:
        armature.pose.bones[control].matrix = targets[control]
        bpy.context.view_layer.update()

for suffix, word, axis in (("L", "Left", "LOC_X"), ("R", "Right", "LOC_Y")):
    for part in ("Arm", "ForeArm", "Hand"):
        bone = armature.pose.bones[f"mixamorig:{word}{part}"]
        for constraint in bone.constraints:
            if constraint.name not in {"KevinRig • Arm FK", "KevinRig • Arm IK"}:
                continue
            try:
                constraint.driver_remove("influence")
            except (TypeError, RuntimeError):
                pass
            driver = constraint.driver_add("influence").driver
            driver.type = "SCRIPTED"
            variable = driver.variables.new()
            variable.name = "ik"
            variable.type = "TRANSFORMS"
            variable.targets[0].id = mode
            variable.targets[0].transform_type = axis
            variable.targets[0].transform_space = "LOCAL_SPACE"
            driver.expression = (
                "max(0,min(1,ik))" if constraint.name.endswith("Arm IK")
                else "1-max(0,min(1,ik))"
            )

# The old self driven properties are misleading. The new mode object owns the
# switches, and all left/right constraints reference that object explicitly.
for suffix in ("L", "R"):
    armature.pop(f"KevinRig_Arm_IK_{suffix}", None)
armature["Kevin_Rig_Note"] = (
    "Select Kevin_Arm_Mode in Object Mode. Transform X=left arm, Y=right arm: "
    "1=IK hand target, 0=FK arm circles. FK and IK are aligned at saved frame 2. "
    "After posing one mode, match the other controls before switching to "
    "avoid a pop."
)

bpy.context.view_layer.update()

def influence(side, part="ForeArm"):
    pb = armature.pose.bones[f"mixamorig:{side}{part}"]
    return tuple(c.influence for c in pb.constraints if c.name in {
        "KevinRig • Arm FK", "KevinRig • Arm IK"
    })


start_delta = max(
    abs(a - b)
    for name in deform_names
    for ar, br in zip(before[name], armature.pose.bones[name].matrix)
    for a, b in zip(ar, br)
)
if start_delta > 0.01:
    raise RuntimeError(f"Default IK pose changed: {start_delta}")

# Test exactly the change that previously failed: edit the switch like a normal
# custom property, then refresh the scene and inspect evaluated constraints.
mode.location.x = 0.0
scene.frame_set(scene.frame_current + 1)
bpy.context.view_layer.update()
left_fk = influence("Left")
if left_fk != (1.0, 0.0):
    raise RuntimeError(f"Left FK switch does not work: {left_fk}")

mode.location.x = 1.0
scene.frame_set(scene.frame_current - 1)
bpy.context.view_layer.update()
left_ik = influence("Left")
if left_ik != (0.0, 1.0):
    raise RuntimeError(f"Left IK switch does not work: {left_ik}")

mode.location.y = 0.0
scene.frame_set(scene.frame_current + 1)
bpy.context.view_layer.update()
right_fk = influence("Right")
if right_fk != (1.0, 0.0):
    raise RuntimeError(f"Right FK switch does not work: {right_fk}")

mode.location.y = 1.0
scene.frame_set(scene.frame_current - 1)
bpy.context.view_layer.update()
right_ik = influence("Right")
if right_ik != (0.0, 1.0):
    raise RuntimeError(f"Right IK switch does not work: {right_ik}")

for axis in ("x", "y"):
    setattr(mode.location, axis, 0.0)
bpy.context.view_layer.update()
fk_pose_delta = max(
    abs(a - b)
    for name in deform_names
    for ar, br in zip(before[name], armature.pose.bones[name].matrix)
    for a, b in zip(ar, br)
)
if fk_pose_delta > 0.01:
    raise RuntimeError(f"Switching at saved frame changes the pose: {fk_pose_delta}")
mode.location = (1.0, 1.0, 0.0)
bpy.context.view_layer.update()

bpy.context.preferences.filepaths.save_version = 0
bpy.ops.wm.save_as_mainfile(filepath=str(OUTPUT), compress=True, check_existing=False)
print("SAVED", OUTPUT)
print("DEFAULT_POSE_DELTA", start_delta)
print("FK_POSE_DELTA", fk_pose_delta)
print("TEST_LEFT_FK_IK", left_fk, left_ik)
print("TEST_RIGHT_FK_IK", right_fk, right_ik)
