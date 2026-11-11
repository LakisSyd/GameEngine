import bpy

for prop in bpy.ops.export_scene.gltf.get_rna_type().properties:
    ident = prop.identifier
    if "export" in ident or "animation" in ident or "sampling" in ident:
        print(ident, getattr(prop, "default", None))
