"""Сравнивает Action и контроллеры мастер-рига с FP-файлом."""

import hashlib
import json

import bpy
from mathutils import Matrix


rig = bpy.data.objects["Armature"]
mesh = bpy.data.objects["MaleBaseMesh LowPoly"]
mode = bpy.data.objects["Kevin_Arm_Mode"]
animation = rig.animation_data_create()


def sample(action_name, frame):
    animation.action = None
    for pose_bone in rig.pose.bones:
        pose_bone.matrix_basis = Matrix.Identity(4)
    action = bpy.data.actions[action_name]
    animation.action = action
    slots = list(action.slots)
    if slots:
        animation.action_slot = next(
            (slot for slot in slots if slot.target_id_type == "OBJECT"),
            slots[0])
    bpy.context.scene.frame_set(frame)
    bpy.context.view_layer.update()
    evaluated = rig.evaluated_get(bpy.context.evaluated_depsgraph_get())
    values = "|".join(
        f"{value:.6f}"
        for name in sorted(bone.name for bone in rig.data.bones
                           if bone.name.startswith("mixamorig:"))
        for row in evaluated.pose.bones[name].matrix
        for value in row
    )
    return hashlib.sha256(values.encode()).hexdigest()


checks = {
    f"{name}@{frame}": sample(name, frame)
    for name, frames in (
        ("Kevin_Idle", (1, 20, 45)),
        ("Kevin_Walk_R_I", (0, 20, 45)),
        ("Kevin_Punch.1", (0, 4, 20)),
    )
    for frame in frames
}
print("FP_AUTHORING_VERIFY " + json.dumps({
    "file": bpy.data.filepath,
    "bones": len(rig.data.bones),
    "controls": sum(bone.name.startswith("CTRL_")
                    for bone in rig.data.bones),
    "actions": sorted(action.name for action in bpy.data.actions),
    "mode": [round(value, 3) for value in mode.location],
    "faces": len(mesh.data.polygons),
    "hashes": checks,
}, ensure_ascii=False))
