import os
import sys

import bpy


def main():
    if "--" not in sys.argv:
        raise RuntimeError("Usage: blender --background input.blend --python script.py -- output.glb")

    output = os.path.abspath(sys.argv[sys.argv.index("--") + 1])
    bpy.ops.object.select_all(action="DESELECT")

    selected = []
    for obj in bpy.context.scene.objects:
        if obj.type in {"MESH", "ARMATURE"}:
            obj.select_set(True)
            selected.append(obj.name)
        if obj.type == "ARMATURE" and obj.pose:
            for pose_bone in obj.pose.bones:
                pose_bone.custom_shape = None

    scene_objects = set(bpy.context.scene.objects)
    for obj in list(bpy.data.objects):
        if obj not in scene_objects:
            bpy.data.objects.remove(obj, do_unlink=True)

    if not selected:
        raise RuntimeError("No mesh or armature objects found")

    kwargs = {
        "filepath": output,
        "export_format": "GLB",
        "use_selection": True,
        "export_apply": False,
        "export_animations": True,
        "export_frame_range": False,
        "export_force_sampling": False,
        "export_yup": True,
        "export_rest_position_armature": False,
    }

    property_names = {
        prop.identifier for prop in bpy.ops.export_scene.gltf.get_rna_type().properties
    }
    if "export_optimize_animation_size" in property_names:
        kwargs["export_optimize_animation_size"] = True
    if "export_optimize_animation_keep_anim_armature" in property_names:
        kwargs["export_optimize_animation_keep_anim_armature"] = True

    os.makedirs(os.path.dirname(output), exist_ok=True)
    bpy.ops.export_scene.gltf(**kwargs)
    print("[first-person-export] objects:", ", ".join(selected))
    print("[first-person-export] output:", output)


main()
