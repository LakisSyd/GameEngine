import bpy
from mathutils import Vector
from pathlib import Path


OUTPUT = Path(r"C:\Users\Lakis\Desktop\MainPlayer_FP_preview.png")
mesh_obj = next(obj for obj in bpy.data.objects if obj.type == "MESH")
arm_obj = next(obj for obj in bpy.data.objects if obj.type == "ARMATURE")
arm_obj.hide_render = True

arm_obj.animation_data_create()
arm_obj.animation_data.action = bpy.data.actions.get("Pistol_AIM")

scene = bpy.context.scene
scene.frame_set(17)
scene.render.engine = "BLENDER_WORKBENCH"
scene.render.resolution_x = 1000
scene.render.resolution_y = 800
scene.render.resolution_percentage = 100
scene.render.film_transparent = True
scene.render.image_settings.file_format = "PNG"
scene.render.filepath = str(OUTPUT)
scene.display.shading.light = "STUDIO"
scene.display.shading.color_type = "MATERIAL"
scene.display.shading.show_shadows = True
scene.display.shading.show_cavity = True
scene.display.shading.cavity_type = "WORLD"

camera_data = bpy.data.cameras.new("FP_QA_Camera")
camera = bpy.data.objects.new("FP_QA_Camera", camera_data)
bpy.context.collection.objects.link(camera)
scene.camera = camera

bounds = [mesh_obj.matrix_world @ Vector(corner) for corner in mesh_obj.bound_box]
center = sum(bounds, Vector()) / len(bounds)
width = max(point.x for point in bounds) - min(point.x for point in bounds)
height = max(point.z for point in bounds) - min(point.z for point in bounds)

camera.data.type = "ORTHO"
camera.data.ortho_scale = max(height * 1.22, width * 0.96)
camera.location = center + Vector((0.0, -2.0, 0.02))
camera.rotation_euler = (center - camera.location).to_track_quat("-Z", "Y").to_euler()

bpy.ops.render.render(write_still=True)
print("PREVIEW", OUTPUT)
