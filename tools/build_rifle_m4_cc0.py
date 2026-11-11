"""Convert Tabasco's CC0 M4 into an engine-ready, hand-grip-centered GLB.

Download smallarms.zip from:
https://opengameart.org/content/various-small-arms-assault-rifles-sniper-pistol
Extract m4.blend, then run:
blender --background --python tools/build_rifle_m4_cc0.py -- --source PATH_TO_m4.blend

The source file is an old Blender format; only its static mesh is used.
"""

import argparse
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Vector


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--preview", type=Path)
    return parser.parse_args(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])


args = parse_args()
project = Path(__file__).resolve().parents[1]
output = args.output or project / "assets/model/Rifle_M4_CC0.glb"
preview = args.preview or project / "tools/qa/rifle_m4_cc0_preview.png"
output.parent.mkdir(parents=True, exist_ok=True)
preview.parent.mkdir(parents=True, exist_ok=True)

bpy.ops.wm.open_mainfile(filepath=str(args.source.resolve()))
scene = bpy.context.scene
gun = bpy.data.objects.get("Cube")
if gun is None or gun.type != "MESH":
    raise RuntimeError("Expected the static M4 mesh named 'Cube' in Tabasco's m4.blend")

# The source spans 10.712245 Blender units from muzzle to butt. Convert it
# to a 90 cm carbine. The old mesh points down source -X; turn it to Blender
# +Y so artists can place the support hand ahead of the right pistol grip.
source_scale = 0.90 / 10.712245
source_right_grip = Vector((2.30, 0.0, -0.70))
original = [gun.matrix_world @ vertex.co for vertex in gun.data.vertices]
for vertex, point in zip(gun.data.vertices, original):
    relative = point - source_right_grip
    vertex.co = Vector((relative.y, -relative.x, relative.z)) * source_scale
gun.matrix_world = Matrix.Identity(4)
gun.name = "Rifle_M4_CC0"
gun.data.name = "Rifle_M4_CC0_Mesh"
for modifier in list(gun.modifiers):
    gun.modifiers.remove(modifier)
gun.vertex_groups.clear()

# The old file predates modern PBR. Keep its original material separation
# while assigning restrained values that read well in third person.
palette = (
    ("Magazine steel", (0.17, 0.18, 0.19, 1.0), 0.55, 0.64),
    ("Gunmetal", (0.35, 0.37, 0.39, 1.0), 0.45, 0.63),
    ("Black polymer", (0.09, 0.10, 0.11, 1.0), 0.04, 0.77),
)
for material, (name, color, metallic, roughness) in zip(gun.data.materials, palette):
    material.name = name
    material.diffuse_color = color
    material.use_nodes = True
    material.node_tree.nodes.clear()
    bsdf = material.node_tree.nodes.new("ShaderNodeBsdfPrincipled")
    output_node = material.node_tree.nodes.new("ShaderNodeOutputMaterial")
    material.node_tree.links.new(bsdf.outputs["BSDF"], output_node.inputs["Surface"])
    bsdf.inputs["Base Color"].default_value = color
    bsdf.inputs["Metallic"].default_value = metallic
    bsdf.inputs["Roughness"].default_value = roughness

for obj in list(bpy.data.objects):
    if obj != gun:
        bpy.data.objects.remove(obj, do_unlink=True)

# A single GLB with one static mesh. This origin is where the right palm
# wraps the pistol grip. Left grip and muzzle are recorded in the source note.
bpy.ops.object.select_all(action="DESELECT")
gun.select_set(True)
bpy.context.view_layer.objects.active = gun
bpy.ops.export_scene.gltf(
    filepath=str(output.resolve()),
    export_format="GLB",
    use_selection=True,
    export_animations=False,
    export_yup=True,
)

# Workbench QA from the right side. The muzzle must appear to the right.
scene.render.engine = "BLENDER_WORKBENCH"
scene.display.shading.light = "FLAT"
scene.display.shading.color_type = "MATERIAL"
scene.display.shading.show_cavity = False
scene.display.shading.background_type = "WORLD"
scene.world.color = (0.24, 0.25, 0.26)
scene.render.resolution_x = 1400
scene.render.resolution_y = 700
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = "PNG"
target = Vector((0.0, 0.15, 0.05))
bpy.ops.object.camera_add(location=(2.0, 0.15, 0.05))
camera = bpy.context.object
# Camera local X is rifle +Y, local Y is world +Z, local Z is world +X.
camera.rotation_euler = Matrix(((0, 0, 1), (1, 0, 0), (0, 1, 0))).to_euler()
camera.data.type = "ORTHO"
camera.data.ortho_scale = 1.1
scene.camera = camera
scene.render.filepath = str(preview.resolve())
bpy.ops.render.render(write_still=True)

points = [gun.matrix_world @ v.co for v in gun.data.vertices]
minimum = tuple(round(min(point[i] for point in points), 4) for i in range(3))
maximum = tuple(round(max(point[i] for point in points), 4) for i in range(3))
print("M4_OUTPUT", output.resolve())
print("M4_PREVIEW", preview.resolve())
print("M4_BOUNDS_BLENDER_XYZ", minimum, maximum)
print("M4_HANDS_BLENDER_XYZ", "right", (0.0, 0.0, 0.0),
      "left", (0.0, 0.33, 0.035), "muzzle", (0.0, 0.607, 0.11))
