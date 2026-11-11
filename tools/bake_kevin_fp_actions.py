"""Запекает Action из FP-копии мастер-рига в 65 игровых костей.

Откройте Kevin_Combat_FP_Controls.blend в Steam Blender и запустите скрипт.
Результат сохраняется в основной игровой Kevin_Combat_FP.blend/.glb.
Авторский файл с контроллерами не изменяется.
"""

import argparse
from pathlib import Path
import sys

import bpy
from mathutils import Matrix, Vector


ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument("--output-blend", type=Path,
                    default=ROOT / "assets/model/Kevin_Combat_FP.blend")
parser.add_argument("--output-glb", type=Path,
                    default=ROOT / "assets/model/Kevin_Combat_FP.glb")
options = parser.parse_args(sys.argv[sys.argv.index("--") + 1:]
                            if "--" in sys.argv else [])
OUTPUT_BLEND = options.output_blend.resolve()
OUTPUT_GLB = options.output_glb.resolve()

rig = bpy.data.objects.get("Armature")
mesh = bpy.data.objects.get("MaleBaseMesh LowPoly")
if not rig or not mesh or not rig.data.bones.get("CTRL_Hand_IK.L"):
    raise RuntimeError("Откройте Kevin_Combat_FP_Controls.blend")
if "FP_Source_Body_Bounds_Min" not in rig or "FP_Source_Body_Bounds_Max" not in rig:
    raise RuntimeError("Нет габаритов полного тела; пересоздайте авторский FP-файл")
if mesh.parent != rig:
    raise RuntimeError("FP-сетка потеряла привязку к исходному ригу")

scene = bpy.context.scene
animation = rig.animation_data_create()
for track in animation.nla_tracks:
    track.mute = True
initial_basis = {bone.name: bone.matrix_basis.copy() for bone in rig.pose.bones}

base_actions = (
    "Kevin_Idle", "Kevin_Walk", "Kevin_Run", "Kevin_Walk_R_I",
    "Kevin_Aim_Rifle", "Kevin_Aim_Shoot_R", "Kevin_Punch.1",
    "Kevin_Idle_Gun_R_SoftSway",
)
missing = [name for name in base_actions[:7] if name not in bpy.data.actions]
if missing:
    raise RuntimeError(f"Отсутствуют игровые Action: {missing}")
names = [name for name in base_actions if name in bpy.data.actions]
names += sorted(action.name for action in bpy.data.actions
                if action.name.startswith(("FP_", "Kevin_Melee_"))
                and action.name not in names)

# Сэмплируем исходный риг вместе с его родными контроллерами и драйверами.
deform = [bone.name for bone in rig.data.bones
          if bone.use_deform and bone.name.startswith("mixamorig:")]
if len(deform) != 65:
    raise RuntimeError(f"Ожидалось 65 игровых костей, найдено {len(deform)}")


def depth(name):
    value = 0
    bone = rig.data.bones[name]
    while bone.parent:
        value += 1
        bone = bone.parent
    return value


deform.sort(key=depth)


def set_action(name):
    animation.action = None
    for pose_bone in rig.pose.bones:
        pose_bone.matrix_basis = initial_basis[pose_bone.name]
    action = bpy.data.actions[name]
    animation.action = action
    slots = list(action.slots)
    if slots:
        animation.action_slot = next(
            (slot for slot in slots if slot.target_id_type == "OBJECT"),
            slots[0])
    return action


left_control = rig.pose.bones["CTRL_Hand_IK.L"]
left_follow = [constraint for constraint in left_control.constraints
               if constraint.type == "CHILD_OF" and constraint.target == rig
               and constraint.subtarget == "mixamorig:RightHand"]
saved_follow = [(constraint, constraint.mute,
                 constraint.inverse_matrix.copy()) for constraint in left_follow]
independent_actions = {
    "Kevin_Idle", "Kevin_Walk", "Kevin_Run", "Kevin_Walk_R_I",
    "Kevin_Punch.1",
}
independent_actions.update(name for name in names
                           if name.startswith(("FP_", "Kevin_Melee_")))
samples = {}

for name in names:
    action = set_action(name)
    start, end = (round(frame) for frame in action.frame_range)
    if end < start or end - start > 1000:
        raise RuntimeError(f"Некорректная длина Action: {name}")
    independent = name in independent_actions

    # Это та же поправка на Child Of, что используется в полном игровом
    # экспорте: старые безоружные клипы не должны сцеплять обе руки.
    if name in {"Kevin_Aim_Rifle", "Kevin_Aim_Shoot_R"} and left_follow:
        for constraint in left_follow:
            constraint.mute = True
        scene.frame_set(start)
        bpy.context.view_layer.update()
        inverse = rig.pose.bones["mixamorig:RightHand"].matrix.inverted()
        for constraint in left_follow:
            constraint.inverse_matrix = inverse
            constraint.mute = False
    else:
        for constraint, _, inverse in saved_follow:
            constraint.inverse_matrix = inverse

    frames = {}
    for frame in range(start, end + 1):
        scene.frame_set(frame)
        for constraint in left_follow:
            constraint.mute = independent
        bpy.context.view_layer.update()
        depsgraph = bpy.context.evaluated_depsgraph_get()
        evaluated = rig.evaluated_get(depsgraph)
        frames[frame] = {
            bone: rig.matrix_world @ evaluated.pose.bones[bone].matrix
            for bone in deform
        }
    samples[name] = frames
    print("FP_SAMPLE", name, start, end)

for constraint, mute, inverse in saved_follow:
    constraint.mute = mute
    constraint.inverse_matrix = inverse

# Нормализация совпадает с игровым Kevin_Combat_Game. Высота и центр
# сохранены до обрезки тела, поэтому руки не увеличиваются до роста персонажа.
minimum = Vector(rig["FP_Source_Body_Bounds_Min"])
maximum = Vector(rig["FP_Source_Body_Bounds_Max"])
height = maximum.z - minimum.z
if height <= 0.01:
    raise RuntimeError("Некорректная высота исходного персонажа")
center = Vector(((maximum.x + minimum.x) / 2,
                 (maximum.y + minimum.y) / 2, minimum.z))
normal = Matrix.Scale(2.2 / height, 4) @ Matrix.Translation(-center)
for frames in samples.values():
    for poses in frames.values():
        for bone, matrix in poses.items():
            location, rotation, _ = (normal @ matrix).decompose()
            poses[bone] = Matrix.LocRotScale(location, rotation,
                                              Vector((1, 1, 1)))

if bpy.context.object and bpy.context.object.mode != "OBJECT":
    bpy.ops.object.mode_set(mode="OBJECT")
rig.data.pose_position = "REST"
bpy.context.view_layer.update()

game_data = bpy.data.armatures.new("Kevin_FP_Game_Skeleton")
game = bpy.data.objects.new("Kevin_Game", game_data)
scene.collection.objects.link(game)
bpy.ops.object.select_all(action="DESELECT")
game.select_set(True)
bpy.context.view_layer.objects.active = game
bpy.ops.object.mode_set(mode="EDIT")

for name in deform:
    source = rig.data.bones[name]
    bone = game_data.edit_bones.new(name)
    bone.head = normal @ rig.matrix_world @ source.head_local
    bone.tail = normal @ rig.matrix_world @ source.tail_local
    _, rotation, _ = (normal @ rig.matrix_world @ source.matrix_local).decompose()
    bone.align_roll(rotation @ Vector((0, 0, 1)))
    parent = source.parent
    while parent and parent.name not in deform:
        parent = parent.parent
    if parent:
        bone.parent = game_data.edit_bones[parent.name]

bpy.ops.object.mode_set(mode="OBJECT")
if game_data.bones["mixamorig:RightHand"].parent.name != "mixamorig:RightForeArm":
    raise RuntimeError("Правая кисть потеряла связь с предплечьем")

game_mesh = bpy.data.objects.new("Kevin_FP_Arms", mesh.data.copy())
scene.collection.objects.link(game_mesh)
game_mesh.data.transform(normal @ mesh.matrix_world)
game_mesh.parent = game
modifier = game_mesh.modifiers.new("Skin", "ARMATURE")
modifier.object = game

game_animation = game.animation_data_create()
baked = []
for name, frames in samples.items():
    action = bpy.data.actions.new("__fp_baked__" + name)
    action.use_fake_user = True
    game_animation.action = action
    for frame, poses in frames.items():
        for bone in deform:
            pose_bone = game.pose.bones[bone]
            target = poses[bone]
            kwargs = {}
            if pose_bone.parent:
                kwargs = {
                    "parent_matrix": poses[pose_bone.parent.name],
                    "parent_matrix_local": pose_bone.parent.bone.matrix_local,
                }
            pose_bone.rotation_mode = "QUATERNION"
            pose_bone.matrix_basis = pose_bone.bone.convert_local_to_pose(
                target, pose_bone.bone.matrix_local, invert=True, **kwargs)
            for property_name in ("location", "rotation_quaternion", "scale"):
                pose_bone.keyframe_insert(property_name, frame=frame,
                                          group=bone)
    baked.append((name, action))
    print("FP_BAKE", name, len(frames), "frames")

# Только производные игровые объекты и запечённые Action переходят в GLB.
for obj in list(bpy.data.objects):
    if obj not in {game, game_mesh}:
        bpy.data.objects.remove(obj, do_unlink=True)
game_animation.action = None
for action in list(bpy.data.actions):
    if action not in [entry for _, entry in baked]:
        bpy.data.actions.remove(action)
for name, action in baked:
    action.name = name

# Сохраняем ту же центровку бега, что у основного игрового экспорта.
run = bpy.data.actions.get("Kevin_Run")
if run:
    game_animation.action = run
    if run.slots:
        game_animation.action_slot = run.slots[0]
    hips = game.pose.bones["mixamorig:Hips"]
    run_frames = range(round(run.frame_range[0]),
                       round(run.frame_range[1]) + 1)
    centers = []
    for frame in run_frames:
        scene.frame_set(frame)
        centers.append(hips.head.copy())
    average = Vector((sum(point.x for point in centers) / len(centers),
                      sum(point.y for point in centers) / len(centers), 0.0))
    local_delta = hips.bone.matrix_local.to_quaternion().inverted() @ (-average)
    for frame in run_frames:
        scene.frame_set(frame)
        hips.location += local_delta
        hips.keyframe_insert(data_path="location", frame=frame,
                             group=hips.name)

game_animation.action = bpy.data.actions.get("Kevin_Idle") or baked[0][1]
scene.frame_set(round(game_animation.action.frame_range[0]))
bpy.context.view_layer.update()
if len(game.data.bones) != 65 or len(game_mesh.data.polygons) < 5000:
    raise RuntimeError("Игровой FP-файл потерял кости или геометрию рук")

bpy.context.preferences.filepaths.save_version = 0
bpy.ops.wm.save_as_mainfile(filepath=str(OUTPUT_BLEND), compress=True,
                            check_existing=False)
bpy.ops.object.select_all(action="DESELECT")
game.select_set(True)
game_mesh.select_set(True)
bpy.context.view_layer.objects.active = game
bpy.ops.export_scene.gltf(
    filepath=str(OUTPUT_GLB), export_format="GLB", use_selection=True,
    export_animations=True, export_frame_range=False,
    export_force_sampling=True, export_animation_mode="ACTIONS",
    export_yup=True, export_rest_position_armature=True)
print("FP_EXPORTED", OUTPUT_BLEND, OUTPUT_GLB, len(baked), "actions")
