"""Assign human PBR materials to Kevin without changing the rig or actions.

Run with a source .blend open and optionally pass after ``--``:
    --output <copy.blend>

The source mesh has no UV map and one black material. Materials therefore use
Generated coordinates, while polygon assignment follows the existing Mixamo
skin weights. This works for both the full body and the cropped FP arms mesh.
"""

import argparse
from pathlib import Path

import bpy
from mathutils import Vector


def arguments():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    argv = []
    if '--' in __import__('sys').argv:
        argv = __import__('sys').argv[__import__('sys').argv.index('--') + 1:]
    return parser.parse_args(argv)


def input_named(node, *names):
    for name in names:
        socket = node.inputs.get(name)
        if socket:
            return socket
    return None


def make_material(name, dark, light, roughness, noise_scale,
                  bump_strength=0.08, metallic=0.0):
    material = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    material.use_nodes = True
    material.diffuse_color = (*light, 1.0)
    nodes = material.node_tree.nodes
    links = material.node_tree.links
    nodes.clear()

    output = nodes.new('ShaderNodeOutputMaterial')
    output.location = (560, 0)
    shader = nodes.new('ShaderNodeBsdfPrincipled')
    shader.location = (280, 0)
    input_named(shader, 'Roughness').default_value = roughness
    input_named(shader, 'Metallic').default_value = metallic

    coordinates = nodes.new('ShaderNodeTexCoord')
    coordinates.location = (-720, 0)
    noise = nodes.new('ShaderNodeTexNoise')
    noise.location = (-500, 20)
    noise.noise_dimensions = '3D'
    noise.inputs['Scale'].default_value = noise_scale
    noise.inputs['Detail'].default_value = 4.0
    noise.inputs['Roughness'].default_value = 0.7

    ramp = nodes.new('ShaderNodeValToRGB')
    ramp.location = (-220, 80)
    ramp.color_ramp.elements[0].position = 0.28
    ramp.color_ramp.elements[0].color = (*dark, 1.0)
    ramp.color_ramp.elements[1].position = 0.72
    ramp.color_ramp.elements[1].color = (*light, 1.0)

    bump = nodes.new('ShaderNodeBump')
    bump.location = (40, -150)
    bump.inputs['Strength'].default_value = bump_strength
    bump.inputs['Distance'].default_value = 0.025

    links.new(coordinates.outputs['Generated'], noise.inputs['Vector'])
    links.new(noise.outputs['Fac'], ramp.inputs['Fac'])
    links.new(ramp.outputs['Color'], input_named(shader, 'Base Color'))
    links.new(noise.outputs['Fac'], bump.inputs['Height'])
    links.new(bump.outputs['Normal'], input_named(shader, 'Normal'))
    links.new(shader.outputs['BSDF'], output.inputs['Surface'])
    return material


def vertex_scores(mesh, vertex):
    scores = {'skin': 0.0, 'hair': 0.0, 'shirt': 0.0, 'pants': 0.0,
              'shoes': 0.0, 'head': 0.0}
    for membership in vertex.groups:
        if membership.group >= len(mesh.vertex_groups):
            continue
        name = mesh.vertex_groups[membership.group].name
        weight = membership.weight
        if 'Head' in name:
            scores['skin'] += weight
            scores['head'] += weight
        elif any(token in name for token in ('Neck', 'ForeArm', 'Hand')):
            scores['skin'] += weight
        elif any(token in name for token in (
                'Spine', 'Shoulder', 'Arm')):
            scores['shirt'] += weight
        elif any(token in name for token in ('Foot', 'ToeBase')):
            # The base mesh has modeled toes rather than footwear geometry.
            scores['skin'] += weight
        elif any(token in name for token in ('Hips', 'UpLeg', 'Leg')):
            scores['pants'] += weight
    return scores


def assign_human_materials(mesh):
    materials = {
        # Warm medium skin with subtle low-frequency variation.
        'skin': make_material('Kevin • Skin', (0.33, 0.13, 0.065),
                              (0.52, 0.245, 0.13), 0.54, 10.0, 0.025),
        'hair': make_material('Kevin • Hair', (0.010, 0.004, 0.002),
                              (0.060, 0.022, 0.008), 0.70, 35.0, 0.16),
        # Navy knitted shirt, graphite cargo trousers and dark leather boots.
        'shirt': make_material('Kevin • Shirt', (0.018, 0.032, 0.050),
                               (0.040, 0.075, 0.110), 0.76, 65.0, 0.12),
        'pants': make_material('Kevin • Trousers', (0.025, 0.030, 0.027),
                               (0.058, 0.068, 0.058), 0.82, 48.0, 0.11),
        'shoes': make_material('Kevin • Boots', (0.012, 0.010, 0.009),
                               (0.045, 0.030, 0.020), 0.43, 26.0, 0.16),
    }

    mesh.data.materials.clear()
    order = ('skin', 'hair', 'shirt', 'pants', 'shoes')
    for key in order:
        mesh.data.materials.append(materials[key])
    slot_for = {key: index for index, key in enumerate(order)}

    cached = [vertex_scores(mesh, vertex) for vertex in mesh.data.vertices]
    counts = {key: 0 for key in order}
    for polygon in mesh.data.polygons:
        totals = {key: 0.0 for key in order}
        head_weight = 0.0
        for vertex_index in polygon.vertices:
            for key in order:
                totals[key] += cached[vertex_index][key]
            head_weight += cached[vertex_index]['head']
        category = max(('skin', 'shirt', 'pants', 'shoes'),
                       key=lambda key: totals[key])
        # Vertices without recognized weights belong to the central body.
        if totals[category] <= 0.0001:
            category = 'shirt'
        # Hair is sculpted into the same connected head mesh. The character
        # faces -Y, so this catches the crown and rear cap without darkening
        # the face, ears, or neck.
        center = sum((mesh.matrix_world @ mesh.data.vertices[index].co
                      for index in polygon.vertices), Vector()) / len(polygon.vertices)
        if head_weight > 0.8 and (center.z > 5.72 or
                                 (center.y > 0.12 and center.z > 5.15)):
            category = 'hair'
        polygon.material_index = slot_for[category]
        counts[category] += 1

    mesh['Kevin_Human_Material_Version'] = 1
    return counts


args = arguments()
mesh = (bpy.data.objects.get('MaleBaseMesh LowPoly') or
        bpy.data.objects.get('Kevin_FP_Arms') or
        bpy.data.objects.get('Kevin_Body'))
if not mesh or mesh.type != 'MESH':
    raise RuntimeError('Kevin body mesh was not found')

counts = assign_human_materials(mesh)
args.output.parent.mkdir(parents=True, exist_ok=True)
bpy.context.preferences.filepaths.save_version = 0
bpy.ops.wm.save_as_mainfile(filepath=str(args.output), compress=True,
                            check_existing=False)
print('KEVIN_TEXTURED', mesh.name, counts, args.output)
