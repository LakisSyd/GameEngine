import bpy


def fmt(values):
    return "(" + ", ".join(f"{v:.4f}" for v in values) + ")"


def obj_world_loc(obj):
    return obj.matrix_world.translation[:]


def bone_world_loc(armature, bone_name):
    bone = armature.pose.bones.get(bone_name)
    if not bone:
        return None
    return (armature.matrix_world @ bone.matrix).translation[:]


def print_action_details(action):
    print(f"Action {action.name}")
    print(f"  id_root={getattr(action, 'id_root', '<none>')}")
    print(f"  slots={len(getattr(action, 'slots', []))}")
    for slot in getattr(action, "slots", []):
        slot_name = (
            getattr(slot, "name", None)
            or getattr(slot, "identifier", None)
            or getattr(slot, "name_display", None)
            or "<unnamed>"
        )
        print(f"    slot={slot_name} target_id_type={getattr(slot, 'target_id_type', '<none>')}")
        try:
            bag = action.channelbag(slot)
        except Exception as exc:
            print(f"      channelbag error: {exc}")
            bag = None
        if bag:
            print(f"      fcurves={len(getattr(bag, 'fcurves', []))}")
            for fc in getattr(bag, "fcurves", []):
                print(f"        {fc.data_path}[{fc.array_index}] keys={len(fc.keyframe_points)}")

    print(f"  layers={len(getattr(action, 'layers', []))}")
    for layer in getattr(action, "layers", []):
        print(f"    layer={layer.name} strips={len(getattr(layer, 'strips', []))}")
        for strip in getattr(layer, "strips", []):
            strip_name = (
                getattr(strip, "name", None)
                or getattr(strip, "identifier", None)
                or strip.__class__.__name__
            )
            print(f"      strip={strip_name} type={getattr(strip, 'type', '<none>')}")
            bag = None
            channelbag_func = getattr(strip, "channelbag", None)
            if callable(channelbag_func):
                for slot in getattr(action, "slots", []):
                    try:
                        bag = channelbag_func(slot)
                    except Exception as exc:
                        print(f"        channelbag({slot}) error={exc}")
                        bag = None
                    if bag:
                        print(f"        slot bag fcurves={len(getattr(bag, 'fcurves', []))}")
                        for fc in getattr(bag, "fcurves", []):
                            print(f"          {fc.data_path}[{fc.array_index}] keys={len(fc.keyframe_points)}")
                continue
            bag = getattr(strip, "channelbag", None)
            if not bag and hasattr(strip, "channelbags"):
                bags = getattr(strip, "channelbags", [])
                print(f"        channelbags={len(bags)}")
                for bag_item in bags:
                    print(f"        bag fcurves={len(getattr(bag_item, 'fcurves', []))}")
                    for fc in getattr(bag_item, "fcurves", []):
                        print(f"          {fc.data_path}[{fc.array_index}] keys={len(fc.keyframe_points)}")
                continue
            if bag:
                print(f"        fcurves={len(getattr(bag, 'fcurves', []))}")
                for fc in getattr(bag, "fcurves", []):
                    print(f"          {fc.data_path}[{fc.array_index}] keys={len(fc.keyframe_points)}")


print("ACTIONS")
for action in bpy.data.actions:
    print_action_details(action)

print("")
print("OBJECT ANIMATION DATA")
for obj in bpy.data.objects:
    anim = obj.animation_data
    if not anim:
        continue
    action = anim.action.name if anim.action else "<none>"
    print(f"{obj.name}: action={action}")
    for track in anim.nla_tracks:
        print(f"  nla track={track.name} mute={track.mute}")
        for strip in track.strips:
            print(f"    strip={strip.name} action={strip.action.name if strip.action else '<none>'} frame={strip.frame_start:.1f}..{strip.frame_end:.1f}")

print("")
print("WORLD MOTION SAMPLE")
objects = [obj for obj in bpy.data.objects if obj.type in {"ARMATURE", "MESH"}]
armatures = [obj for obj in bpy.data.objects if obj.type == "ARMATURE"]
root_bones = {}
for arm in armatures:
    candidates = [
        b.name for b in arm.pose.bones
        if b.parent is None or "hips" in b.name.lower() or "root" in b.name.lower()
    ]
    root_bones[arm.name] = candidates[:4]

for frame in [1, 5, 10, 15, 20, 25, 30, 40, 50, 60]:
    bpy.context.scene.frame_set(frame)
    print(f"Frame {frame}")
    for obj in objects:
        print(f"  {obj.name:28} world={fmt(obj_world_loc(obj))} local={fmt(obj.location)}")
        if obj.type == "ARMATURE":
            for bone_name in root_bones.get(obj.name, []):
                loc = bone_world_loc(obj, bone_name)
                print(f"    bone {bone_name:22} world={fmt(loc)} localLoc={fmt(obj.pose.bones[bone_name].location)}")
