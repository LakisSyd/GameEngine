"""Фиксирует текущий сгиб обоих кулаков в Action Kevin_Punch.1."""

import bpy


rig = bpy.data.objects["Armature"]
action = bpy.data.actions["Kevin_Punch.1"]
animation = rig.animation_data_create()
if animation.action != action:
    raise RuntimeError("Откройте Kevin_Punch.1 и сохраните позу перед запуском")

scene = bpy.context.scene
saved_frame = scene.frame_current
fists = {}
for name in ("CTRL_Fist.L", "CTRL_Fist.R"):
    bone = rig.pose.bones[name]
    if bone.rotation_mode != "QUATERNION":
        raise RuntimeError(f"Неожиданный режим вращения {name}")
    fists[name] = bone.rotation_quaternion.copy()

start, end = (round(value) for value in action.frame_range)
for frame in (start, end):
    scene.frame_set(frame)
    for name, rotation in fists.items():
        bone = rig.pose.bones[name]
        # Ключи на обоих концах удерживают сгиб во всём ударе,
        # даже после переключения с другого Action.
        bone.rotation_quaternion = rotation
        bone.keyframe_insert(data_path="rotation_quaternion", frame=frame,
                             group=name)

scene.frame_set(saved_frame)
bpy.context.preferences.filepaths.save_version = 0
bpy.ops.wm.save_as_mainfile(filepath=bpy.data.filepath, compress=True,
                            check_existing=False)
print("PUNCH_FISTS_KEYED", start, end,
      {name: tuple(round(value, 5) for value in rotation)
       for name, rotation in fists.items()})
