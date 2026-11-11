"""Rebuild guarded locomotion from Kevin's authored walk; save a separate master."""
import math
from pathlib import Path
import bpy
from mathutils import Vector

OUT = Path(r'C:\Users\karin\OneDrive\Desktop\Kevin_Main_Switchable_GuardWalk_v2.blend')
QA = Path(r'C:\Users\karin\OneDrive\Desktop\DX_FrameWork\DX_FrameWork\tools\qa_guard_walk_v2')
QA.mkdir(exist_ok=True)
rig = bpy.data.objects['Armature']
scene = bpy.context.scene
anim = rig.animation_data_create()
for track in anim.nla_tracks:
    track.mute = True
initial = {b.name: b.matrix_basis.copy() for b in rig.pose.bones}

def activate(action, frame):
    anim.action = None
    for b in rig.pose.bones:
        b.matrix_basis = initial[b.name]
    anim.action = action
    anim.action_slot = action.slots[0]
    scene.frame_set(math.floor(frame), subframe=frame % 1)
    bpy.context.view_layer.update()

def capture():
    return {b.name: b.matrix_basis.copy() for b in rig.pose.bones}

old = bpy.data.actions['Kevin_Melee_Guard_Walk']
walk = bpy.data.actions['Kevin_Walk']
guard = bpy.data.actions['Kevin_Melee_Guard']
activate(guard, 1)
guard_pose = capture()
chest = rig.pose.bones['CTRL_Chest'].matrix.copy()
upper_targets = ['CTRL_Hand_IK.L', 'CTRL_Hand_IK.R',
                 'CTRL_Elbow_Pole.L', 'CTRL_Elbow_Pole.R']
relative_targets = {n: chest.inverted() @ rig.pose.bones[n].matrix
                    for n in upper_targets}
walk_controls = ['CTRL_Root', 'CTRL_COG', 'CTRL_Spine', 'CTRL_Spine_01',
                 'CTRL_Chest', 'CTRL_Neck', 'CTRL_Head']
walk_controls += [b.name for b in rig.pose.bones if b.name.startswith(
    ('CTRL_Thigh_', 'CTRL_Shin_', 'CTRL_Foot_', 'CTRL_Knee_', 'CTRL_Toe.'))]
samples = []
for frame in range(1, 62):
    # Use one complete original gait and make the wrap exact.
    source_frame = 1 + (frame - 1) * 60 / 61 if frame < 61 else 1
    activate(walk, source_frame)
    walk_pose = capture()
    anim.action = None
    for b in rig.pose.bones:
        b.matrix_basis = guard_pose[b.name]
    for name in walk_controls:
        rig.pose.bones[name].matrix_basis = walk_pose[name]
    bpy.context.view_layer.update()
    new_chest = rig.pose.bones['CTRL_Chest'].matrix.copy()
    for name in upper_targets:
        rig.pose.bones[name].matrix = new_chest @ relative_targets[name]
        bpy.context.view_layer.update()
    # Match FK controls to the evaluated guard so switching stays editable.
    for side, word in [('L', 'Left'), ('R', 'Right')]:
        for control, deform in [('UpperArm', 'Arm'), ('Forearm', 'ForeArm'), ('Hand', 'Hand')]:
            rig.pose.bones[f'CTRL_{control}_FK.{side}'].matrix = \
                rig.pose.bones[f'mixamorig:{word}{deform}'].matrix.copy()
            bpy.context.view_layer.update()
    samples.append((frame, capture()))

# Close the authored source's imperfect end pose over the last eight frames,
# rather than snapping the feet to the first pose on the final frame.
for index in range(53, 61):
    t = (index - 52) / 8
    blend = t * t * (3 - 2 * t)
    frame, pose = samples[index]
    for name in pose:
        pose[name] = pose[name].lerp(samples[0][1][name], blend)

old.name = 'Kevin_Melee_Guard_Walk_Previous'
old.use_fake_user = True
action = bpy.data.actions.new('Kevin_Melee_Guard_Walk')
action.use_fake_user = True
action['Description'] = 'Natural walking legs and weight transfer with chest-relative combat guard. 30 FPS looping in place.'
anim.action = action
for frame, pose in samples:
    scene.frame_set(frame)
    for name, basis in pose.items():
        b = rig.pose.bones[name]
        b.matrix_basis = basis
        for path in ['location', 'rotation_quaternion' if b.rotation_mode == 'QUATERNION' else 'rotation_euler', 'scale']:
            b.keyframe_insert(data_path=path, frame=frame, group=name)
anim.action_slot = action.slots[0]
scene.frame_start, scene.frame_end = 1, 61
scene.render.fps = 30
scene.frame_set(1)
bpy.ops.wm.save_as_mainfile(filepath=str(OUT), compress=True)
print('SAVED', OUT)

# Compare evaluated deformation at the loop boundary.
activate(action, 1)
first = {b.name: b.matrix.copy() for b in rig.pose.bones if b.name.startswith('mixamorig:')}
activate(action, 61)
seam = max(abs(x-y) for n, m in first.items()
           for a,b in zip(m,rig.pose.bones[n].matrix) for x,y in zip(a,b))
assert seam < 0.01, seam
print('LOOP_SEAM', seam)

scene.render.engine = 'BLENDER_WORKBENCH'
scene.render.resolution_x = 640
scene.render.resolution_y = 640
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = 'PNG'
scene.display.shading.light = 'STUDIO'
scene.display.shading.color_type = 'SINGLE'
scene.display.shading.single_color = (0.65, 0.69, 0.75)
scene.display.shading.show_cavity = True
mesh = bpy.data.objects['MaleBaseMesh LowPoly']
for obj in bpy.data.objects:
    if obj.type in {'MESH', 'ARMATURE'}:
        obj.hide_render = obj != mesh
data = bpy.data.cameras.new('GuardWalkQA')
cam = bpy.data.objects.new('GuardWalkQA', data)
scene.collection.objects.link(cam)
scene.camera = cam
data.type = 'ORTHO'
data.ortho_scale = 7.3
for label, clip in [('before', old), ('after', action)]:
    for frame in [1, 16, 31, 46]:
        activate(clip, frame)
        for view, position in [('front', (0,-13,5)), ('quarter', (9,-12,5.8))]:
            cam.location = position
            cam.rotation_euler = (Vector((0,0,3.15))-cam.location).to_track_quat('-Z','Y').to_euler()
            scene.render.filepath = str(QA/f'{label}_{frame}_{view}.png')
            bpy.ops.render.render(write_still=True)
print('QA', QA)
activate(action, 1)
cam.location = (9,-12,5.8)
cam.rotation_euler = (Vector((0,0,3.15))-cam.location).to_track_quat('-Z','Y').to_euler()
for frame in range(1,61,2):
    scene.frame_set(frame)
    scene.render.filepath = str(QA/f'preview_{(frame-1)//2:03d}.png')
    bpy.ops.render.render(write_still=True)
