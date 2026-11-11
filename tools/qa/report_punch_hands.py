import bpy


rig = bpy.data.objects.get("Armature") or bpy.data.objects.get("Kevin_Game")
action = bpy.data.actions["Kevin_Punch.1"]
animation = rig.animation_data_create()
animation.action = action
if action.slots:
    animation.action_slot = action.slots[0]

start, end = (round(value) for value in action.frame_range)
for frame in (start, (start + end) // 4, (start + end) // 2,
              3 * (start + end) // 4, end):
    bpy.context.scene.frame_set(frame)
    bpy.context.view_layer.update()
    values = {}
    for name in ("mixamorig:LeftHand", "mixamorig:RightHand"):
        point = (rig.matrix_world @ rig.pose.bones[name].matrix).translation
        values[name] = tuple(round(value, 4) for value in point)
    print("PUNCH_HANDS", frame, values)
