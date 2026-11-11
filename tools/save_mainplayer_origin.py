import bpy
from pathlib import Path


ACTION_NAME = "Pistol_AIM"
BONE_PATH = 'pose.bones["mixamorig:Hips"].location'
OUTPUT = Path(r"C:\Users\Lakis\Desktop\MainPlayer.blend")


def iter_action_fcurves(action):
    for layer in getattr(action, "layers", []):
        for strip in getattr(layer, "strips", []):
            channelbag = getattr(strip, "channelbag", None)
            if callable(channelbag):
                for slot in getattr(action, "slots", []):
                    try:
                        bag = channelbag(slot)
                    except Exception:
                        bag = None
                    if bag:
                        yield from getattr(bag, "fcurves", [])
            elif channelbag:
                yield from getattr(channelbag, "fcurves", [])

    if hasattr(action, "fcurves"):
        yield from action.fcurves


for object_name in ("Main", "Suit_Suit_0.003"):
    obj = bpy.data.objects.get(object_name)
    if obj:
        obj.location = (0.0, 0.0, 0.0)

action = bpy.data.actions.get(ACTION_NAME)
if not action:
    raise RuntimeError(f"Action not found: {ACTION_NAME}")

fixed = 0
for fc in iter_action_fcurves(action):
    if fc.data_path != BONE_PATH:
        continue

    axis_value = 0.0
    for key in fc.keyframe_points:
        key.co.y = axis_value
        key.handle_left.y = axis_value
        key.handle_right.y = axis_value
    fc.update()
    fixed += 1
    print(f"Fixed {fc.data_path}[{fc.array_index}] -> 0.0")

if fixed != 3:
    raise RuntimeError(f"Expected 3 location fcurves for {BONE_PATH}, fixed {fixed}")

bpy.ops.wm.save_as_mainfile(filepath=str(OUTPUT))
print(f"Saved final file: {OUTPUT}")
