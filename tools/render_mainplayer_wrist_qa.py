import math
import sys
from argparse import ArgumentParser
from pathlib import Path

import bpy
from mathutils import Quaternion, Vector


arguments = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
parser = ArgumentParser()
parser.add_argument("--angle", type=float, default=40.0)
options = parser.parse_args(arguments)


armature = bpy.data.objects["Main"]
mesh_obj = bpy.data.objects["Suit_Suit_0.003"]
scene = bpy.context.scene

if armature.animation_data:
    armature.animation_data.action = None
for pose_bone in armature.pose.bones:
    pose_bone.location = (0.0, 0.0, 0.0)
    pose_bone.rotation_mode = "QUATERNION"
    pose_bone.rotation_quaternion = Quaternion()
    pose_bone.scale = (1.0, 1.0, 1.0)

for obj in bpy.data.objects:
    obj.hide_render = obj != mesh_obj

scene.render.engine = "BLENDER_WORKBENCH"
scene.render.resolution_x = 1200
scene.render.resolution_y = 700
scene.render.resolution_percentage = 100
scene.render.film_transparent = False
scene.render.image_settings.file_format = "PNG"
scene.display.shading.light = "STUDIO"
scene.display.shading.color_type = "MATERIAL"
scene.display.shading.show_shadows = True
scene.display.shading.show_cavity = True
scene.display.shading.cavity_type = "WORLD"
scene.display.shading.background_type = "VIEWPORT"
scene.display.shading.background_color = (0.035, 0.04, 0.05)

camera_data = bpy.data.cameras.new("Wrist_QA_Camera")
camera = bpy.data.objects.new("Wrist_QA_Camera", camera_data)
bpy.context.scene.collection.objects.link(camera)
scene.camera = camera
camera.data.type = "ORTHO"
camera.data.ortho_scale = 0.155

stem = Path(bpy.data.filepath).stem
output_directory = Path(r"C:\Users\Lakis\Desktop\DX_FrameWork\DX_FrameWork\tools\qa")
output_directory.mkdir(parents=True, exist_ok=True)

for side, sign in (("Left", 1.0), ("Right", -1.0)):
    hand_name = f"mixamorig:{side}Hand"
    wrist = armature.matrix_world @ armature.data.bones[hand_name].head_local
    camera.location = wrist + Vector((0.0, -2.0, 0.0))
    camera.rotation_euler = (wrist - camera.location).to_track_quat("-Z", "Y").to_euler()
    for axis_name, axis in (("X", Vector((1, 0, 0))), ("Y", Vector((0, 1, 0))), ("Z", Vector((0, 0, 1)))):
        armature.pose.bones[hand_name].rotation_quaternion = Quaternion(axis, math.radians(options.angle * sign))
        bpy.context.view_layer.update()
        output = output_directory / f"{stem}_{side}_wrist_{axis_name}_{options.angle:g}deg.png"
        scene.render.filepath = str(output)
        bpy.ops.render.render(write_still=True)
        print("RENDER", output)
        armature.pose.bones[hand_name].rotation_quaternion = Quaternion()
