"""Author matching editable melee actions on the Kevin master or FP control rig.

Run with Blender 5.2: blender --background <authoring.blend> --python author_kevin_melee.py
The scene's existing character, controllers, constraints, and actions are kept.
"""

import bpy
from mathutils import Matrix, Vector


rig = bpy.data.objects['Armature']
scene = bpy.context.scene
animation = rig.animation_data_create()
source = bpy.data.actions['Kevin_Punch.1']
is_first_person = 'FP_Source_Body_Bounds_Min' in rig
initial_basis = {bone.name: bone.matrix_basis.copy() for bone in rig.pose.bones}
for track in animation.nla_tracks:
    track.mute = True


def activate(action):
    animation.action = None
    # Some original fist controls are intentionally stored as the scene's
    # unkeyed pose. Restoring the loaded values preserves their closed shape.
    for bone in rig.pose.bones:
        bone.matrix_basis = initial_basis[bone.name]
    animation.action = action
    if action.slots:
        animation.action_slot = next(
            (slot for slot in action.slots if slot.target_id_type == 'OBJECT'),
            action.slots[0])


def move_world(name, offset):
    bone = rig.pose.bones[name]
    matrix = bone.matrix.copy()
    matrix.translation += rig.matrix_world.inverted().to_3x3() @ Vector(offset)
    bone.matrix = matrix
    bpy.context.view_layer.update()


def snapshot():
    result = {}
    for bone in rig.pose.bones:
        props = {name: value for name, value in bone.items()
                 if isinstance(value, (int, float, bool))}
        result[bone.name] = (
            bone.location.copy(), bone.rotation_euler.copy(),
            bone.rotation_quaternion.copy(), bone.scale.copy(), props)
    return result


fp_jab_offsets = {
    1: (0, 0, 0), 3: (0, 0.04, -0.02),
    5: (-0.01, -0.15, 0.02), 7: (-0.03, -0.35, 0.05),
    9: (-0.05, -0.58, 0.08), 10: (-0.05, -0.64, 0.09),
    12: (-0.05, -0.55, 0.07), 15: (-0.02, -0.25, 0.03),
    18: (0, -0.04, 0.01), 22: (0, 0, 0),
}
jab_chest_turn = {
    1: 0.0, 3: 0.015, 5: 0.0, 7: -0.025, 9: -0.045,
    10: -0.05, 12: -0.04, 15: -0.015, 18: 0.0, 22: 0.0,
}
jab_support_offsets = {
    1: (0, 0, 0), 3: (-0.01, 0, 0.01),
    5: (-0.025, -0.02, 0.02), 7: (-0.04, -0.05, 0.035),
    9: (-0.055, -0.08, 0.05), 10: (-0.06, -0.09, 0.055),
    12: (-0.055, -0.08, 0.05), 15: (-0.03, -0.04, 0.025),
    18: (-0.01, -0.01, 0.008), 22: (0, 0, 0),
}

activate(source)
scene.frame_set(0)
bpy.context.view_layer.update()
guard_right_world = (rig.matrix_world @ rig.pose.bones['CTRL_Hand_IK.R'].head).copy()


def sample(spec, fp_jab=False):
    frame, source_frame, moves, rotations = spec
    if fp_jab:
        # Keep the jab on the guard's forward axis in both views. The original
        # Punch clip rotates the hips and sends the fist across the target.
        source_frame = 0
        moves = dict(moves)
        left = fp_jab_offsets[frame]
        moves['CTRL_Hand_IK.L'] = left if is_first_person else (
            left[0], left[1] * 1.7, left[2] * 1.2)
        if not is_first_person:
            rotations = dict(rotations)
            rotations['CTRL_Chest'] = (0, 0, jab_chest_turn[frame])
    activate(source)
    scene.frame_set(source_frame)
    bpy.context.view_layer.update()
    # Keep the feet and guard grounded in the source boxing stance. Add torso
    # turn first, then set the attacking IK hand and pole in world space.
    for name, angles in rotations.items():
        bone = rig.pose.bones[name]
        for axis, angle in enumerate(angles):
            bone.rotation_euler[axis] += angle
    bpy.context.view_layer.update()
    for name, offset in moves.items():
        move_world(name, offset)
    if fp_jab and not is_first_person:
        target = guard_right_world + Vector(jab_support_offsets[frame])
        current = rig.matrix_world @ rig.pose.bones['CTRL_Hand_IK.R'].head
        move_world('CTRL_Hand_IK.R', target - current)
    return frame, snapshot()


def write(name, description, specs, fp_jab=False):
    captured = [sample(spec, fp_jab=fp_jab) for spec in specs]
    old = bpy.data.actions.get(name)
    if old:
        if animation.action == old:
            animation.action = None
        bpy.data.actions.remove(old)
    action = bpy.data.actions.new(name)
    action.use_fake_user = True
    action['Kevin_Melee_Description'] = description
    animation.action = action
    for frame, state in captured:
        scene.frame_set(frame)
        for bone_name, (location, euler, quaternion, scale, props) in state.items():
            bone = rig.pose.bones[bone_name]
            bone.location = location
            bone.rotation_euler = euler
            bone.rotation_quaternion = quaternion
            bone.scale = scale
            for key, value in props.items():
                bone[key] = value
            for path in ('location', 'rotation_quaternion' if bone.rotation_mode == 'QUATERNION' else 'rotation_euler', 'scale'):
                bone.keyframe_insert(data_path=path, frame=frame, group=bone_name)
            for key in props:
                bone.keyframe_insert(data_path=f'["{key}"]', frame=frame, group=bone_name)
    if action.slots:
        animation.action_slot = action.slots[0]
    print('MELEE_ACTION', action.name, tuple(action.frame_range), len(action.layers))
    return action


def spec(frame, source_frame=0, move=None, rotate=None):
    return frame, source_frame, move or {}, rotate or {}


# A short, seamless breathing cycle. It matches the first and last frame of
# every strike, so any punch can blend straight back into guard.
guard = [
    spec(1),
    spec(13, move={'CTRL_Hand_IK.L': (0, -0.012, 0.018),
                   'CTRL_Hand_IK.R': (0, -0.012, 0.018)},
         rotate={'CTRL_Chest': (0.012, 0, 0)}),
    spec(25),
    spec(37, move={'CTRL_Hand_IK.L': (0, 0.010, -0.012),
                   'CTRL_Hand_IK.R': (0, 0.010, -0.012)},
         rotate={'CTRL_Chest': (-0.010, 0, 0)}),
    spec(49),
]

# The left straight starts from the compact guard and drives forward with a
# convincing torso wind-up. Retiming puts the contact at frame 10 (30 fps).
jab = [
    spec(1, 0),
    spec(3, 3, {'CTRL_Hand_IK.R': (0, -0.015, 0.012)}),
    spec(5, 9, {'CTRL_Hand_IK.R': (0.025, -0.035, 0.025)}),
    spec(7, 16, {'CTRL_Hand_IK.R': (0.055, -0.060, 0.045)}),
    spec(9, 22, {'CTRL_Hand_IK.R': (0.09, -0.09, 0.065)}),
    spec(10, 25, {'CTRL_Hand_IK.R': (0.10, -0.10, 0.07)}),
    spec(12, 28, {'CTRL_Hand_IK.R': (0.08, -0.08, 0.055)}),
    spec(15, 36, {'CTRL_Hand_IK.R': (0.04, -0.04, 0.03)}),
    spec(18, 47, {'CTRL_Hand_IK.R': (0.01, -0.01, 0.008)}),
    spec(22, 52),
]

cross = [
    spec(1),
    spec(4, move={'CTRL_Hand_IK.R': (-0.05, 0.14, -0.04),
                  'CTRL_Hand_IK.L': (0.025, 0.025, 0.015)},
         rotate={'CTRL_Chest': (0, 0, -0.065)}),
    spec(7, move={'CTRL_Hand_IK.R': (0.13, -0.48, 0.10),
                  'CTRL_Hand_IK.L': (-0.025, -0.065, 0.055)},
         rotate={'CTRL_Chest': (0, 0, 0.10), 'CTRL_COG': (0, 0, 0.035)}),
    spec(10, move={'CTRL_Hand_IK.R': (0.33, -1.05, 0.23),
                   'CTRL_Hand_IK.L': (-0.06, -0.105, 0.075),
                   'CTRL_Elbow_Pole.R': (0, -0.25, 0.38)},
         rotate={'CTRL_Chest': (0, 0, 0.21), 'CTRL_COG': (0, 0, 0.08)}),
    spec(11, move={'CTRL_Hand_IK.R': (0.35, -1.12, 0.24),
                   'CTRL_Hand_IK.L': (-0.065, -0.11, 0.08),
                   'CTRL_Elbow_Pole.R': (0, -0.27, 0.4)},
         rotate={'CTRL_Chest': (0, 0, 0.22), 'CTRL_COG': (0, 0, 0.08)}),
    spec(14, move={'CTRL_Hand_IK.R': (0.14, -0.53, 0.11),
                   'CTRL_Hand_IK.L': (-0.025, -0.055, 0.045)},
         rotate={'CTRL_Chest': (0, 0, 0.10), 'CTRL_COG': (0, 0, 0.035)}),
    spec(19, move={'CTRL_Hand_IK.R': (0, 0.025, 0.02),
                   'CTRL_Hand_IK.L': (0.008, 0.005, 0.01)}),
    spec(24),
]

hook = [
    spec(1),
    spec(5, move={'CTRL_Hand_IK.L': (0.28, 0.12, 0.01),
                  'CTRL_Hand_IK.R': (-0.025, 0.015, 0.015),
                  'CTRL_Elbow_Pole.L': (0.15, 0, 0.25)},
         rotate={'CTRL_Chest': (0, 0, 0.09)}),
    spec(8, move={'CTRL_Hand_IK.L': (0.47, -0.33, 0.06),
                  'CTRL_Hand_IK.R': (0.015, -0.055, 0.045),
                  'CTRL_Elbow_Pole.L': (0.32, -0.12, 0.55)},
         rotate={'CTRL_Chest': (0, 0, -0.055)}),
    spec(11, move={'CTRL_Hand_IK.L': (-0.18, -0.72, 0.12),
                   'CTRL_Hand_IK.R': (0.055, -0.10, 0.065),
                   'CTRL_Elbow_Pole.L': (0.25, -0.36, 0.70)},
         rotate={'CTRL_Chest': (0, 0, -0.23), 'CTRL_COG': (0, 0, -0.06)}),
    spec(12, move={'CTRL_Hand_IK.L': (-0.29, -0.76, 0.13),
                   'CTRL_Hand_IK.R': (0.060, -0.105, 0.07),
                   'CTRL_Elbow_Pole.L': (0.23, -0.39, 0.70)},
         rotate={'CTRL_Chest': (0, 0, -0.24), 'CTRL_COG': (0, 0, -0.06)}),
    spec(16, move={'CTRL_Hand_IK.L': (-0.09, -0.35, 0.08),
                   'CTRL_Hand_IK.R': (0.025, -0.05, 0.035)},
         rotate={'CTRL_Chest': (0, 0, -0.10)}),
    spec(23, move={'CTRL_Hand_IK.L': (0, 0.02, 0),
                   'CTRL_Hand_IK.R': (-0.008, 0.008, 0.005)}),
    spec(28),
]

uppercut = [
    spec(1),
    spec(5, move={'CTRL_Hand_IK.R': (-0.10, 0.08, -0.34),
                  'CTRL_Hand_IK.L': (-0.025, 0.02, 0.01),
                  'CTRL_Elbow_Pole.R': (-0.12, 0.02, -0.1)},
         rotate={'CTRL_Chest': (0.04, 0, -0.04)}),
    spec(8, move={'CTRL_Hand_IK.R': (0.06, -0.29, -0.16),
                  'CTRL_Hand_IK.L': (0.015, -0.045, 0.03)},
         rotate={'CTRL_Chest': (-0.02, 0, 0.055)}),
    spec(12, move={'CTRL_Hand_IK.R': (0.28, -0.72, 0.52),
                   'CTRL_Hand_IK.L': (0.05, -0.085, 0.06),
                   'CTRL_Elbow_Pole.R': (-0.12, -0.18, 0.28)},
         rotate={'CTRL_Chest': (-0.13, 0, 0.14), 'CTRL_COG': (-0.035, 0, 0.04)}),
    spec(13, move={'CTRL_Hand_IK.R': (0.29, -0.77, 0.61),
                   'CTRL_Hand_IK.L': (0.055, -0.09, 0.065),
                   'CTRL_Elbow_Pole.R': (-0.12, -0.20, 0.30)},
         rotate={'CTRL_Chest': (-0.15, 0, 0.15), 'CTRL_COG': (-0.035, 0, 0.04)}),
    spec(18, move={'CTRL_Hand_IK.R': (0.10, -0.32, 0.24),
                   'CTRL_Hand_IK.L': (0.025, -0.04, 0.03)},
         rotate={'CTRL_Chest': (-0.04, 0, 0.07)}),
    spec(25, move={'CTRL_Hand_IK.R': (0, 0.02, 0.02),
                   'CTRL_Hand_IK.L': (0.005, 0.005, 0.005)}),
    spec(30),
]

write('Kevin_Melee_Guard', 'Boxing guard, seamless 48-frame breathing loop', guard)
write('Kevin_Melee_Jab_L', 'Fast straight left from boxing guard', jab,
      fp_jab=True)
write('Kevin_Melee_Cross_R', 'Right straight with shoulder and hip rotation', cross)
write('Kevin_Melee_Hook_L', 'Left hook with lifted elbow and lateral arc', hook)
write('Kevin_Melee_Uppercut_R', 'Right rising uppercut', uppercut)

activation = bpy.data.actions['Kevin_Melee_Guard']
activate(activation)
scene.frame_set(1)
scene.frame_start = 1
scene.frame_end = 49
scene.render.fps = 30
bpy.context.preferences.filepaths.save_version = 0
bpy.ops.wm.save_as_mainfile(filepath=bpy.data.filepath, compress=True,
                            check_existing=False)
print('MELEE_SAVED', bpy.data.filepath)
