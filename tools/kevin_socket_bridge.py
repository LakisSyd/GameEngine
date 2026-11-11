"""Coordinate conversion shared by Kevin's Blender socket setup and export.

The engine uses row-vector DirectX matrices and Y-up coordinates. Blender uses
column-vector matrices and Z-up coordinates. Socket positions are in metres in
the normalized 2.2 m game rig, never in the large authoring rig's units.
"""

from math import asin, atan2, cos, pi, sin

from mathutils import Matrix, Vector
import json
from pathlib import Path


AXIS_SWAP = Matrix(((1, 0, 0, 0),
                    (0, 0, 1, 0),
                    (0, 1, 0, 0),
                    (0, 0, 0, 1)))


def normalized_transform(matrix):
    position, rotation, _ = matrix.decompose()
    return Matrix.LocRotScale(position, rotation, Vector((1, 1, 1)))


def directx_row(position, rotation, scale=(1.0, 1.0, 1.0)):
    pitch, yaw, roll = rotation
    cp, sp = cos(pitch), sin(pitch)
    cy, sy = cos(yaw), sin(yaw)
    cr, sr = cos(roll), sin(roll)
    sx, sy_scale, sz = scale
    return Matrix(((sx * (cr * cy + sr * sp * sy), sx * sr * cp,
                    sx * (sr * sp * cy - cr * sy), 0),
                   (sy_scale * (cr * sp * sy - sr * cy), sy_scale * cr * cp,
                    sy_scale * (sr * sy + cr * sp * cy), 0),
                   (sz * cp * sy, -sz * sp, sz * cp * cy, 0),
                   (position[0], position[1], position[2], 1)))


def engine_local_to_blender(position, rotation, scale=(1.0, 1.0, 1.0)):
    return AXIS_SWAP @ directx_row(position, rotation, scale).transposed() @ AXIS_SWAP


def _wrapped(angle):
    while angle > pi:
        angle -= 2 * pi
    while angle < -pi:
        angle += 2 * pi
    return angle


def blender_local_to_engine(matrix):
    row = (AXIS_SWAP @ matrix @ AXIS_SWAP).transposed()
    scale = tuple(Vector(row[i][:3]).length for i in range(3))
    if min(scale) < 1e-7:
        raise ValueError("Socket transform has a zero scale")
    r = [[row[i][j] / scale[i] for j in range(3)] for i in range(3)]
    pitch = asin(max(-1.0, min(1.0, -r[2][1])))
    yaw = atan2(r[2][0], r[2][2])
    roll = atan2(r[0][1], r[1][1])
    first = (pitch, yaw, roll)
    second = (pi - pitch, _wrapped(yaw - pi), _wrapped(roll - pi))
    rotation = min((first, second), key=lambda values: sum(abs(v) for v in values))
    return (tuple(float(row[3][i]) for i in range(3)),
            tuple(float(v) for v in rotation),
            tuple(float(v) for v in scale))


def profile_vec(values):
    return [round(float(value), 7) for value in values]


def game_normalization(mesh):
    world = mesh.matrix_world.copy()
    points = [world @ vertex.co for vertex in mesh.data.vertices]
    height = max(point.z for point in points) - min(point.z for point in points)
    if height <= 0:
        raise ValueError("Character mesh has no height")
    factor = 2.2 / height
    center = Vector(((max(p.x for p in points) + min(p.x for p in points)) / 2,
                     (max(p.y for p in points) + min(p.y for p in points)) / 2,
                     min(p.z for p in points)))
    return Matrix.Scale(factor, 4) @ Matrix.Translation(-center)


def write_socket_sidecar(output, source_blend, right_hand_game,
                         normal, marker_worlds, grip_metadata, aim_frame):
    """Write Blender-authored M4 sockets without changing a saved weapon profile."""
    output = Path(output)
    expected = {'grip', 'left_hand', 'muzzle'}
    document = {
        'version': 1,
        'source_blend': Path(source_blend).name,
        'source_action': 'Kevin_Aim_Rifle',
        'source_frame': aim_frame,
        'weapons': [],
    }
    if expected.issubset(marker_worlds):
        raw_grip = right_hand_game.inverted() @ normal @ marker_worlds['grip']
        grip_position, grip_rotation, _ = raw_grip.decompose()
        uniform_scale = float(grip_metadata.get('dx_engine_scale', 0.8))
        if not 0.01 <= uniform_scale <= 10.0:
            raise ValueError('DX_SOCKET_WeaponGrip dx_engine_scale must be 0.01..10')
        grip_local = Matrix.LocRotScale(
            grip_position, grip_rotation, Vector((uniform_scale,) * 3))
        world_position, world_rotation, world_scale = blender_local_to_engine(grip_local)
        game_gun = right_hand_game @ grip_local

        raw_left = game_gun.inverted() @ normal @ marker_worlds['left_hand']
        left_local = normalized_transform(raw_left)
        left_position, left_rotation, _ = blender_local_to_engine(left_local)

        muzzle_world = normal @ marker_worlds['muzzle'].translation
        muzzle_local = game_gun.inverted() @ muzzle_world
        muzzle_engine = AXIS_SWAP @ muzzle_local

        document['weapons'].append({
            'profile_name': str(grip_metadata.get('dx_profile_name', '')),
            'visual_model': str(grip_metadata.get(
                'dx_weapon_model', 'assets/model/Rifle_M4_CC0.glb')),
            'right_hand_bone': str(grip_metadata.get(
                'dx_right_hand_bone', 'mixamorig:RightHand')),
            'world_position': profile_vec(world_position),
            'world_rotation': profile_vec(world_rotation),
            'world_scale': profile_vec(world_scale),
            'left_hand_grip': profile_vec(left_position),
            'left_hand_rotation': profile_vec(left_rotation),
            'muzzle_position': profile_vec(muzzle_engine[:3]),
        })
    else:
        print('SOCKET_EXPORT_SKIPPED missing', sorted(expected - marker_worlds))

    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(document, indent=2) + '\n', encoding='utf-8')
    print('SOCKET_EXPORT', output, 'weapons', len(document['weapons']))
    return document
