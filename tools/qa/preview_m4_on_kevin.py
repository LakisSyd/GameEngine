"""Quick Blender-side attachment QA; does not save or modify source files."""

from pathlib import Path
from math import asin, atan2, radians, sqrt

import bpy
from mathutils import Matrix, Vector

project = Path(__file__).resolve().parents[2]
bpy.ops.wm.open_mainfile(filepath=str(project / "assets/model/Kevin_Combat_Game.blend"))
scene = bpy.context.scene
rig = bpy.data.objects["Kevin_Game"]
action = bpy.data.actions.get("Kevin_Rifle_Idle") or bpy.data.actions["Kevin_Aim_Rifle"]
rig.animation_data.action = action
scene.frame_set(int(action.frame_range[0]))
bpy.context.view_layer.update()
before = set(bpy.data.objects)
bpy.ops.import_scene.gltf(filepath=str(project / "assets/model/Rifle_M4_CC0.glb"))
gun = next(o for o in bpy.data.objects if o not in before and o.type == "MESH")

bone = rig.pose.bones["mixamorig:RightHand"]
bone_world = rig.matrix_world @ bone.matrix
gun_world_rotation = Matrix.Rotation(radians(195.0), 4, "Z")
bone_rotation = bone_world.to_3x3().normalized()
grip_rotation = (bone_rotation.inverted() @ gun_world_rotation.to_3x3()).to_4x4()
grip = Matrix.Translation(Vector((0.03, 0.08, 0.0))) @ grip_rotation
gun.matrix_world = rig.matrix_world @ bone.matrix @ grip
bpy.context.view_layer.update()
print("ACTION", action.name)
print("GRIP_ROTATION_BLENDER_XYZ_RAD", tuple(round(v, 4) for v in grip_rotation.to_euler("XYZ")))
conversion = Matrix(((1, 0, 0), (0, 0, 1), (0, 1, 0)))
engine_grip_rotation = conversion @ grip_rotation.to_3x3() @ conversion
print("GRIP_ROTATION_ENGINE_XYZ_RAD", tuple(round(v, 4) for v in engine_grip_rotation.to_euler("XYZ")))
dx = engine_grip_rotation.transposed()
dx_pitch = asin(max(-1.0, min(1.0, -dx[2][1])))
dx_yaw = atan2(dx[2][0], dx[2][2])
dx_roll = atan2(dx[0][1], dx[1][1])
print("GRIP_ROTATION_DIRECTXMATH_RAD", tuple(round(v, 4) for v in (dx_pitch, dx_yaw, dx_roll)))
print("GUN_ORIGIN", tuple(round(v, 4) for v in gun.matrix_world.translation))
for label, point in (("LEFT_SOCKET", Vector((0.0, 0.33, 0.035))),
                     ("MUZZLE", Vector((0.0, 0.607, 0.11)))):
    print(label, tuple(round(v, 4) for v in gun.matrix_world @ point))
print("LEFT_HAND", tuple(round(v, 4) for v in rig.matrix_world @ rig.pose.bones["mixamorig:LeftHand"].head))
gun_inverse = gun.matrix_world.inverted()
for name in ("mixamorig:LeftHand", "mixamorig:LeftHandIndex1",
             "mixamorig:LeftHandMiddle1", "mixamorig:LeftHandThumb1"):
    point = rig.matrix_world @ rig.pose.bones[name].head
    print("GUN_LOCAL", name, tuple(round(v, 4) for v in gun_inverse @ point))

for obj in list(bpy.data.objects):
    if obj.type == "MESH" and obj not in {gun, bpy.data.objects.get("Kevin_Body")}:
        bpy.data.objects.remove(obj, do_unlink=True)
body = bpy.data.objects["Kevin_Body"]
for material in body.data.materials:
    material.diffuse_color = (0.38, 0.39, 0.40, 1.0)
scene.render.engine = "BLENDER_WORKBENCH"
scene.display.shading.light = "STUDIO"
scene.display.shading.color_type = "MATERIAL"
scene.display.shading.background_type = "WORLD"
scene.world.color = (0.35, 0.35, 0.35)
scene.render.resolution_x = 1100
scene.render.resolution_y = 1000
scene.render.resolution_percentage = 100
camera_data = bpy.data.cameras.new("QA Camera")
camera = bpy.data.objects.new("QA Camera", camera_data)
scene.collection.objects.link(camera)
camera_data.type = "ORTHO"
scene.camera = camera
for label, location, target, ortho in (
    ("quarter", Vector((2.4, -3.8, 2.6)), Vector((0, -0.5, 1.75)), 1.65),
    ("side", Vector((3.0, -0.35, 1.83)), Vector((0, -0.35, 1.83)), 1.55),
    ("front", Vector((0, -3.0, 1.80)), Vector((0, -0.35, 1.80)), 1.55),
    ("overhead", Vector((0, -0.35, 4.0)), Vector((0, -0.35, 1.8)), 1.4),
):
    camera.location = location
    camera.rotation_euler = (target - location).to_track_quat("-Z", "Y").to_euler()
    if label == "side":
        camera.rotation_euler = Matrix(((0, 0, 1), (1, 0, 0), (0, 1, 0))).to_euler()
    elif label == "front":
        camera.rotation_euler = Matrix(((1, 0, 0), (0, 0, -1), (0, 1, 0))).to_euler()
    elif label == "overhead":
        camera.rotation_euler = (0, 0, 0)
    camera_data.ortho_scale = ortho
    scene.render.filepath = str(Path(__file__).with_name(f"m4_on_kevin_{label}.png"))
    bpy.ops.render.render(write_still=True)
