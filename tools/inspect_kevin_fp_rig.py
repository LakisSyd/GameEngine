"""Print the details needed to transfer Kevin's arm controls to the FP rig."""

import json
import bpy


def vector3(value):
    return [round(float(v), 5) for v in value[:3]]


for obj in bpy.data.objects:
    if obj.type != "ARMATURE":
        continue
    bone_names = [bone.name for bone in obj.data.bones]
    controls = [name for name in bone_names if name.startswith(("CTRL_", "FK_", "IK_"))]
    sample = {}
    for name in (
        "mixamorig:Hips", "mixamorig:Spine", "mixamorig:LeftShoulder",
        "mixamorig:LeftArm", "mixamorig:LeftForeArm", "mixamorig:LeftHand",
        "mixamorig:RightShoulder", "mixamorig:RightArm",
        "mixamorig:RightForeArm", "mixamorig:RightHand",
        "CTRL_Hand_IK.L", "CTRL_Hand_IK.R", "CTRL_Elbow_Pole.L",
        "CTRL_Elbow_Pole.R", "CTRL_UpperArm_FK.L",
    ):
        bone = obj.data.bones.get(name)
        if bone:
            sample[name] = {
                "head": vector3(bone.head_local),
                "tail": vector3(bone.tail_local),
                "parent": bone.parent.name if bone.parent else None,
                "deform": bone.use_deform,
            }
    constraints = {}
    for name in (
        "mixamorig:LeftArm", "mixamorig:LeftForeArm", "mixamorig:LeftHand",
        "mixamorig:RightArm", "mixamorig:RightForeArm", "mixamorig:RightHand",
    ):
        pb = obj.pose.bones.get(name)
        if pb:
            constraints[name] = [
                {"name": c.name, "type": c.type,
                 "target": c.target.name if hasattr(c, "target") and c.target else None,
                 "subtarget": getattr(c, "subtarget", ""),
                 "influence": round(c.influence, 3)}
                for c in pb.constraints
            ]
    drivers = []
    if obj.animation_data:
        for curve in obj.animation_data.drivers:
            if "mixamorig:" in curve.data_path and "influence" in curve.data_path:
                drivers.append({"path": curve.data_path,
                                "expression": curve.driver.expression})
    result = {
        "file": bpy.data.filepath,
        "blender": bpy.app.version_string,
        "name": obj.name,
        "location": vector3(obj.location),
        "scale": vector3(obj.scale),
        "bones": len(bone_names),
        "controls": controls,
        "collections": [c.name for c in obj.data.collections],
        "sample": sample,
        "constraints": constraints,
        "drivers": drivers[:15],
        "active_action": obj.animation_data.action.name
            if obj.animation_data and obj.animation_data.action else None,
        "scene_frame": bpy.context.scene.frame_current,
    }
    print("RIG_PROBE " + json.dumps(result, ensure_ascii=False))

print("SCENE_OBJECTS " + json.dumps(
    [(obj.name, obj.type) for obj in bpy.context.scene.objects],
    ensure_ascii=False))
print("ACTIONS " + json.dumps([action.name for action in bpy.data.actions],
    ensure_ascii=False))
