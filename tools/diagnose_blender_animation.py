import bpy
import math


def fmt_vec(values):
    return "(" + ", ".join(f"{v:.4f}" for v in values) + ")"


def curve_range(fc):
    values = [kp.co.y for kp in fc.keyframe_points]
    if not values:
        return None
    return min(values), max(values), max(values) - min(values)


def print_header(title):
    print("")
    print("=" * 72)
    print(title)
    print("=" * 72)


def iter_action_fcurves(action):
    if hasattr(action, "fcurves"):
        yield from action.fcurves
        return

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

    for slot in getattr(action, "slots", []):
        channelbag = action.channelbag(slot) if hasattr(action, "channelbag") else None
        if channelbag:
            yield from getattr(channelbag, "fcurves", [])


print_header("FILE")
print(bpy.data.filepath)

print_header("OBJECTS")
for obj in bpy.data.objects:
    if obj.type in {"ARMATURE", "MESH", "EMPTY"}:
        anim = obj.animation_data
        action = anim.action.name if anim and anim.action else "<none>"
        parent = obj.parent.name if obj.parent else "<none>"
        print(
            f"{obj.name:36} type={obj.type:8} parent={parent:28} "
            f"loc={fmt_vec(obj.location)} rot={fmt_vec(obj.rotation_euler)} "
            f"scale={fmt_vec(obj.scale)} action={action}"
        )
        if obj.constraints:
            for c in obj.constraints:
                target = c.target.name if getattr(c, "target", None) else "<none>"
                subtarget = getattr(c, "subtarget", "")
                print(f"  constraint {c.type:18} target={target} subtarget={subtarget}")

print_header("ACTIONS WITH LOCATION KEYS")
for action in bpy.data.actions:
    fcurves = list(iter_action_fcurves(action))
    location_curves = [
        fc for fc in fcurves
        if "location" in fc.data_path.lower()
    ]
    if not location_curves:
        continue

    print(f"Action: {action.name}")
    grouped = {}
    for fc in location_curves:
        grouped.setdefault(fc.data_path, []).append(fc)

    for data_path, curves in sorted(grouped.items()):
        ranges = []
        total_delta = 0.0
        for fc in sorted(curves, key=lambda c: c.array_index):
            r = curve_range(fc)
            if r is None:
                continue
            mn, mx, delta = r
            total_delta += abs(delta)
            ranges.append(f"{fc.array_index}: {mn:.4f}..{mx:.4f} d={delta:.4f}")
        suspicious = "  <-- BIG MOVE" if total_delta > 0.25 else ""
        print(f"  {data_path}{suspicious}")
        for r in ranges:
            print(f"    axis {r}")

print_header("ARMATURE POSE BONE CONSTRAINTS")
for obj in bpy.data.objects:
    if obj.type != "ARMATURE":
        continue
    print(f"Armature: {obj.name}")
    for bone in obj.pose.bones:
        if not bone.constraints:
            continue
        print(f"  Bone: {bone.name}")
        for c in bone.constraints:
            target = c.target.name if getattr(c, "target", None) else "<none>"
            subtarget = getattr(c, "subtarget", "")
            print(f"    constraint {c.type:18} target={target} subtarget={subtarget}")

print_header("LIKELY ROOT BONES")
for obj in bpy.data.objects:
    if obj.type != "ARMATURE":
        continue
    print(f"Armature: {obj.name}")
    for bone in obj.data.bones:
        name = bone.name.lower()
        if bone.parent is None or any(token in name for token in ["root", "master", "hips", "pelvis"]):
            parent = bone.parent.name if bone.parent else "<none>"
            print(f"  {bone.name:32} parent={parent}")
