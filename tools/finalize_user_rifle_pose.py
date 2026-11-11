"""Capture the user's current Kevin pose as a one-frame Rifle_Idle Action."""

from pathlib import Path

import bpy


OUTPUT = Path(r"C:\Users\karin\OneDrive\Desktop\Kevin_Rifle_Anim.blend")
rig = bpy.data.objects["Armature"]
scene = bpy.context.scene

old = bpy.data.actions.get("Rifle_Idle")
if old is not None:
    bpy.data.actions.remove(old)

action = bpy.data.actions.new("Rifle_Idle")
action.use_fake_user = True
rig.animation_data_create()
rig.animation_data.action = action
scene.frame_start = 1
scene.frame_end = 1
scene.frame_set(1)

keyed = []
for pb in rig.pose.bones:
    if not pb.name.startswith("CTRL_"):
        continue
    pb.keyframe_insert("location", frame=1, group=pb.name)
    if pb.rotation_mode == "QUATERNION":
        pb.keyframe_insert("rotation_quaternion", frame=1, group=pb.name)
    elif pb.rotation_mode == "AXIS_ANGLE":
        pb.keyframe_insert("rotation_axis_angle", frame=1, group=pb.name)
    else:
        pb.keyframe_insert("rotation_euler", frame=1, group=pb.name)
    pb.keyframe_insert("scale", frame=1, group=pb.name)
    keyed.append(pb.name)

bpy.context.preferences.filepaths.save_version = 0
bpy.ops.wm.save_as_mainfile(filepath=str(OUTPUT), compress=True, check_existing=False)
print("SAVED", OUTPUT)
print("ACTION", action.name, tuple(action.frame_range))
print("KEYED_CONTROLS", len(keyed))
