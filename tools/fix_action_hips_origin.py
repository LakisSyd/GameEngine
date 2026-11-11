import bpy
import sys
from pathlib import Path


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


action_name = "Rifle_Gun"
bone_path = 'pose.bones["mixamorig:Hips"].location'
output_path = Path(r"C:\Users\Lakis\Desktop\MainPlayer.blend")

action = bpy.data.actions.get(action_name)
if not action:
    print("Available actions:")
    for action_item in bpy.data.actions:
        print(f"  {action_item.name}")
    raise RuntimeError(f"Action not found: {action_name}")

fixed = 0
for fc in iter_action_fcurves(action):
    if fc.data_path != bone_path:
        continue

    for key in fc.keyframe_points:
        key.co.y = 0.0
        key.handle_left.y = 0.0
        key.handle_right.y = 0.0
    fc.update()
    fixed += 1
    print(f"Fixed {action_name}: {fc.data_path}[{fc.array_index}] -> 0.0")

if fixed != 3:
    raise RuntimeError(f"Expected 3 hips location fcurves, fixed {fixed}")

for object_name in ("Main", "Suit_Suit_0.003"):
    obj = bpy.data.objects.get(object_name)
    if obj:
        obj.location = (0.0, 0.0, 0.0)

bpy.ops.wm.save_as_mainfile(filepath=str(output_path))
print(f"Saved: {output_path}")
