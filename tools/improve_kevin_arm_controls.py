"""Expose per-arm IK/FK controls without changing Kevin's current pose.

Run with Blender in background and Kevin_Main.blend loaded. Writes a new .blend.
"""

import bpy
from pathlib import Path


OUTPUT = Path(r"C:\Users\karin\OneDrive\Desktop\Kevin_Main_Rig_v2.blend")
armature = bpy.data.objects["Armature"]

for side, mixamo_side in (("L", "Left"), ("R", "Right")):
    property_name = f"KevinRig_Arm_IK_{side}"
    armature[property_name] = 1.0
    armature.id_properties_ui(property_name).update(
        min=0.0,
        max=1.0,
        soft_min=0.0,
        soft_max=1.0,
        description="1 = move hand with IK; 0 = rotate FK shoulder, upper arm, forearm and hand",
    )

    for part in ("Arm", "ForeArm", "Hand"):
        deform = armature.pose.bones[f"mixamorig:{mixamo_side}{part}"]
        for constraint in deform.constraints:
            if constraint.name not in {"KevinRig • Arm FK", "KevinRig • Arm IK"}:
                continue
            try:
                constraint.driver_remove("influence")
            except (TypeError, RuntimeError):
                pass
            curve = constraint.driver_add("influence")
            driver = curve.driver
            driver.type = "SCRIPTED"
            variable = driver.variables.new()
            variable.name = "ik"
            variable.type = "SINGLE_PROP"
            variable.targets[0].id = armature
            variable.targets[0].data_path = f'["{property_name}"]'
            driver.expression = "ik" if constraint.name.endswith("Arm IK") else "1-ik"

fk_collection = armature.data.collections.get("Kevin • FK")
if fk_collection is None:
    raise RuntimeError("FK bone collection is missing")
fk_collection.is_visible = True

# Match FK controls to the currently saved IK pose. The user has already posed
# the rifle at this frame, so the first switch to FK should not throw the arms
# back to their original rest positions.
for side, mixamo_side in (("L", "Left"), ("R", "Right")):
    pairs = (
        (f"CTRL_UpperArm_FK.{side}", f"mixamorig:{mixamo_side}Arm"),
        (f"CTRL_Forearm_FK.{side}", f"mixamorig:{mixamo_side}ForeArm"),
        (f"CTRL_Hand_FK.{side}", f"mixamorig:{mixamo_side}Hand"),
    )
    desired_matrices = {
        control_name: armature.pose.bones[deform_name].matrix.copy()
        for control_name, deform_name in pairs
    }
    for control_name, _ in pairs:
        armature.pose.bones[control_name].matrix = desired_matrices[control_name]
        bpy.context.view_layer.update()

armature["Kevin_Rig_Note"] = (
    "Hands default to IK. On Armature object set KevinRig_Arm_IK_L/R=0 for FK. "
    "In FK, rotate CTRL_Shoulder, CTRL_UpperArm_FK, CTRL_Forearm_FK "
    "and CTRL_Hand_FK on the matching side. Switch at the start of a new Action "
    "unless poses have been matched. FK controls are matched to the saved frame."
)

bpy.context.scene.frame_set(bpy.context.scene.frame_current)
bpy.context.view_layer.update()
bpy.context.preferences.filepaths.save_version = 0
bpy.ops.wm.save_as_mainfile(filepath=str(OUTPUT), compress=True, check_existing=False)
print("SAVED", OUTPUT)
print("FK_COLLECTION_VISIBLE", fk_collection.is_visible)
for side, mixamo_side in (("L", "Left"), ("R", "Right")):
    print("HAND_CONTROL", side, armature[f"KevinRig_Arm_IK_{side}"])
    for part in ("Arm", "ForeArm", "Hand"):
        deform = armature.pose.bones[f"mixamorig:{mixamo_side}{part}"]
        print("DEFORM", deform.name, [(c.name, c.influence) for c in deform.constraints])
