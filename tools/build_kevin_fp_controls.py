"""Создаёт FP-файл из Kevin_Main_Switchable.blend без перестройки рига.

Steam Blender:
blender --background C:/Users/karin/OneDrive/Desktop/Kevin_Main_Switchable.blend \
    --python tools/build_kevin_fp_controls.py

В выходном файле меняется только геометрия видимого тела: остаются руки.
Оригинальные контроллеры, ограничения, драйверы и Action сохраняются.
"""

from pathlib import Path

import bmesh
import bpy


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "assets/model/Kevin_Combat_FP_Controls.blend"
RIG_NAME = "Armature"
MESH_NAME = "MaleBaseMesh LowPoly"
WEIGHT_THRESHOLD = 0.10

rig = bpy.data.objects.get(RIG_NAME)
mesh = bpy.data.objects.get(MESH_NAME)
mode = bpy.data.objects.get("Kevin_Arm_Mode")
if not rig or rig.type != "ARMATURE" or not mesh or mesh.type != "MESH":
    raise RuntimeError("Откройте Kevin_Main_Switchable.blend с исходным ригом")
if not mode or mesh.parent != rig or not rig.data.bones.get("CTRL_Hand_IK.L"):
    raise RuntimeError("Файл не содержит исходные IK-контроллеры Kevin")

action_name = (rig.animation_data.action.name
               if rig.animation_data and rig.animation_data.action else None)
actions_before = sorted(action.name for action in bpy.data.actions)
control_count = sum(bone.name.startswith("CTRL_") for bone in rig.data.bones)
bone_count = len(rig.data.bones)

# Сохраняем габариты полного тела для будущего игрового экспорта: после
# обрезки высоту персонажа нельзя будет вычислить по одним рукам.
world_points = [mesh.matrix_world @ vertex.co for vertex in mesh.data.vertices]
lower = [min(point[i] for point in world_points) for i in range(3)]
upper = [max(point[i] for point in world_points) for i in range(3)]
rig["FP_Source_Body_Bounds_Min"] = lower
rig["FP_Source_Body_Bounds_Max"] = upper

group_names = {group.index: group.name for group in mesh.vertex_groups}


def is_arm_group(name):
    return name.startswith((
        "mixamorig:LeftArm", "mixamorig:RightArm",
        "mixamorig:LeftForeArm", "mixamorig:RightForeArm",
        "mixamorig:LeftHand", "mixamorig:RightHand",
    ))


arm_weights = [
    sum(item.weight for item in vertex.groups
        if is_arm_group(group_names.get(item.group, "")))
    for vertex in mesh.data.vertices
]
kept_faces = {
    face.index for face in mesh.data.polygons
    if all(arm_weights[index] >= WEIGHT_THRESHOLD for index in face.vertices)
}
if len(kept_faces) < 5000 or len(kept_faces) > 12000:
    raise RuntimeError(f"Неверный выбор геометрии рук: {len(kept_faces)} граней")

# BMesh сохраняет UV, материалы, группы вершин и веса. Ни одна кость или
# анимация здесь не удаляется и не перезаписывается.
if bpy.context.object and bpy.context.object.mode != "OBJECT":
    bpy.ops.object.mode_set(mode="OBJECT")
bm = bmesh.new()
bm.from_mesh(mesh.data)
bm.faces.ensure_lookup_table()
bm.faces.index_update()
bmesh.ops.delete(bm,
    geom=[face for face in bm.faces if face.index not in kept_faces],
    context="FACES")
bmesh.ops.delete(bm,
    geom=[vertex for vertex in bm.verts if not vertex.link_faces],
    context="VERTS")
bm.to_mesh(mesh.data)
bm.free()
mesh.data.update()

if len(mesh.data.vertices) < 10000 or len(mesh.data.polygons) < 5000:
    raise RuntimeError("После обрезки сетка рук стала неполной")

# Дублирующая полная сетка служит только справочным телом и не участвует
# в контроллерах. Сам Armature.001 оставляем: на него смотрят Retarget-ограничения.
reference_body = bpy.data.objects.get("MaleBaseMesh LowPoly.001")
if reference_body:
    bpy.data.objects.remove(reference_body, do_unlink=True)

# Скрываем остальные модели исходной сцены. Не удаляем их: исходные Child Of,
# драйверы и вспомогательные привязки продолжают работать точно как в мастере.
for obj in bpy.data.objects:
    if obj in {rig, mesh, mode} or obj.name.startswith("WGT_"):
        continue
    if obj.type in {"MESH", "ARMATURE"}:
        obj.hide_set(True)
        obj.hide_render = True

# В мастер-файле пользователь работает IK-контроллерами. FK-кости остаются
# внутри рига для совместимости, но не загромождают экран.
fk_collection = rig.data.collections.get("Kevin • FK")
if fk_collection:
    fk_collection.is_visible = False
ik_collection = rig.data.collections.get("Kevin • IK")
if ik_collection:
    ik_collection.is_visible = True
rig.show_in_front = True

if sorted(action.name for action in bpy.data.actions) != actions_before:
    raise RuntimeError("Список Action неожиданно изменился")
if len(rig.data.bones) != bone_count or sum(
    bone.name.startswith("CTRL_") for bone in rig.data.bones) != control_count:
    raise RuntimeError("Контроллеры исходного рига неожиданно изменились")
if action_name and rig.animation_data.action.name != action_name:
    raise RuntimeError("Активный Action неожиданно изменился")

bpy.ops.object.select_all(action="DESELECT")
rig.select_set(True)
bpy.context.view_layer.objects.active = rig
bpy.ops.object.mode_set(mode="POSE")
bpy.context.preferences.filepaths.save_version = 0
bpy.ops.wm.save_as_mainfile(filepath=str(OUTPUT), compress=True,
                            check_existing=False)
print("FP_AUTHORING_SAVED", OUTPUT)
print("FP_AUTHORING_GEOMETRY", len(mesh.data.vertices),
      len(mesh.data.polygons), "faces")
print("FP_AUTHORING_RIG", bone_count, "bones", control_count,
      "controls", len(actions_before), "actions", action_name)
