import bpy

for obj in bpy.data.objects:
    print(
        obj.name,
        obj.type,
        "parent", obj.parent.name if obj.parent else "-",
        "parent_type", obj.parent_type,
        "children", [child.name for child in obj.children],
    )

mesh_obj = next(obj for obj in bpy.data.objects if obj.type == "MESH")
for modifier in mesh_obj.modifiers:
    print(
        "MODIFIER",
        modifier.name,
        modifier.type,
        "object", getattr(getattr(modifier, "object", None), "name", None),
    )
