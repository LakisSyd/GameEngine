"""Verify that Kevin_Walk_R_I survived the baked GLB export."""
import bpy
from pathlib import Path

root = Path(__file__).resolve().parents[2]
for filename in ("Kevin_Combat_Game.glb", "Kevin_Combat_FP.glb"):
    bpy.ops.import_scene.gltf(filepath=str(root / "assets/model" / filename))
    rig = next(obj for obj in bpy.data.objects if obj.type == "ARMATURE")
    body = next(obj for obj in bpy.data.objects
                if obj.type == "MESH" and len(obj.data.vertices) > 1000)
    action = bpy.data.actions.get("Kevin_Walk_R_I")

    assert len(rig.data.bones) == 65, len(rig.data.bones)
    assert body.parent == rig, "Kevin body is not bound to the exported rig"
    assert action is not None, f"Kevin_Walk_R_I is missing from {filename}"
    assert len(action.slots) == 1, len(action.slots)

    rig.animation_data.action = action
    rig.animation_data.action_slot = action.slots[0]
    bpy.context.scene.frame_set(round(action.frame_range[0]))
    bpy.context.view_layer.update()
    print("VERIFIED", filename, action.name, tuple(action.frame_range),
          "bones", len(rig.data.bones), "vertices", len(body.data.vertices))

    for obj in list(bpy.data.objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    for action in list(bpy.data.actions):
        bpy.data.actions.remove(action)
