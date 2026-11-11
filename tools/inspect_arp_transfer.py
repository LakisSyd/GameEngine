import bpy

print("=== FILE ===", bpy.data.filepath)
print("=== OBJECTS ===")
for obj in bpy.data.objects:
    if obj.type not in {"ARMATURE", "MESH"}:
        continue
    modifiers = [
        (modifier.name, modifier.type, getattr(modifier.object, "name", ""))
        for modifier in obj.modifiers
    ]
    action = (
        obj.animation_data.action.name
        if obj.animation_data and obj.animation_data.action
        else "NONE"
    )
    print(
        obj.name,
        "type=", obj.type,
        "parent=", obj.parent.name if obj.parent else "NONE",
        "action=", action,
        "scale=", tuple(round(v, 6) for v in obj.scale),
        "modifiers=", modifiers,
    )
    if obj.type == "ARMATURE":
        names = [bone.name for bone in obj.data.bones]
        print("  BONES", len(names), names[:20])
        print("  MIXAMO", sum(name.startswith("mixamorig:") for name in names))
        print("  ARP_CONTROLS", sum(name.startswith("c_") for name in names))

print("=== ACTIONS ===")
for action in bpy.data.actions:
    slots = [slot.identifier for slot in getattr(action, "slots", [])]
    print(
        action.name,
        "frames=", tuple(round(v, 3) for v in action.frame_range),
        "users=", action.users,
        "slots=", slots,
    )

print("=== SCENE ARP PROPS ===")
for key in bpy.context.scene.keys():
    if "arp" in key.lower() or "remap" in key.lower():
        print(key, bpy.context.scene[key])
