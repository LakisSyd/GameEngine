# Skybox assets

The engine now looks for skyboxes in this order:

1. `assets/texture/cubemap/day.dds`
2. `assets/texture/cubemap/day/sky_px.jpg`, `sky_nx.jpg`, `sky_py.jpg`, `sky_ny.jpg`, `sky_pz.jpg`, `sky_nz.jpg`
3. `assets/texture/cubemap/evening.dds`
4. `assets/texture/cubemap/evening/sky_px.jpg`, `sky_nx.jpg`, `sky_py.jpg`, `sky_ny.jpg`, `sky_pz.jpg`, `sky_nz.jpg`
5. `assets/texture/cubemap/night.dds`
6. `assets/texture/cubemap/night/sky_px.jpg`, `sky_nx.jpg`, `sky_py.jpg`, `sky_ny.jpg`, `sky_pz.jpg`, `sky_nz.jpg`
7. Legacy fallback: `assets/texture/cubemap/sky_cube1.dds`

## Recommended free sources

All of these options are suitable for commercial and non-commercial projects:

| Time of day | Recommended asset | Source | License |
| --- | --- | --- | --- |
| Day | [`Kloofendal 43d Clear (Pure Sky)`](https://polyhaven.com/a/kloofendal_43d_clear_puresky) or [`Syferfontein 18d Clear (Pure Sky)`](https://polyhaven.com/a/syferfontein_18d_clear_puresky) | Poly Haven | CC0 |
| Evening | [`Evening Road 01 (Pure Sky)`](https://polyhaven.com/a/evening_road_01_puresky) or [`Rosendal Park Sunset (Pure Sky)`](https://polyhaven.com/a/rosendal_park_sunset_puresky) | Poly Haven | CC0 |
| Night | [`Rogland Clear Night`](https://polyhaven.com/a/rogland_clear_night) or [`Solitude Night`](https://polyhaven.com/a/solitude_night) | Poly Haven | CC0 |
| Ready-made PNG cubemap pack | [`Cloudy Skyboxes`](https://opengameart.org/content/cloudy-skyboxes-0) by Screaming Brain Studios | OpenGameArt | CC0 |

Poly Haven HDRIs are usually distributed as `.hdr`/`.exr` equirectangular panoramas. Convert them to a DirectX cube `.dds` before placing them in `assets/texture/cubemap/`.

OpenGameArt `Cloudy Skyboxes` includes horizontal-cross PNG cubemaps. Split a chosen cross into six faces and name them with the `_px/_nx/_py/_ny/_pz/_nz` suffixes above, or convert the cross into a `.dds` texture cube.

## Practical recommendation

For this DirectX 11 renderer, use DDS cubemaps when possible:

```text
assets/texture/cubemap/day.dds
assets/texture/cubemap/evening.dds
assets/texture/cubemap/night.dds
```

If you want quick iteration without DDS conversion, use six face images:

```text
assets/texture/cubemap/day/sky_px.jpg
assets/texture/cubemap/day/sky_nx.jpg
assets/texture/cubemap/day/sky_py.jpg
assets/texture/cubemap/day/sky_ny.jpg
assets/texture/cubemap/day/sky_pz.jpg
assets/texture/cubemap/day/sky_nz.jpg
```

Repeat the same layout for `evening` and `night`.
