# Полный индекс runtime исходников

Этот индекс нужен для проверки полноты. В нём перечислены собственные runtime-модули проекта и глава, где объясняется их код. Парные `.h/.cpp` считаются одной реализацией. Простые component-файлы читаются вместе с системой, которая владеет их полями.

## 1 Явно исключено

1. Весь `Editor` и editor-only методы UI, gizmo, selection и authoring.
2. `FloodingSystem.h/.cpp` как рудимент старой концепции подлодки.
3. Исходники сторонних библиотек: ImGui, ImGuizmo, stb, tinygltf header, Assimp, Bullet, Recast и Detour.
4. `tests` и `tools` не являются игровым runtime; инженерные главы объясняют принципы их использования.
5. HLSL-файлы не исключены: они разобраны в shader-главах как собственный код проекта.

## 2 Core и platform

| Файл или модуль | Где разобран |
|---|---|
| `main.cpp` | V12 Core runtime, раздел 1 |
| `SystemClass` | V12 Core runtime, разделы 2–7 |
| `TimeManager` | V12 Core runtime, раздел 8 |
| `GameTimer` | V12 Core runtime, раздел 9 |
| `PhysicsWorld` | V12 Core runtime, раздел 10 |
| `JobSystem` | V12 Core runtime, раздел 11 |
| `FileSystem` | V12 Core runtime, разделы 12–15 |
| `AssetsLoadSystem` | V12 Core runtime, разделы 16–20 |
| `WorldEvent` и `WorldEventLog` | V12 Core runtime, раздел 21 |
| `DebugHud` | V12 Core runtime, раздел 21 |
| `ApplicationClass` runtime | V12 Application ECS input scene, разделы 1–6 |
| `CameraController` | V12 Application ECS input scene, раздел 9 |
| `InputClass` | V12 Application ECS input scene, раздел 7 |
| `InputManager` | V12 Application ECS input scene, раздел 8 |
| `GameSettings` | V7 foundations и главы соответствующих settings consumers |
| `CollisionUtils` | V10 Bullet physics и V12 Physics geometry AI |
| `helper.h`, `FileName.h` | V12 Core runtime, вспомогательные преобразования и пути |

## 3 ECS

| Файл или модуль | Где разобран |
|---|---|
| `ECSTypes` | V7 foundations, сущность и INVALID sentinel |
| `IComponentPool` | V7 foundations и V12 Application ECS input scene, раздел 10 |
| `ComponentManager<T>` | V12 Application ECS input scene, раздел 11 |
| `ECSRegistry` | V12 Application ECS input scene, раздел 10 |
| `Components.h` | V7 foundations и главы систем-владельцев |
| `TransformSystem` | V12 Application ECS input scene, раздел 12 |
| `RenderSystem` | V12 Application ECS input scene, раздел 13 |
| `DoorSystem` | V12 Gameplay runtime, раздел 20 |
| `PhysicsSystem` | V12 Physics geometry AI, разделы 1–7 |
| `JointConnectionSystem` | V12 World effects runtime, раздел 12 |
| `SubmarineJointSystem` | V12 World effects runtime, раздел 11, историческое имя |
| `PressureSystem` | V12 World effects runtime, раздел 8 |
| `MeshCuttingTest` | V12 Physics geometry AI, разделы 9–17 |
| `RenderMesh` | V12 Render runtime, model и buffer contracts |

## 4 Render и camera

| Файл или модуль | Где разобран |
|---|---|
| `D3DClass` | V12 Render runtime, разделы 1–10 |
| `Renderer` | V12 Render runtime, раздел 45 |
| `CameraClass` | V8 shaders from zero, view/projection; V12 camera controller |
| `ViewPointClass` | V8 shaders from zero и V12 camera/controller path |
| `FrustumCulling` и `RenderCandidateSystem` | V12 Render runtime, раздел 42 |
| `TextureClass` | V12 Render runtime, раздел 11 |
| `RenderTextureClass` | V12 Render runtime, раздел 12 |
| `ShadowMap` | V12 Render runtime, раздел 13 |
| `BitmapClass` | V12 Render runtime, раздел 15 |
| `WorldSpriteClass` | V12 Render runtime, раздел 16 |
| `DWriteTextSystem` | V12 Render runtime, раздел 24 |
| `GrassRenderer` | V12 Render runtime, разделы 20–23 |
| `ParticleRenderer` | V12 Render runtime, раздел 25 |
| `MaterialClass` | V12 Render runtime, раздел 47 |
| `ModelClass` legacy | V12 Render runtime, раздел 17 |
| `ModelManager` | V12 Render runtime, раздел 18 |
| `TinyGltfModelClass` | V12 Render runtime, раздел 19, experimental placeholder |
| `AssimpModelClass` | V11 Assets animation studios и V12 Render runtime, раздел 48 |
| `ModelScaleUtils` | V11 import, нормализация масштаба |

## 5 Shader wrappers и HLSL

| Файл или модуль | Где разобран |
|---|---|
| `ShaderBaseClass` | V12 Render runtime, раздел 46 |
| `ShaderManager` | V12 Render runtime, раздел 41 |
| `ColorShaderClass` | V12 Render runtime, раздел 27 |
| `TextureShaderClass` | V12 Render runtime, раздел 28 |
| `LightShaderClass` | V12 Render runtime, раздел 29 |
| `PBRShaderClass` | V12 Render runtime, раздел 30 |
| `NormalMapShaderClass` | V12 Render runtime, раздел 31 |
| `LightMapShaderClass` | V12 Render runtime, раздел 32 |
| `MultiTextureShaderClass` | V12 Render runtime, раздел 32 |
| `FogShaderClass` | V12 Render runtime, раздел 33 |
| `ClipPlaneShaderClass` | V12 Render runtime, раздел 34 |
| `ReflectionShaderClass` | V12 Render runtime, раздел 35 |
| `WaterShaderClass` | V12 Render runtime, раздел 36; графический эффект, не FloodingSystem |
| `GlassShaderClass` | V12 Render runtime, раздел 37 |
| `ProjectionShaderClass` | V12 Render runtime, раздел 38 |
| `TerrainBlendShaderClass` | V12 Render runtime, раздел 39 |
| `SkyShaderClass` | V12 Render runtime, раздел 40 |
| `ShadowShaderClass` | V12 Render runtime, раздел 14 |
| Все собственные `.vs/.ps/.hlsl` | V8 shaders from zero и V11 shader pipeline, по стадиям и buffers |

## 6 Scene и assets

| Файл или модуль | Где разобран |
|---|---|
| `SceneManager` runtime ownership | V12 Application ECS input scene, разделы 14–15 |
| `GameObject` legacy | V12 Application ECS input scene, раздел 16 |
| `LightClass` | V8 shaders и V11 shader pipeline |
| `PickingSystem` math/runtime | V12 Application ECS input scene, раздел 17; editor selection исключён |
| Static asset async path | V12 Core runtime, `AssetsLoadSystem` |
| GLB/FBX import | V11 Assets animation studios |
| Embedded/external textures | V11 Assets animation studios и V12 Render runtime, раздел 48 |
| Scene JSON save/load | V12 Application ECS input scene, раздел 14 |

## 7 Animation

| Файл или модуль | Где разобран |
|---|---|
| `AnimationComponent` | V11 Assets animation studios, data ownership |
| `AnimationController` | V12 Application ECS input scene, раздел 18 |
| `AnimationSystem` | V12 Application ECS input scene, раздел 19 |
| `TwoBoneIK` | V12 Application ECS input scene, раздел 20 |
| `WeaponIKSystem` | V12 Application ECS input scene, раздел 21 |
| `MeleeActionJson` | V12 Application ECS input scene, раздел 22 |
| Bone loading/interpolation | V11 Assets animation studios и V12 Render runtime, раздел 48 |
| Animation Studio | V11 Assets animation studios, snapshot, profiles, events и live scene |

## 8 Player combat inventory interaction

| Файл или модуль | Где разобран |
|---|---|
| `GameplayController` | V12 Gameplay runtime, раздел 1 |
| `GameplayInputSystem` | V12 Gameplay runtime, раздел 2 |
| `CharacterSpawnSystem` | V12 Gameplay runtime, раздел 3 |
| `CharacterMovementSystem` | V12 Gameplay runtime, раздел 4 |
| `CharacterControllerSystem` | V12 Gameplay runtime, раздел 5 |
| `CharacterExternalVelocitySystem` | V12 Gameplay runtime, раздел 6 |
| `CharacterFacingSystem` | V12 Gameplay runtime, раздел 7 |
| `FirstPersonViewSystem` | V12 Gameplay runtime, раздел 8 |
| `PlayerVitalsSystem` | V12 Gameplay runtime, раздел 9 |
| `PlayerComponents` и physics tuning | Разобраны рядом с системами 3–9 |
| `WeaponComponent` | V12 Gameplay runtime, разделы 10–11 |
| `WeaponSystem` | V12 Gameplay runtime, раздел 10 |
| `WeaponFiringRules` | V12 Gameplay runtime, раздел 11 |
| `PlayerPunchSystem` | V12 Gameplay runtime, раздел 12 |
| `DamageEvent` и `DamageSystem` | V12 Gameplay runtime, раздел 13 |
| `InventoryComponent` и `InventorySystem` | V12 Gameplay runtime, раздел 14 |
| `WeaponPickupSystem` | V12 Gameplay runtime, раздел 15 |
| `EventSystem` | V12 Gameplay runtime, раздел 16 |
| `InteractionSystem` | V12 Gameplay runtime, раздел 17 |
| `TriggerZoneComponent/System` | V12 Gameplay runtime, раздел 18 |
| `TriggerActionSystem` | V12 Gameplay runtime, раздел 19 |

## 9 World power repair effects UI

| Файл или модуль | Где разобран |
|---|---|
| `PowerSystem` | V12 World effects runtime, раздел 1 |
| `GeneratorComponent/System` | V12 World effects runtime, раздел 2 |
| `EmergencyBatterySystem` | V12 World effects runtime, раздел 3 |
| `PowerGridComponent`, `PowerConsumerComponent`, `ShipPowerGridSystem` | V12 World effects runtime, раздел 4 |
| `OxygenGeneratorComponent/System` | V12 World effects runtime, раздел 5 |
| `RoomSystem` | V12 World effects runtime, раздел 6 |
| `RoomEnvironmentSystem` | V12 World effects runtime, раздел 7 |
| `DecompressionSourceComponent` | V12 World effects runtime, раздел 7 |
| `RoomCullingSystem` | V12 World effects runtime, раздел 9 |
| `ShipAlertSystem` | V12 World effects runtime, раздел 10 |
| `WorldAreaComponent` | V12 World effects runtime, room/world classification |
| `SubmarineSystem` | V12 World effects runtime, исторические transform/group helpers |
| `RepairComponent`, `RuntimeMeshDamageComponent`, `RepairSystem` | V12 World effects runtime, раздел 13 |
| `ParticleSystem` | V12 World effects runtime, раздел 14 |
| `ParticleRenderer` | V12 World effects runtime, раздел 15 и Render runtime 25 |
| `ExplosionSystem` | V12 World effects runtime, раздел 16 |
| `GameHud`, `GeneratorHud`, `OxygenGeneratorHud` | V12 World effects runtime, раздел 17 |
| `SpotLightComponent` | V8/V11 lighting и shader pipeline |

## 10 NPC и crew AI

| Файл или модуль | Где разобран |
|---|---|
| `NPCComponents` | V7 NPC и V11 AI architecture, владельцы данных |
| `NpcCommandSystem` | V12 Physics geometry AI, раздел 18 |
| `NpcUtilitySystem` | V12 Physics geometry AI, разделы 19–20 |
| `StateSystem` | V7 NPC, исполнение state/route/repair |
| `NpcCombatSystem` | V12 Physics geometry AI, раздел 22 |
| `NpcTacticalDecisionSystem` | V12 Physics geometry AI, раздел 23 |
| `NpcCoverQuery` | V12 Physics geometry AI, разделы 24–25 |
| `NpcCoverSystem` | V12 Physics geometry AI, раздел 26 |
| `NpcCombatDecisionRules` | V12 Physics geometry AI, раздел 27 |
| `NpcRangedCombatSystem` | V12 Physics geometry AI, раздел 28 |
| `NpcNavMeshSystem` | V12 Physics geometry AI, разделы 29–32 |
| `NpcCrowdSpacing` | V12 Physics geometry AI, раздел 33 |
| `NpcSquadCombatSystem` | V12 Physics geometry AI, раздел 34; необязательный group layer |
| `CrewTaskCoordinator` | V12 Physics geometry AI, раздел 21 |
| `NpcSimulationStudio` | V12 Physics geometry AI, раздел 35 и V11 Studio chapter |

## 11 Как пользоваться индексом при изучении файла

1. Найди файл в таблице.
2. Сначала прочитай его профильную V12 главу.
3. Открой исходник и подпиши возле каждого значимого блока: вход, ранний выход, чтение, вычисление, запись, событие.
4. Для непонятной формулы вернись в математическую главу.
5. Для внешнего API вернись в DirectX, Bullet или import chapter.
6. Если обнаружен файл runtime, которого нет в индексе, добавь его сюда одновременно с новой системой. Так документация остаётся проверяемой.

