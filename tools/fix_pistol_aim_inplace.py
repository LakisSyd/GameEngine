import bpy
from pathlib import Path


ACTION_NAME = "Pistol_AIM"
BONE_PATH = 'pose.bones["mixamorig:Hips"].location'
OUTPUT = Path(r"C:\Users\Lakis\Desktop\MainPlayer (2)_pistol_aim_inplace.blend")


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


action = bpy.data.actions.get(ACTION_NAME)
if not action:
    raise RuntimeError(f"Action not found: {ACTION_NAME}")

fixed = 0
for fc in iter_action_fcurves(action):
    if fc.data_path != BONE_PATH:
        continue
    if not fc.keyframe_points:
        continue

    base = fc.keyframe_points[0].co.y
    for key in fc.keyframe_points:
        key.co.y = base
        key.handle_left.y = base
        key.handle_right.y = base
    fc.update()
    fixed += 1
    print(f"Fixed {fc.data_path}[{fc.array_index}] -> constant {base:.6f}")

if fixed == 0:
    raise RuntimeError(f"No fcurves fixed for {BONE_PATH}")

bpy.ops.wm.save_as_mainfile(filepath=str(OUTPUT))
print(f"Saved fixed copy: {OUTPUT}")

