from pathlib import Path

import bpy
from mathutils import Quaternion, Vector


OUTPUT_DIR = Path(r"C:\Users\Lakis\Desktop\DX_FrameWork\DX_FrameWork\tools\mainplayer_new_qa")
OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

armature = next(obj for obj in bpy.data.objects if obj.type == "ARMATURE")
mesh_obj = next(obj for obj in bpy.data.objects if obj.type == "MESH")
scene = bpy.context.scene

if armature.animation_data:
    armature.animation_data.action = None
for pose_bone in armature.pose.bones:
    pose_bone.location = (0.0, 0.0, 0.0)
    pose_bone.rotation_mode = "QUATERNION"
    pose_bone.rotation_quaternion = Quaternion()
    pose_bone.scale = (1.0, 1.0, 1.0)
bpy.context.view_layer.update()

for obj in bpy.data.objects:
    obj.hide_render = obj != mesh_obj

scene.render.engine = "BLENDER_WORKBENCH"
scene.render.resolution_x = 900
scene.render.resolution_y = 900
scene.render.resolution_percentage = 100
scene.render.film_transparent = False
scene.render.image_settings.file_format = "PNG"
scene.display.shading.light = "STUDIO"
scene.display.shading.color_type = "MATERIAL"
scene.display.shading.show_shadows = True
scene.display.shading.show_cavity = True
scene.display.shading.cavity_type = "WORLD"
scene.display.shading.background_type = "VIEWPORT"
scene.display.shading.background_color = (0.025, 0.03, 0.04)

camera_data = bpy.data.cameras.new("Hand_QA_Camera")
camera = bpy.data.objects.new("Hand_QA_Camera", camera_data)
scene.collection.objects.link(camera)
scene.camera = camera
camera.data.type = "ORTHO"


def bone_point(name, tail=False):
    bone = armature.data.bones[name]
    point = bone.tail_local if tail else bone.head_local
    return armature.matrix_world @ point


hips = bone_point("mixamorig:Hips")
head = bone_point("mixamorig:Head", tail=True)
left_hand = bone_point("mixamorig:LeftHand")
right_hand = bone_point("mixamorig:RightHand")
up = (head - hips).normalized()
right_axis = (left_hand - right_hand).normalized()
forward = right_axis.cross(up).normalized()


def render(name, center, view_direction, up_axis, ortho_scale):
    camera.data.ortho_scale = ortho_scale
    camera.location = center + view_direction.normalized() * 2.5
    camera.rotation_euler = (center - camera.location).to_track_quat("-Z", up_axis).to_euler()
    output = OUTPUT_DIR / f"{name}.png"
    scene.render.filepath = str(output)
    bpy.ops.render.render(write_still=True)
    print("RENDER", output)


bounds = [mesh_obj.matrix_world @ Vector(corner) for corner in mesh_obj.bound_box]
body_center = sum(bounds, Vector()) / len(bounds)
body_height = max(point.z for point in bounds) - min(point.z for point in bounds)
render("full_front", body_center, -forward, "Z", body_height * 1.08)

for side in ("Left", "Right"):
    hand = bone_point(f"mixamorig:{side}Hand")
    index_tip = bone_point(f"mixamorig:{side}HandIndex3", tail=True)
    thumb_tip = bone_point(f"mixamorig:{side}HandThumb3", tail=True)
    center = (hand + index_tip + thumb_tip) / 3.0
    render(f"{side}_front", center, -forward, "Z", 0.19)
    render(f"{side}_back", center, forward, "Z", 0.19)
    render(f"{side}_side", center, right_axis if side == "Left" else -right_axis, "Z", 0.19)
