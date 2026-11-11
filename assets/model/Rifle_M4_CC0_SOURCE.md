# Rifle_M4_CC0.glb

The mesh is adapted from the M4 in Tabasco's **Various Small Arms** pack:
https://opengameart.org/content/various-small-arms-assault-rifles-sniper-pistol

The author marked the pack CC0/public domain on 2023-12-15. The game asset
retains the original geometry and material groups. `tools/build_rifle_m4_cc0.py`
only changes its scale, axis, origin and PBR colors. The upstream archive is
not included in this repository.

To rebuild, download `smallarms.zip` from that page, extract `m4.blend`, and
run Blender with:

```text
blender --background --python tools/build_rifle_m4_cc0.py -- --source <path-to-m4.blend>
```

The model is 0.900 m long. Its origin is at the right palm's pistol grip.
In Blender space, the barrel points along +Y and +Z is up. The suggested
left palm support point is `(0, 0.33, 0.035)` m and the muzzle point is
`(0, 0.607, 0.11)` m. The butt extends to Y = -0.293 m.

With the project's glTF import through Assimp `ConvertToLeftHanded`, the
equivalent engine model-space sockets are:

| Socket | X | Y (up) | Z (forward) |
| --- | ---: | ---: | ---: |
| Right palm | 0 | 0 | 0 |
| Left palm contact on fore-end | 0 | 0.035 | 0.33 |
| Approximate left wrist IK target | -0.025 | 0.00 | 0.22 |
| Muzzle | 0 | 0.11 | 0.607 |

`WeaponIKSystem` targets the left wrist bone, so its `left_hand_grip` value
should use the wrist row as a starting point, not the palm contact row. The
current `Kevin_Rifle_Idle` clip bakes the support-hand pose, so left-hand IK
should be disabled until its wrist rotation is calibrated. The final grip
transform depends on Kevin's right-hand pose and should be checked in Play
Mode before saving the profile.
