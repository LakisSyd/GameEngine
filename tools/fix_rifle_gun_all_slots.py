import bpy
from pathlib import Path


ACTION_NAME = "Rifle_Gun"
BONE_NAME = "mixamorig:Hips"
BONE_PATH = f'pose.bones["{BONE_NAME}"].location'
OUTPUT = Path(r"C:\Users\Lakis\Desktop\MainPlayer.blend")


def iter_action_channelbags(action):
    seen = set()

    for layer in getattr(action, "layers", []):
        for strip in getattr(layer, "strips", []):
            bags = getattr(strip, "channelbags", None)
            if bags:
                for bag in bags:
                    ptr = bag.as_pointer()
                    if ptr not in seen:
                        seen.add(ptr)
                        yield bag

            channelbag = getattr(strip, "channelbag", None)
            if callable(channelbag):
                for slot in getattr(action, "slots", []):
                    try:
                        bag = channelbag(slot)
                    except Exception:
                        bag = None
                    if bag:
                        ptr = bag.as_pointer()
                        if ptr not in seen:
                            seen.add(ptr)
                            yield bag


def zero_fcurve(fc):
    for key in fc.keyframe_points:
        key.co.y = 0.0
        key.handle_left.y = 0.0
        key.handle_right.y = 0.0
    fc.update()


action = bpy.data.actions.get(ACTION_NAME)
if not action:
    raise RuntimeError(f"Action not found: {ACTION_NAME}")

changed = 0
for bag in iter_action_channelbags(action):
    for fc in list(getattr(bag, "fcurves", [])):
        if fc.data_path == BONE_PATH:
            zero_fcurve(fc)
            changed += 1
            bag_name = getattr(bag, "name", None) or getattr(bag, "slot_handle", None) or "<bag>"
            print(f"Zeroed bag={bag_name} {fc.data_path}[{fc.array_index}]")

if hasattr(action, "fcurves"):
    for fc in action.fcurves:
        if fc.data_path == BONE_PATH:
            zero_fcurve(fc)
            changed += 1
            print(f"Zeroed legacy {fc.data_path}[{fc.array_index}]")

arm = bpy.data.objects.get("Main")
if not arm:
    raise RuntimeError("Armature object Main not found")

arm.location = (0.0, 0.0, 0.0)
arm.animation_data_create()
arm.animation_data.action = action

mesh = bpy.data.objects.get("Suit_Suit_0.003")
if mesh:
    mesh.location = (0.0, 0.0, 0.0)

start, end = action.curve_frame_range
start = int(start)
end = int(end)
if end < start:
    start, end = 1, 60

# Force-key the currently active slot too. This catches Blender 5.x cases where
# an action slot is not visible through the older fcurve API shape.
for frame in range(start, end + 1):
    bpy.context.scene.frame_set(frame)
    bone = arm.pose.bones.get(BONE_NAME)
    if not bone:
        raise RuntimeError(f"Bone not found: {BONE_NAME}")
    bone.location = (0.0, 0.0, 0.0)
    bone.keyframe_insert(data_path="location", frame=frame)

print(f"Zeroed fcurves: {changed}, baked frames: {start}..{end}")

bpy.ops.wm.save_as_mainfile(filepath=str(OUTPUT))
print(f"Saved: {OUTPUT}")
