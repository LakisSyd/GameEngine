import bpy
from mathutils import Quaternion, Vector


armature = bpy.data.objects["Main"]
if armature.animation_data:
    armature.animation_data.action = None
bpy.context.scene.frame_set(1)

for pose_bone in armature.pose.bones:
    pose_bone.location = (0.0, 0.0, 0.0)
    pose_bone.rotation_mode = "QUATERNION"
    pose_bone.rotation_quaternion = Quaternion()
    pose_bone.scale = (1.0, 1.0, 1.0)
bpy.context.view_layer.update()

for side, suffix in (("Left", "L"), ("Right", "R")):
    upper = armature.pose.bones[f"mixamorig:{side}Arm"]
    forearm = armature.pose.bones[f"mixamorig:{side}ForeArm"]
    hand = armature.pose.bones[f"mixamorig:{side}Hand"]
    control = armature.pose.bones[f"CTRL_Hand.{suffix}"]

    reference = {
        "upper": upper.matrix.copy(),
        "forearm": forearm.matrix.copy(),
        "hand": hand.matrix.copy(),
        "wrist": forearm.tail.copy(),
    }
    control["IK_FK"] = 1.0
    bpy.context.view_layer.update()

    print(side.upper(), "IK_ENABLE")
    print("wrist_snap", round((forearm.tail - reference["wrist"]).length, 8))
    print(
        "rotation_snap",
        round(reference["upper"].to_quaternion().rotation_difference(upper.matrix.to_quaternion()).angle, 8),
        round(reference["forearm"].to_quaternion().rotation_difference(forearm.matrix.to_quaternion()).angle, 8),
        round(reference["hand"].to_quaternion().rotation_difference(hand.matrix.to_quaternion()).angle, 8),
    )

    control.location += Vector((0.0, -0.025, 0.025))
    bpy.context.view_layer.update()
    target_position = control.head.copy()
    print("ik_follow_error", round((forearm.tail - target_position).length, 8))

    control["IK_FK"] = 0.0
    control.location = (0.0, 0.0, 0.0)
    bpy.context.view_layer.update()

print("CONTROL_COLLECTION", sorted(bone.name for bone in armature.data.collections["Controllers"].bones))
print("ACTIONS", sorted(action.name for action in bpy.data.actions))
