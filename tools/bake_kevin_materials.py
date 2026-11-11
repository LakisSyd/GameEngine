"""Bake Kevin's procedural Blender materials to one glTF-friendly atlas."""

import argparse
import math
from pathlib import Path
import sys

import bpy


def args():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--texture', type=Path, required=True)
    parser.add_argument('--size', type=int, default=1024)
    values = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    return parser.parse_args(values)


def input_named(node, *names):
    for name in names:
        socket = node.inputs.get(name)
        if socket:
            return socket
    return None


options = args()
mesh = (bpy.data.objects.get('MaleBaseMesh LowPoly') or
        bpy.data.objects.get('Kevin_FP_Arms') or
        bpy.data.objects.get('Kevin_Body'))
if not mesh or mesh.type != 'MESH':
    raise RuntimeError('Kevin body mesh was not found')
if not mesh.data.materials:
    raise RuntimeError('Kevin has no source materials to bake')

# The imported mesh stores every triangle as a separate island. A conventional
# unwrap therefore produces tens of thousands of tiny islands and dark mip
# seams. Use one cylindrical projection and reserve a horizontal atlas band
# for each of the five authored material regions.
if mesh.data.uv_layers:
    mesh.data.uv_layers.remove(mesh.data.uv_layers[0])
uv = mesh.data.uv_layers.new(name='Kevin_Human_UV')
mesh.data.uv_layers.active = uv
mesh.data.uv_layers.active_index = 0
world_points = [mesh.matrix_world @ vertex.co for vertex in mesh.data.vertices]
min_z = min(point.z for point in world_points)
max_z = max(point.z for point in world_points)
height = max(max_z - min_z, 0.0001)
band_count = 5
band_padding = 0.012
for polygon in mesh.data.polygons:
    category = min(max(polygon.material_index, 0), band_count - 1)
    u_values = []
    for loop_index in polygon.loop_indices:
        point = world_points[mesh.data.loops[loop_index].vertex_index]
        u_values.append(math.atan2(point.y, point.x) / (2.0 * math.pi) + 0.5)
    if max(u_values) - min(u_values) > 0.5:
        u_values = [value + 1.0 if value < 0.5 else value
                    for value in u_values]
    for loop_index, value_u in zip(polygon.loop_indices, u_values):
        point = world_points[mesh.data.loops[loop_index].vertex_index]
        value_z = min(max((point.z - min_z) / height, 0.0), 1.0)
        local_v = band_padding + value_z * (1.0 - 2.0 * band_padding)
        uv.data[loop_index].uv = (value_u,
                                  (category + local_v) / band_count)

image_name = 'Kevin_Human_BaseColor_FP' if 'FP' in options.texture.stem else 'Kevin_Human_BaseColor'
old = bpy.data.images.get(image_name)
if old:
    bpy.data.images.remove(old)
image = bpy.data.images.new(image_name, width=options.size,
                            height=options.size, alpha=False,
                            float_buffer=False)
palette = (
    ((0.40, 0.16, 0.08), (0.68, 0.33, 0.18)),       # skin
    ((0.018, 0.006, 0.002), (0.11, 0.038, 0.010)),   # hair
    ((0.028, 0.070, 0.135), (0.075, 0.19, 0.34)),    # shirt
    ((0.045, 0.058, 0.048), (0.12, 0.15, 0.12)),     # trousers
    ((0.018, 0.014, 0.010), (0.075, 0.045, 0.025)),  # footwear/reserved
)
pixels = []
two_pi = 2.0 * math.pi
for y in range(options.size):
    atlas_v = (y + 0.5) / options.size
    category = min(int(atlas_v * band_count), band_count - 1)
    local_v = atlas_v * band_count - category
    dark, light = palette[category]
    for x in range(options.size):
        value_u = (x + 0.5) / options.size
        noise = (0.5 +
                 0.16 * math.sin(two_pi * 7.0 * value_u) *
                 math.sin(two_pi * 11.0 * local_v) +
                 0.08 * math.sin(two_pi *
                                 (23.0 * value_u + 5.0 * local_v)))
        noise = min(max(noise, 0.0), 1.0)
        pixels.extend((dark[0] + (light[0] - dark[0]) * noise,
                       dark[1] + (light[1] - dark[1]) * noise,
                       dark[2] + (light[2] - dark[2]) * noise, 1.0))
image.pixels.foreach_set(pixels)
image.update()

options.texture.parent.mkdir(parents=True, exist_ok=True)
image.filepath_raw = str(options.texture.resolve())
image.file_format = 'PNG'
image.save()

# Replace the procedural network with the small subset exported reliably by
# Blender's glTF exporter and read by Assimp in the engine.
material = bpy.data.materials.get('Kevin • Baked Human') or bpy.data.materials.new('Kevin • Baked Human')
material.use_nodes = True
nodes = material.node_tree.nodes
links = material.node_tree.links
nodes.clear()
output = nodes.new('ShaderNodeOutputMaterial')
output.location = (420, 0)
shader = nodes.new('ShaderNodeBsdfPrincipled')
shader.location = (140, 0)
input_named(shader, 'Roughness').default_value = 0.68
image_node = nodes.new('ShaderNodeTexImage')
image_node.location = (-180, 20)
image_node.image = image
image_node.interpolation = 'Linear'
image_node.extension = 'REPEAT'
links.new(image_node.outputs['Color'], input_named(shader, 'Base Color'))
links.new(shader.outputs['BSDF'], output.inputs['Surface'])

mesh.data.materials.clear()
mesh.data.materials.append(material)
for polygon in mesh.data.polygons:
    polygon.material_index = 0
mesh['Kevin_Baked_Texture'] = str(options.texture.resolve())

options.output.parent.mkdir(parents=True, exist_ok=True)
bpy.context.preferences.filepaths.save_version = 0
bpy.ops.wm.save_as_mainfile(filepath=str(options.output.resolve()),
                            compress=True, check_existing=False)
print('KEVIN_BAKE_OK', mesh.name, len(mesh.data.polygons),
      len(mesh.data.uv_layers), image.size[:], options.texture, options.output)
