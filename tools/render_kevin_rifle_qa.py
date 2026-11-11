"""Render front and side upper-body checks from the loaded Kevin .blend."""

from pathlib import Path

import bpy
from mathutils import Vector


OUT = Path(r"C:\Users\karin\OneDrive\Desktop\DX_FrameWork\DX_FrameWork\tools\kevin_rifle_qa")
OUT.mkdir(parents=True, exist_ok=True)
for name in ("MaleBaseMesh LowPoly.001", "Armature.001"):
    obj = bpy.data.objects.get(name)
    if obj:
        obj.hide_render = True
for obj in bpy.data.objects:
    if obj.type == "MESH" and obj.name.startswith("WGT_"):
        obj.hide_render = True

scene = bpy.context.scene
scene.render.engine = "BLENDER_WORKBENCH"
scene.render.resolution_x = 900
scene.render.resolution_y = 900
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = "PNG"
scene.render.film_transparent = False
scene.display.shading.light = "STUDIO"
scene.display.shading.color_type = "MATERIAL"
scene.display.shading.show_cavity = True
scene.display.shading.cavity_type = "BOTH"

camera_data = bpy.data.cameras.new("QA camera")
camera = bpy.data.objects.new("QA camera", camera_data)
scene.collection.objects.link(camera)
scene.camera = camera
camera_data.type = "ORTHO"
camera_data.ortho_scale = 3.9
target = Vector((0.35, -2.2, 5.55))

for label, position in (
    ("front", Vector((0.35, -10.0, 5.55))),
    ("side", Vector((8.0, -2.2, 5.55))),
    ("three_quarter", Vector((5.5, -7.0, 5.55))),
):
    camera.location = position
    direction = target - position
    camera.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
    scene.render.filepath = str(OUT / f"{label}.png")
    bpy.ops.render.render(write_still=True)
    print("QA_RENDER", scene.render.filepath)
