from pathlib import Path

import bpy


root = Path(__file__).resolve().parents[2]
bpy.ops.import_scene.gltf(
    filepath=str(root / "assets/model/Kevin_Combat_FP.glb"))
game = next(obj for obj in bpy.data.objects if obj.type == "ARMATURE")
mesh = next(obj for obj in bpy.data.objects
            if obj.type == "MESH" and len(obj.data.vertices) > 1000)
action = bpy.data.actions["Kevin_Punch.1"]
assert len(game.data.bones) == 65
assert mesh.parent == game
assert len(bpy.data.actions) == 8
game.animation_data.action = action
game.animation_data.action_slot = action.slots[0]

start, end = (round(value) for value in action.frame_range)
for frame in (start, (start + end) // 2, end):
    bpy.context.scene.frame_set(frame)
    bpy.context.view_layer.update()
    for name in ("mixamorig:RightHandIndex1",
                 "mixamorig:RightHandMiddle1",
                 "mixamorig:LeftHandIndex1"):
        bone = game.pose.bones[name]
        angle = bone.rotation_quaternion.angle
        assert angle > 1.0, (frame, name, angle)
        print("FINGER_BAKED", frame, name, round(angle, 4))
print("FP_PUNCH_GLB_OK", len(game.data.bones), len(mesh.data.vertices),
      len(bpy.data.actions), tuple(action.frame_range))
