import bpy

for action in bpy.data.actions:
    print("ACTION", action.name)
    print(" action attrs", [name for name in dir(action) if "curve" in name.lower() or "channel" in name.lower() or "slot" in name.lower()][:80])
    for layer in getattr(action, "layers", []):
        print(" LAYER", layer, [name for name in dir(layer) if "strip" in name.lower() or "channel" in name.lower()][:80])
        for strip in getattr(layer, "strips", []):
            print("  STRIP", strip, [name for name in dir(strip) if "curve" in name.lower() or "channel" in name.lower() or "slot" in name.lower()][:120])
            for name in dir(strip):
                if name.startswith("_"):
                    continue
                if "channel" in name.lower() or "curve" in name.lower():
                    try:
                        value = getattr(strip, name)
                    except Exception as exc:
                        value = f"<err {exc}>"
                    print("   ", name, type(value), value)
    print()

