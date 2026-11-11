# Physics geometry navigation и AI построчный runtime

Эта глава разбирает исполнение физики, изменение mesh, навигацию и NPC. Теория поведения из предыдущих глав здесь не повторяется: внимание на том, что делает код сверху вниз и кто имеет право менять состояние.

## 1 PhysicsSystem InitializeWorld

1. Метод проверяет, не создан ли world раньше.
2. PhysicsWorld или совместимый singleton создаёт collision configuration.
3. Dispatcher получает configuration.
4. Broadphase отвечает за грубый поиск потенциальных пар.
5. Solver разрешает контакты и constraints.
6. Dynamics world связывает эти части.
7. Gravity устанавливается один раз из engine settings.
8. Ошибка любой стадии вызывает cleanup уже созданных частей.

## 2 Создание collision shapes

### 2.1 Primitive shapes

1. Radius, half extents и height ограничиваются положительными минимумами.
2. Scale применяется согласованно: sphere использует консервативный максимальный scale, box — покомпонентный.
3. Capsule отдельно учитывает радиус и цилиндрическую высоту.
4. Новая shape возвращается вызывающему коду, который обязан определить ownership.

### 2.2 Convex hull

1. CPU vertices читаются из model.
2. Пустая геометрия завершает создание.
3. Каждая вершина умножается на scale.
4. Точки добавляются в `btConvexHullShape`.
5. Hull recalculates local AABB.
6. Margin задаётся после построения.

### 2.3 Triangle mesh

1. Требуются vertices и минимум один triangle.
2. Индексы проверяются по диапазону.
3. Triangles добавляются в `btTriangleMesh`.
4. Static body может использовать BvhTriangleMeshShape.
5. Dynamic concave body запрещён или заменяется convex approximation, потому что Bullet не рассчитан на обычное динамическое BVH mesh.
6. `triangleMesh` хранится рядом с shape и живёт не меньше неё.

### 2.4 Compound

1. Создаётся `btCompoundShape`.
2. Каждый element преобразуется в primitive или mesh child.
3. Local position/rotation формируют child transform.
4. Child добавляется в compound.
5. Ownership child shapes сохраняется для корректного удаления.

## 3 PhysicsSystem CreateRigidBody

1. Требуются TransformComponent и RigidBodyComponent.
2. Physics model выбирается между отдельной collision model и visual model.
3. NaN position отклоняется до попадания в Bullet.
4. Неположительный scale исправляется или приводит к отказу.
5. Interior collider исторической конструкции создаётся отдельным path, если это включено.
6. Compound path перебирает elements.
7. Обычный path выбирает shape через `shapeType`.
8. Effective mass становится нулевой для static/kinematic body.
9. Dynamic body вычисляет local inertia.
10. NaN inertia не передаётся Bullet.
11. Motion state создаётся с world transform.
12. Construction info получает mass, shape, inertia, friction и restitution.
13. Body создаётся и записывается в component.
14. CCD threshold и swept sphere radius включаются для быстрых объектов.
15. Static, kinematic и dynamic flags устанавливаются взаимоисключающе.
16. Collision group и mask определяют, с кем body сталкивается.
17. Неактивная ECS entity не добавляется в world до активации.

## 4 RefreshRigidBody

1. Старый body удаляется из world.
2. Motion state освобождается.
3. Body удаляется до shape.
4. Old shape и triangle mesh освобождаются.
5. Runtime pointers зануляются.
6. `CreateRigidBody` вызывается с текущими component settings.

Refresh нельзя вызывать внутри Bullet callback, когда world итерирует collision objects.

## 5 PhysicsSystem Update до шага

1. Character controllers синхронизируются с внешне изменёнными transforms.
2. Для каждого rigid body проверяется ActiveComponent.
3. Деактивация удаляет body из world и ставит `suspendedByActiveState`.
4. Повторная активация возвращает body с корректной transform.
5. Kinematic body каждый кадр получает ECS transform.
6. Dynamic body не перезаписывается transform без явного teleport, иначе физика потеряет собственное движение.
7. `ScaleChangedExternally` запускает refresh shape.
8. `TransformChangedExternally` для static body обновляет Bullet transform.
9. Forces, buoyancy и другие producers применяются до `stepSimulation`.

## 6 Историческая buoyancy часть

Она остаётся в текущем PhysicsSystem, хотя flooding исключён из учебника.

1. Проверяются active entity, rigid body и transform.
2. Bounds тела сравниваются с effective water level.
3. Полностью выше — submersion zero.
4. Полностью ниже — submersion one.
5. Пересечение интерполирует долю.
6. Buoyant force зависит от вытесненного объёма и submersion.
7. Drag уменьшает линейную и угловую скорость.

Если вода окончательно не нужна, этот блок лучше удалить из общего PhysicsSystem или превратить в подключаемый force module.

## 7 PhysicsSystem Update после шага

1. World делает фиксированные substeps через PhysicsWorld.
2. Dynamic body world transform читается из motion state.
3. NaN и слишком далёкие координаты отбрасываются как аварийная защита.
4. Position и rotation записываются в ECS TransformComponent.
5. Scale остаётся ECS-данными и не читается из Bullet.
6. Character ghost transforms синхронизируются отдельным helper.
7. Последние applied transforms сохраняются для обнаружения внешнего teleport в следующем кадре.

## 8 PhysicsRaycast

1. Start и end переводятся в `btVector3`.
2. Callback хранит collision mask.
3. Ignored entities или collision objects фильтруются.
4. `rayTest` возвращает ближайшее допустимое попадание.
5. Collision object user pointer переводится в ECS entity.
6. Hit point, normal и fraction копируются в engine struct.
7. Отсутствие world или hit возвращает пустой результат.

Один общий wrapper не даёт оружию, interaction и cover query по-разному трактовать Bullet.

## 9 MeshCutting math helpers

### 9.1 Vector helpers

`Length`, `Subtract`, `Normalize`, `Cross` и `Dot` делают операции над XMFLOAT3 без хранения SIMD-ссылок. `Normalize` проверяет почти нулевую длину, чтобы не получить NaN.

### 9.2 LerpPolyVertex

1. Position интерполируется по `t`.
2. UV интерполируется тем же `t`.
3. Normal, tangent и binormal интерполируются.
4. Направляющие векторы затем нормализуются.

Одинаковый `t` сохраняет непрерывность атрибутов на новой границе triangle.

### 9.3 2D geometry

1. Cut plane задаёт локальные оси, проецирующие triangle в 2D.
2. `SignedDistanceToEdge` определяет сторону линии.
3. `DistancePointSegmentSq` clamp-ит projection parameter от нуля до единицы.
4. `PointInTriangle2D` проверяет ориентации рёбер.
5. `CircleOverlapsTriangle2D` проверяет вершины, центр в triangle и расстояние до рёбер.

## 10 Polygon clipping и triangulation

1. Triangle превращается в polygon из трёх PolyVertex.
2. Каждая граница cut profile последовательно режет polygon.
3. Переход inside/outside создаёт интерполированную вершину.
4. Inside vertices сохраняются для удаляемой области.
5. Outside pieces сохраняются как будущая поверхность.
6. Polygon меньше трёх вершин отбрасывается.
7. Выпуклый polygon триангулируется веером.
8. Ориентация triangles сохраняется, чтобы normals не перевернулись.

## 11 RayModelHit

1. World ray переводится inverse world matrix в local coordinates model.
2. Local direction нормализуется.
3. AABB test быстро отбрасывает model.
4. Индексы читаются тройками.
5. Невалидный индекс пропускает triangle.
6. Ray-triangle test вычисляет local hit.
7. Hit переводится обратно в world.
8. World distance сравнивается с текущим ближайшим.
9. Normal разворачивается против луча, если triangle orientation смотрит внутрь.
10. Результат хранит entity, model, hit point, normal и distance.

## 12 Cut radius и глубина

1. Radius берётся относительно bounds model.
2. Минимум не даёт отверстию исчезнуть из-за масштаба.
3. Максимум не позволяет одному выстрелу удалить половину объекта.
4. World radius переводится в local radius с учётом scale.
5. Depth определяет, режется только передняя поверхность или сквозной слой.

## 13 Pending lock и damage history

1. `TryMarkCutPending(model)` проверяет global set под mutex.
2. Уже обрабатываемый model получает false.
3. Новый model добавляется и получает true.
4. `ClearCutPending` вызывается при любом завершении, включая ошибку submit.
5. RuntimeMeshDamageComponent хранит baseline vertices/indices и список cut operations.
6. Baseline записывается один раз до первого изменения.
7. Operation id позволяет позже отменить конкретный cut.

## 14 Detached islands

1. Welded positions квантуют координаты, объединяя почти одинаковые вершины.
2. Triangle adjacency строится по общим welded positions.
3. DFS/BFS находит connected components triangles.
4. Площадь каждой island суммируется.
5. Самая большая island считается основным объектом.
6. Маленькие detached islands помечаются на удаление.
7. Оставшиеся indices compact-ятся.
8. Неиспользуемые vertices удаляются через remap table.

Это геометрическая связность, не Bullet islands.

## 15 Apply cut CPU и GPU

1. Проверяется размер исходных buffers и защитный maximum indices.
2. Каждый triangle тестируется против cut volume.
3. Незатронутые triangles копируются как есть.
4. Затронутые режутся polygon algorithm.
5. Если изменений нет, операция возвращает false.
6. При необходимости удаляются detached fragments.
7. Пустая итоговая mesh отклоняется.
8. Новые immutable/dynamic D3D buffers создаются во временные pointers.
9. Только после успеха старые vertex/index buffers освобождаются.
10. CPU arrays заменяются тем же итогом.
11. Bounds пересчитываются.

## 16 Async cut

1. Main thread выполняет ray query и собирает immutable input snapshot.
2. Pending lock резервирует models.
3. Worker изменяет только CPU arrays.
4. Worker не вызывает D3D device context и не трогает registry.
5. Result помещается в main-thread queue.
6. Main thread проверяет жизнь entity и актуальность operation generation.
7. GPU buffers заменяются.
8. PhysicsSystem refresh-ит collision mesh, если он тоже был изменён.
9. Pending locks снимаются на всех paths.

## 17 RepairCut

1. Нулевой operation id отклоняется.
2. Ищется damage track, содержащий operation.
3. Если model pending, ремонт не начинается поверх cut.
4. Geometry восстанавливается из baseline.
5. Все оставшиеся cut operations повторно применяются кроме удаляемой.
6. Полученный mesh заменяет CPU/GPU buffers.
7. Collision body обновляется.
8. Operation удаляется из history только после успешного commit.

Таким образом repair не пытается «заклеить triangles назад» локально, а детерминированно воспроизводит оставшиеся повреждения.

## 18 NpcCommandSystem

### 18.1 Begin

1. Command revision увеличивается.
2. Type и active flag записываются.
3. Ошибки прошлой команды очищаются.
4. Navigation runtime сбрасывается.

### 18.2 ResumeAutomatic

1. Ручная команда очищается.
2. Brain возвращается automatic policy.
3. Route dirty заставляет navigation пересчитать путь.

### 18.3 Stop Wander Move Follow Repair

Каждый публичный метод сначала вызывает Begin с нужным type, затем записывает только свои параметры: target position, room или entity. Он не двигает NPC сам.

### 18.4 ForceRepath и ResetNavigation

1. Route очищается.
2. Index ставится в ноль.
3. `routeDirty = true`.
4. Stuck/no-progress state сбрасывается.

## 19 NpcUtilitySystem Update

1. Перебираются NPCComponent и NpcBrainComponent.
2. Читаются crew, combat и faction.
3. Faction combatant исключается из старой repair utility, даже если legacy `isHostile` false.
4. Inactive и off-duty NPC тоже недоступны.
5. Если такой NPC выполнял автоматический repair state, target очищается и state возвращается Idle.
6. Manual command не стирается.
7. Доступный NPC передаётся `EvaluateNpc`.

## 20 NpcUtilitySystem EvaluateNpc

1. Idle score задаёт небольшой baseline.
2. Best repair target начинается INVALID, score нулём.
3. Все breaches оцениваются, но claim другого NPC исключает цель.
4. Все damageable equipment оцениваются отдельно.
5. Entity, уже являющаяся breach, не оценивается второй раз.
6. Лучший score и target сохраняются.
7. Debug `repairScore` обновляется даже при manual command.
8. Активная manual command завершает automatic arbitration.
9. Уже ремонтирующий NPC сохраняет валидную цель, уменьшая дёрганье.
10. Смена target ставит `routeDirty`.
11. Repair побеждает Idle только при валидной цели и большем score.
12. Иначе target очищается и state становится Idle.

### 20.1 Score ремонта

1. Severity растёт с недостающим health, размером breach или потерей функции.
2. Distance уменьшает score.
3. Role affinity повышает задачи своей профессии.
4. Danger или combat pressure может понизить обычный ремонт.
5. Claim исключает дублирование.
6. Clamp удерживает сравнимый диапазон.

## 21 CrewTaskCoordinator

1. Update сначала удаляет невалидные claims.
2. Затем собирает repair candidates.
3. Crew list содержит active, alive и on-duty членов.
4. Hostile/combat NPC и manual commands пропускаются.
5. Валидное прежнее assignment сохраняется, если claim всё ещё принадлежит NPC.
6. Для свободного NPC каждый target получает score по distance, role и urgency.
7. Лучший target получает `CrewTaskClaimComponent` с assignee.
8. Brain получает repairTarget и MoveToRepair.
9. Snapshot обновляет counts для UI.

Claim — reservation. Utility score отвечает «что выгодно», Coordinator — «кто именно имеет право это делать».

## 22 NpcCombatSystem поиск цели

1. Внешний цикл перебирает NPCCombatComponent: это observer/attacker.
2. Observer получает NPC, brain, transform и faction.
3. Мёртвый, inactive или manual-command NPC пропускается согласно policy.
4. Внутренний цикл перебирает faction candidates.
5. `candidateEntity == npcEntity` исключает самого себя.
6. `AreEnemies(observerFaction, candidateFaction)` определяет отношение симметрично или через таблицу.
7. Candidate должен быть живым NPC или живым PlayerComponent, но не обязан иметь оба компонента.
8. Distance сравнивается с detection range.
9. FOV проверяет угол между forward и направлением на кандидата.
10. Physics line of sight подтверждает контакт.
11. Лучший target выбирается по score/distance, а не «последний в ForEach».
12. Target entity и last known position обновляются только при подтверждённом восприятии.
13. Потеря видимости не телепатически обновляет позицию.

## 23 NpcTacticalDecisionSystem

1. Threat distance sample сравнивается с прошлым и делится на `deltaTime`.
2. Отрицательная скорость расстояния означает, что враг приближается.
3. Manual selected style сразу назначает active style.
4. Auto style соблюдает `tacticCommitTimer`, чтобы не переключаться каждый кадр.
5. Dangerously close выбирает defensive/withdraw response.
6. Cautious NPC раньше реагирует на близкую угрозу.
7. Enemy pushing и under fire усиливают оборонительный выбор.
8. Высокая aggression склоняет к assault.
9. Низкая aggression — к defensive.
10. Style меняет desired distance, cover preference и search pressure, но не двигает NPC напрямую.

## 24 NpcCoverQuery статические точки

1. Перебираются cover point entities.
2. Rejected cover и чужая reservation исключаются.
3. Distance от NPC ограничивает стоимость пути.
4. Ray от target к нескольким высотам cover position проверяет реальную защиту.
5. Hit самого target не считается стеной.
6. Cover point должен лежать на доступной navigation area.
7. Для левой и правой стороны строятся peek candidates.
8. Peek должен видеть target и иметь место для capsule.
9. Score учитывает protection, path distance, preferred combat range, angle и style.
10. Лучший result хранит cover, hide, peek и score.

## 25 NpcCoverQuery dynamic cover

1. Если статическая точка не найдена и `allowDynamicCover`, начинается geometry query.
2. Bullet obstacles рассматриваются как потенциальные стены.
3. Candidate hide position строится с противоположной от target стороны obstacle.
4. Высоты 1.0, 1.6 и 0.55 проверяют разные уровни защиты.
5. Navmesh проецирует точку на достижимую поверхность.
6. Левый и правый peek строятся вдоль tangent obstacle.
7. Dynamic result получает virtual id/state, а не постоянную ECS cover point.
8. Debug rays сохраняют причины rejection.

## 26 NpcCoverSystem Update

1. Null world, NaN или неположительное время завершают update.
2. Перебираются combat NPC с tactical, brain, transform и health.
3. Timers уменьшаются с clamp до нуля.
4. Damage reaction превращается в under-fire memory даже при invulnerability.
5. Невалидный target освобождает cover state.
6. Existing reserved cover повторно проверяется против текущей target position.
7. `coverChangeRequested` освобождает reservation, временно отвергает старую точку и обнуляет decision timer.
8. NPC без cover вызывает CoverQuery только когда cooldown разрешает.
9. Успешный result резервируется и создаёт move target hide position.
10. При достижении hide position state становится InCover и начинается hide timer.
11. По окончании hide timer NPC идёт к peek position.
12. В peek state `exposed = true`, запускается exposure timer и разрешается атака.
13. Истечение exposure или попадание возвращает за cover.
14. Failed peek увеличивается только при действительно неудачной экспозиции.
15. Достижение лимита просит сменить cover.
16. Reload может удерживать NPC за cover дольше.

## 27 NpcCombatDecisionRules

1. `CanUseMovingFire` разрешает стрельбу только определённым movement states и видимой цели.
2. TakingCover может стрелять, если не находится в скрытой фазе.
3. Chasing разрешает огонь при сохранённом контакте.
4. `ShouldRelocateAfterFailedPeeks` сравнивает count с limit.
5. `NeedsNewAngle` требует новый угол при невидимой цели или подавлении согласно policy.
6. Чистые inline rules не меняют components, поэтому легко тестируются.

## 28 NpcRangedCombatSystem

1. Система получает combat target и WeaponComponent.
2. Weapon timers уже должны быть обновлены.
3. Target visibility проверяется в этот кадр.
4. Muzzle ray подтверждает, что ствол не упирается в стену.
5. Fire lane ray ищет союзника между muzzle и target.
6. Союз определяется фракцией, а не legacy `isHostile`.
7. Aim time накапливается только при устойчивой возможности стрелять.
8. Movement увеличивает spread или требует дополнительного aim time.
9. Reload request создаётся при пустом магазине или тактической возможности.
10. `WeaponSystem::TryFire` выполняет выстрел и урон.
11. Ranged status по-русски объясняет hold fire, reload, aim или fire.

## 29 NpcNavMeshSystem build

1. Geometry собирается из отмеченных surfaces, rigid bodies и static colliders.
2. Doors исключаются или учитываются отдельным connectivity rule.
3. Duplicate Bullet bodies удаляются set-ом.
4. World bounds определяют Recast grid.
5. Cell count ограничивается, защищая память.
6. Triangles rasterize-ятся в heightfield.
7. Walkable slope и agent height фильтруют поверхность.
8. Compact heightfield строит свободные spans.
9. Regions и contours выделяют связные области.
10. Poly mesh и detail mesh создают navigation polygons.
11. Detour data превращается в `dtNavMesh` и `dtNavMeshQuery`.
12. Ошибка любой стадии освобождает промежуточные Recast structures.

## 30 Navmesh cache и fingerprint

1. Geometry fingerprint хеширует bodies, shapes и quantized transforms.
2. До `nextGeometryCheck` старый mesh переиспользуется.
3. Совпавший fingerprint продлевает cache без rebuild.
4. Изменение fingerprint пересобирает room/world mesh.
5. Paths, относящиеся к перестроенной области, инвалидируются.

## 31 BuildPath и движение

1. Start и target проецируются на ближайшие polygons.
2. Если target не проецируется, fallback пытается использовать допустимую ближайшую точку.
3. `findPath` строит corridor polygon refs.
4. Неполный corridor получает ближайший достижимый endpoint.
5. `findStraightPath` создаёт world waypoints.
6. Cache хранит points, next index, room и target.
7. Достигнутые points пропускаются по threshold.
8. Финальная точка завершает path и сообщает arrival.

## 32 Dynamic avoidance

1. Nearby rigid bodies фильтруются по высоте персонажа.
2. Для obstacle вычисляются forward distance и lateral clearance.
3. Объект позади или достаточно сбоку не мешает.
4. Лучший blocking obstacle запоминается.
5. Side memory удерживает выбранную сторону некоторое время, предотвращая метание влево-вправо.
6. Candidate avoidance point проецируется на navmesh.
7. Слишком близкая точка отклоняется.
8. При отсутствии точки используется основной path direction.

## 33 NpcCrowdSpacing

1. Для пары NPC вычисляется XZ difference.
2. Почти нулевая дистанция получает стабильное запасное направление.
3. Внутри personal radius сила отталкивания растёт.
4. Result суммируется по соседям.
5. Максимальная correction ограничивается.
6. Spacing корректирует steering, но не меняет tactical goal.

## 34 NpcSquadCombatSystem

Group tactics рассматривается как отдельный необязательный слой.

1. Сначала собираются участники одной стороны.
2. Shared contact создаётся только после подтверждения наблюдателем.
3. Получатель данных не считает target видимой автоматически; он должен подтвердить контакт самостоятельно.
4. Roles назначаются с commit timer.
5. Reservations не дают двум союзникам занять одну cover point.
6. Assault, support, flank и defensive goals пишутся как предложения tactical layer.
7. Individual safety rules имеют право отклонить предложение.
8. Feature flag может отключить весь слой, сохранив одиночный AI.

## 35 NpcSimulationStudio

1. `Capture` перебирает NPC и строит compact snapshot.
2. Health читается из общего damageable или NPC data.
3. Combat snapshot сохраняет target, visibility, ammo и reload.
4. Tactical snapshot сохраняет order, reason, cover, timers и failed peeks.
5. Первый snapshot создаёт baseline без ложных событий.
6. Следующие сравниваются с предыдущим.
7. Изменение state создаёт decision event.
8. Command revision создаёт command event.
9. Target/visibility создаёт perception event.
10. Уменьшение health создаёт damage event.
11. Route change и no-progress threshold создают navigation events.
12. Deque ограничивается `MaxEvents`, удаляя самые старые записи.

### 35.1 QueueDamage

1. UI записывает request в отдельный buffer.
2. Pending flag сообщает runtime, что запрос есть.
3. `ConsumeDamageRequest` атомарно отдаёт один запрос и снимает pending.
4. Damage проходит обычный DamageSystem, а не тайно меняет health из UI.

### 35.2 ExportHistory

1. Файл открывается до изменения UI state.
2. Text export пишет одну запись на строку.
3. JSON escape обрабатывает кавычки, slash и control characters.
4. Russian translation применяется только к отображению, не к enum storage.
5. Ошибка открытия возвращает false.

## 36 AI система без конфликтов

Для каждого поля нужен один writer:

1. Perception пишет visible и last known facts.
2. Utility/goal layer выбирает намерение.
3. Tactical layer выбирает позиционный способ.
4. Navigation пишет путь и steering.
5. Character controller исполняет движение.
6. WeaponSystem владеет ammo, reload и shot cooldown.
7. Animation читает факты и пишет pose.
8. Studio только наблюдает и создаёт команды через обычные очереди.

Если две системы пишут `brain.state`, вводится priority/arbitration или разделяются поля `desiredState` и `executedState`.

## 37 Что рефакторить в этом блоке

1. Выделить buoyancy из PhysicsSystem как подключаемый force module.
2. Заменить global pending maps MeshCutting на scene-owned service.
3. Ввести scene generation для async cut/repair results.
4. Разделить NpcCoverSystem на evaluator и phase executor.
5. Сохранить pure decision rules без ECS для unit-тестов.
6. Вынести Recast build, cache и steering в три класса.
7. Сделать squad system producer-ом предложений, а не прямым владельцем individual state.

## 38 StateSystem исполнитель выбранного поведения

`StateSystem` отвечает на вопрос «как выполнить уже выбранную задачу». Utility, command и tactical systems выбирают намерение; StateSystem превращает его в movement, navigation и repair execution.

### 38.1 WorkZone helpers

1. `GetClosestPointInWorkZone` строит inverse world matrix зоны.
2. World position NPC переводится в local coordinates.
3. Каждая координата clamp-ится внутри куба от минус единицы до единицы.
4. Local point переводится обратно в world.
5. `TransformWorkZoneLocalPoint` выполняет только обратное преобразование для заранее выбранного sample.
6. `TryPickReachableWorkZonePoint` собирает ближайшую точку, центр, углы и середины граней.
7. Каждый candidate проецируется NavMeshSystem.
8. Непроецируемая точка в известной комнате отбрасывается.
9. Побеждает ближайшая достижимая XZ-позиция.

### 38.2 Update общий проход

1. Неположительный `deltaTime` запрещает движение и прогресс ремонта.
2. Перебираются brain и transform.
3. Inactive NPC получает нулевое movement и нулевой Bullet controller velocity.
4. Конфликтующий RigidBodyComponent удаляется: capsule controller остаётся единственным physics owner.
5. `EnsureComponent` создаёт недостающие movement/controller runtime components.
6. Полностью пустой controller создаётся через CharacterSpawnSystem.
7. Частично созданный controller не перестраивается поверх старых pointers.
8. NPC speeds копируются в общий CharacterMovement.
9. Animated mesh без AnimationComponent получает default component.
10. Current room определяется с предпочтением прошлой комнаты на дверном пороге.
11. `UpdateNpc` исполняет command или state.
12. Pair separation корректирует направление, но не телепортирует transform.
13. Portal collision bypass выключается, если в этом кадре его никто не запросил.
14. `UpdateStuckStatus` сравнивает фактический прогресс с желаемым движением.

### 38.3 EnsureComponent

1. Сначала запрашивается существующий T.
2. При null добавляется default T.
3. Pointer запрашивается снова, потому что storage мог перераспределиться.
4. Возвращается актуальный адрес.

### 38.4 UpdateNpc arbitration

1. Active manual command имеет абсолютный приоритет и завершает метод через return.
2. Устаревшие combat states без target и memory переводятся в search/wander policy.
3. Switch по brain state вызывает узкий executor: idle, wandering, chasing, taking cover, attacking, repair или waiting door.
4. Executor меняет movement intent и status, но Bullet transform меняется позже controller system.
5. Неизвестное state безопасно останавливает движение и пишет диагностику.

### 38.5 Manual commands

1. Stop обнуляет движение.
2. Wander использует тот же устойчивый wander runtime, что automatic search.
3. MoveToPosition вызывает NavigateTo target position.
4. MoveToRoom сначала строит межкомнатный route.
5. FollowEntity каждый кадр читает актуальную target position, но держит follow distance.
6. RepairEntity подтверждает валидность repair target до движения.
7. Завершение или ошибка записывает command result и возвращает automatic control только по явному правилу.

### 38.6 Межкомнатный BFS

1. Start и goal rooms являются узлами графа.
2. Doors дают рёбра только при валидных связях.
3. Queue обходит комнаты по слоям.
4. `RouteNode` хранит previous room и via door.
5. Goal раскручивается назад в список steps.
6. Список разворачивается в прямой порядок.
7. Закрытая дверь переводит NPC в WaitingForDoor и создаёт interaction/open request.
8. После прохода порога current route index увеличивается.

### 38.7 NavigateTo

1. Для текущей room запрашивается NavMesh path или reachable point.
2. Следующий waypoint превращается в XZ direction.
3. Arrival threshold возвращает Arrived.
4. Door wait возвращает Waiting.
5. Ошибка path и невозможность fallback возвращают Blocked.
6. Обычное движение возвращает Moving.
7. Desired speed выбирает walk/run по distance, stamina и tactical urgency.

### 38.8 Repair execution

1. Repair target повторно проверяется каждый кадр.
2. Work zone или ближайшая reachable point задаёт позицию выполнения.
3. Пока NPC далеко, StateSystem только движет его.
4. В радиусе движения останавливается и начинается progress.
5. JobSystem получает тяжёлую mesh repair работу, если она нужна.
6. Claim остаётся за NPC до успеха, отмены или невалидной цели.
7. Успех очищает target, state и reservation; Utility выберет следующую задачу позднее.

### 38.9 Stuck status

1. Если NPC хочет двигаться, сравнивается текущая позиция с последним sample.
2. Достаточный прогресс сбрасывает no-progress timer.
3. Недостаточный прогресс накапливает timer.
4. Первый threshold просит repath.
5. Повторный длительный stuck меняет status/blocked reason и разрешает higher layer выбрать другую цель.
6. Когда движения нет по замыслу, stuck timer не растёт.

