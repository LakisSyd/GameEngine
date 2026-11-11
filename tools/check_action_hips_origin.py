import bpy


ACTION_NAME = "Rifle_Gun"
BONE_PATH = 'pose.bones["mixamorig:Hips"].location'


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


action = bpy.data.actions[ACTION_NAME]
ok = True
for fc in iter_action_fcurves(action):
    if fc.data_path != BONE_PATH:
        continue
    values = [round(key.co.y, 6) for key in fc.keyframe_points]
    unique = sorted(set(values))
    print(f"{fc.data_path}[{fc.array_index}] values={unique[:8]} count={len(unique)}")
    if unique != [0.0]:
        ok = False

arm = bpy.data.objects["Main"]
arm.animation_data.action = action
for frame in [1, 5, 10, 20, 30, 45, 60]:
    bpy.context.scene.frame_set(frame)
    hip = arm.pose.bones["mixamorig:Hips"]
    print(f"frame={frame} hips_local={tuple(round(v, 6) for v in hip.location)}")
    if tuple(round(v, 6) for v in hip.location) != (0.0, 0.0, 0.0):
        ok = False

if not ok:
    raise RuntimeError("Hips location is not fully zero")

print("CHECK OK")
