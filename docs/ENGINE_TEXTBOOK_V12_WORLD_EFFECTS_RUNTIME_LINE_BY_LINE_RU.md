# Мир энергия помещения ремонт и эффекты

Исторические названия `Ship`, `Submarine` и `Hull` в этой главе не означают, что игра снова обязана стать игрой про подлодку. Они описывают сохранившиеся runtime-модули. `FloodingSystem` намеренно исключён как рудимент. Системы давления, кислорода, помещений и энергии разобраны, потому что они по-прежнему полезны для станции, корабля, базы или герметичного здания.

## 1 PowerSystem простой распределитель

1. `Update` собирает активные generators и суммирует доступные мегаватты.
2. Неактивный, сломанный или остановленный generator не добавляет мощность.
3. Consumers собираются отдельно вместе с priority и requested power.
4. Перед новым распределением каждый consumer получает `powered = false` и нулевую delivered power. Это стирает результат прошлого кадра.
5. Сортировка ставит меньший номер приоритета раньше.
6. Цикл берёт первый consumer и сравнивает request с available power.
7. Полное питание отмечает consumer powered и уменьшает остаток.
8. Если частичное питание не разрешено, недостаточная мощность оставляет consumer выключенным.
9. Если частичное разрешено, delivered получает остаток, а коэффициент питания считается как delivered/requested.
10. Телеметрия сохраняет generation, demand, supplied и shortage.

Этот вариант подходит одному общему bus. Для нескольких независимых сетей используется `ShipPowerGridSystem`.

## 2 GeneratorSystem

### 2.1 Update

1. Отрицательный `deltaTime` заменяется нулём.
2. ECS-перебор вызывает `UpdateGenerator` для каждого generator component.
3. Система не распределяет электричество; она только вычисляет, сколько конкретный источник способен произвести.

### 2.2 MoveTowards

1. Если current меньше target, прибавляется не больше `maxDelta`.
2. Если current выше target, вычитается не больше `maxDelta`.
3. Иначе значение уже достигнуто.

Функция убирает overshoot и делает ramp симметричным.

### 2.3 UpdateGenerator

1. Requested output ограничивается паспортным максимумом.
2. Состояние enabled, fuel и scram определяет target output.
3. Output плавно идёт к target через ramp rate.
4. Расход fuel равен мощности умножить на время и коэффициент расхода.
5. Работа увеличивает heat, остановка охлаждает.
6. Достижение `scramTemperature` включает аварийное отключение.
7. Scrammed generator остаётся выключенным, пока heat не упадёт до `restartTemperature`.
8. Отсутствие fuel принудительно обнуляет target и фактический output постепенно.
9. Все ресурсы ограничиваются допустимыми диапазонами.

## 3 EmergencyBatterySystem

### 3.1 Discharge

1. Нулевой запрос, нулевой `deltaTime` или пустая батарея возвращает ноль.
2. Requested MW переводятся в MWh через время в часах.
3. Доступная энергия ограничивает фактическую мощность.
4. Charge уменьшается на реально отданную энергию.
5. Возвращается delivered MW, а не MWh.

### 3.2 Charge

1. Отрицательный surplus не заряжает батарею.
2. Свободная ёмкость вычисляется из capacity минус charge.
3. Charging efficiency уменьшает реально сохранённую энергию.
4. Charge ограничивается capacity.
5. Возвращаемая принятая мощность позволяет telemetry правильно посчитать остаток surplus.

MW — скорость передачи энергии, MWh — запас. Их нельзя складывать без умножения на время.

## 4 ShipPowerGridSystem

### 4.1 FindGridRoot

1. Если entity сама имеет `PowerGridComponent`, она и есть root.
2. Иначе `ShipPartComponent` указывает на родительскую конструкцию.
3. Невалидная связь возвращает `INVALID_ENTITY`.

### 4.2 Update сбор

1. Создаётся временная map `root -> WorkingGrid`.
2. Grid components регистрируют roots.
3. Каждый generator относится к root через `FindGridRoot`.
4. Каждый consumer добавляется в соответствующий рабочий grid.
5. Grid без валидного root не получает случайную энергию из другой сети.
6. После сбора `UpdateGrid` вызывается отдельно для каждой сети.

### 4.3 UpdateGrid

1. Generation суммируется из фактического output generators.
2. Demand суммирует запросы активных consumers.
3. При дефиците battery разряжается и добавляет available MW.
4. Consumers сортируются по priority и стабильному entity order.
5. Перед распределением старые powered flags очищаются.
6. Available power выдаётся последовательно.
7. При избытке остаток направляется в battery charge.
8. Telemetry получает generation, battery flow, demand, supplied и blackout state.

Сортировка по entity после priority нужна для воспроизводимости: одинаковая сцена не должна случайно питать разные устройства.

## 5 OxygenGeneratorSystem

1. Неположительный `deltaTime` завершает update.
2. Для каждого oxygen generator определяется grid root и доступность питания.
3. Disabled, broken или unpowered unit не производит кислород.
4. Output плавно зависит от power fraction.
5. Ищутся активные rooms, связанные с той же конструкцией.
6. При отсутствии rooms производство не исчезает в неизвестность: output telemetry обнуляется или остаётся как potential согласно component policy.
7. Produced oxygen делится между rooms.
8. Каждая комната ограничивает oxygen своим maximum.
9. Generator component сохраняет фактическую производительность.

## 6 RoomSystem

### 6.1 Зачем система нужна

`RoomSystem` определяет, какие динамические объекты находятся в каких объёмах. Это spatial membership, а не сама симуляция кислорода.

### 6.2 Synchronous path

1. Сначала у всех rooms очищаются `objectsInRoom`.
2. Перебираются trackable entities.
3. Character controller считается trackable всегда.
4. Обычный rigid body учитывается, только если он динамический.
5. World position переводится в local space каждой комнаты.
6. Point-in-box проверяет half extents.
7. Entity добавляется в первую или лучшую подходящую room согласно текущей политике.

### 6.3 Job path

1. `s_membershipJobPending.exchange(true)` не даёт запустить второй расчёт поверх первого.
2. Main thread создаёт snapshot rooms и entity positions. Worker не должен читать мутирующий ECS напрямую.
3. Пустой список комнат завершает задачу заранее.
4. Job вычисляет membership только на snapshot data.
5. Результат передаётся в main-thread queue.
6. Main thread ещё раз проверяет, что entities существуют, и применяет списки.
7. Pending flag снимается после применения или при ошибке submit.

Ссылка `&registry`, захваченная worker-задачей, безопасна только пока registry гарантированно жив дольше job. Лучше передавать в worker исключительно snapshot, а registry касаться лишь в main-thread callback.

## 7 RoomEnvironmentSystem

### 7.1 Update комнат

1. Нулевое время ничего не меняет.
2. Значения pressure и oxygen сначала ограничиваются физически допустимым диапазоном.
3. `ApplyBreaches` вычисляет утечки.
4. `EqualizeConnectedRooms` переносит среду через двери.
5. После transfer снова выполняется clamp, чтобы ошибки накопления не дали отрицательную атмосферу.

### 7.2 ApplyBreaches

1. Перебираются незапечатанные breach sources.
2. Source связывается с комнатой через membership или owner data.
3. Утечка зависит от размера отверстия, разницы давления и `deltaTime`.
4. Pressure и oxygen уменьшаются пропорционально, но не ниже нуля.
5. Suction включается только при ненулевых radius, strength и room pressure.
6. Для каждого объекта комнаты вычисляется направление к breach.
7. За пределами radius объект пропускается.
8. Falloff уменьшает воздействие с расстоянием.
9. Dynamic rigid body получает force или impulse.
10. Character не толкается как rigid body: скорость записывается через `CharacterExternalVelocitySystem`.

### 7.3 EqualizeConnectedRooms

1. Сначала строится список transfer, не изменяющий rooms на месте.
2. Для каждой пары связанных rooms читается door flow multiplier.
3. Разница давления определяет направление.
4. `LimitedFlow` не разрешает перенести больше половины разницы за один шаг.
5. Oxygen переносится вместе с долей атмосферы источника.
6. После сбора transfer применяются ко всем rooms.

Двухфазная схема исключает зависимость результата от порядка перебора дверей.

### 7.4 GetDoorFlowMultiplier

1. Система ищет дверь, соединяющую пару rooms в любом порядке.
2. Закрытая дверь даёт почти нулевой flow.
3. Открытая — полный или заданный коэффициент.
4. Частично открытая интерполирует поток через `openAmount`.

## 8 PressureSystem без затопления

Этот файл исторически принимает `waterLevel`, но рассматривается только как пример внешнего давления на корпус. Сам `FloodingSystem` в книгу не включён.

1. Система перебирает hull components.
2. Invulnerable entity пропускается.
3. Требуется rigid body и валидный Bullet body.
4. Depth вычисляется относительно уровня среды.
5. Объект выше уровня не получает внешнего гидростатического давления.
6. Pressure растёт с depth по заложенной формуле.
7. Если pressure выше `maxPressure`, создаётся damage или failure state.

Для космической игры этот модуль лучше обобщить до `ExternalPressureSystem`: входом будет давление снаружи, а не water level.

## 9 RoomCullingSystem

### 9.1 FindRoomContainingPoint

1. Перебираются active rooms.
2. World point переводится обратной матрицей transform комнаты.
3. Local coordinates сравниваются с bounds.
4. Возвращается room entity или INVALID.

### 9.2 BuildVisibleRooms

1. Начинаем с комнаты камеры.
2. Room добавляется в output только один раз.
3. Через открытые двери обходятся соседние rooms.
4. Depth или количество переходов ограничивается, чтобы граф с циклом не зациклил поиск.
5. Полностью закрытая дверь останавливает видимость.

### 9.3 Apply visibility

1. Пустой visible list означает fallback policy, а не обязательное скрытие всего мира.
2. Doors видимы, если относятся хотя бы к одной видимой комнате.
3. Специальные panel batches обрабатываются отдельно.
4. Для обычной entity ищется cached room.
5. Устаревший cache перепроверяется по transform.
6. Mesh renderer получает visible flag.
7. Debug stats считают tested, visible и culled entities.

Room culling не заменяет frustum culling. Сначала можно убрать невидимые комнаты, затем камерой отсечь объекты внутри видимых.

## 10 ShipAlertSystem

1. Каждый update начинает с нормального состояния.
2. Определяется комната игрока.
3. Низкие oxygen или pressure повышают уровень тревоги.
4. Breach в комнате формирует более конкретное сообщение.
5. Через ship root читается power grid.
6. Blackout или severe shortage повышает уровень.
7. `SetAlert` заменяет текущую тревогу только более серьёзной.
8. `GetCurrentAlert` отдаёт готовый snapshot HUD и звуку.

Это aggregator: он не чинит генератор и не наносит урон, а только превращает факты разных систем в одну понятную диагностику.

## 11 SubmarineJointSystem исторический модуль соединений

1. Система читает joint component и две связанные entities.
2. Невалидные или удалённые bodies приводят к безопасному удалению constraint.
3. При отсутствии runtime constraint он создаётся по anchor и axes.
4. Constraint добавляется в Bullet world после обоих bodies.
5. Нагрузка или относительное смещение сравниваются с break threshold.
6. При разрушении constraint сначала удаляется из world, затем освобождается.
7. Component получает broken state для gameplay и ремонта.

Название можно позже заменить на `StructuralJointSystem`, не меняя механику.

## 12 JointConnectionSystem

1. Система разрешает component-level ссылки на тела.
2. Локальные anchors переводятся в transforms Bullet.
3. Тип joint выбирает fixed, hinge или другой constraint.
4. Limits применяются после создания.
5. Existing constraint обновляется или пересоздаётся, когда configuration revision меняется.
6. Удаление entity вызывает detach constraint до body destruction.

## 13 RepairSystem Update

### 13.1 Начало кадра

1. Feedback и текущая progress display приводятся к состоянию этого кадра.
2. Неположительный `deltaTime` не накапливает ремонт.
3. Требуются camera и physics world.
4. Ищется local player и controller.
5. Если кнопка repair не удерживается, активная попытка сбрасывает progress согласно policy.

### 13.2 Поиск цели

1. Ray идёт из камеры на repair distance.
2. Bullet hit даёт ближайший collider entity.
3. Дополнительно проверяются runtime mesh damage marks, потому что одна entity может иметь несколько повреждений.
4. Кандидат должен иметь RepairableComponent и быть не repaired.
5. Distance и angle сравниваются с допустимыми значениями.
6. Лучший кандидат выбирается по ray distance, а не по порядку ECS.

### 13.3 Накопление

1. Смена target сбрасывает или переносит progress по явному правилу.
2. Repair rate может зависеть от инструмента.
3. Progress увеличивается на rate умножить `deltaTime`.
4. При достижении required time вызывается CompleteRepair.
5. HUD получает target, progress и причину блокировки.

### 13.4 CompleteRepair

1. Repairable отмечается repaired.
2. Breach source получает sealed state.
3. Damage decal или runtime hole удаляется через специализированный helper.
4. Structural health восстанавливается в пределах настройки.
5. Событие ремонта отправляется другим системам.
6. Active target и progress очищаются.

### 13.5 Runtime mesh repair

1. Damage record ищется по id.
2. Async cut/repair job не должен одновременно менять тот же mesh.
3. CPU geometry восстанавливается или перестраивается.
4. GPU buffers заменяются на main thread.
5. Старые buffers освобождаются после успешной замены.

## 14 ParticleSystem lifecycle

### 14.1 Initialize и renderer

1. Конструктор задаёт deterministic random seed и назначает active system.
2. `SetDevice` не создаёт renderer повторно.
3. Первая отрисовка лениво создаёт `ParticleRenderer`.
4. Флаг initialization attempted предотвращает бесконечные неудачные попытки каждый кадр.
5. Destructor снимает active pointer, только если он указывает на этот instance.

### 14.2 Emitters

1. `CreateEmitter` проверяет `MAX_EMITTERS`.
2. Новый id не может совпасть с invalid sentinel.
3. Descriptor копируется в runtime emitter.
4. Некоторые effect types получают готовые presets.
5. SetPosition, SetActive и UpdateEmitter сначала ищут id.
6. DestroyEmitter удаляет именно runtime emitter, но уже выпущенные частицы могут дожить.
7. `EmitBurst` создаёт заданное количество сразу без постоянного emitter.

### 14.3 Muzzle explosion impact presets

1. `EmitMuzzleFlash` нормализует direction и создаёт короткие яркие частицы.
2. Smoke получает больший lifetime и меньшую скорость.
3. `EmitExplosion` комбинирует flash, sparks, smoke и debris presets.
4. Metal impact использует hit normal для полусферы разлёта.
5. Hull dent и crack создают отдельные визуальные записи.
6. Breach может запустить постоянный emitter.

### 14.4 Update particles

1. Неположительный `deltaTime` пропускается.
2. Active emitter накапливает fractional spawn count.
3. Целая часть accumulator создаёт particles, дробная остаётся на следующий кадр.
4. Age каждой частицы увеличивается.
5. Position интегрируется через velocity умножить на time.
6. Gravity изменяет velocity.
7. Size и color интерполируются по нормализованному age.
8. Мёртвые particles удаляются компактным проходом.
9. Debug trails рисуются только по флагу.
10. Crack lifetimes обновляются отдельно.
11. `RebuildRenderItems` готовит presentation data после simulation.

### 14.5 SpawnParticle

1. Global maximum защищает память и draw cost.
2. Random unit vector формирует variation.
3. Cone смешивает forward с random vector.
4. Нулевое направление заменяется безопасным up.
5. Lifetime, speed, size и rotation выбираются в диапазонах descriptor.
6. Texture shape сохраняет path и atlas parameters.
7. Random start frame выбирается только при atlas frame count больше одного.

## 15 ParticleRenderer

1. Render items разделяются по texture и blend mode.
2. Для каждой particle строится camera-facing quad.
3. Right и up камеры масштабируются размером частицы.
4. Dynamic vertex buffer обновляется пачкой.
5. Alpha и additive states включаются явно для соответствующего batch.
6. Texture cache не загружает один файл каждый кадр.
7. После particles render state восстанавливается для следующего pass.

## 16 ExplosionSystem Emit

1. Radius ограничивается минимумом, impulse — нулём снизу.
2. Счётчик affected bodies начинается с нуля.
3. Visual explosion отправляется ParticleSystem независимо от физики.
4. Перебираются rigid bodies.
5. Null, static, kinematic и нулевая inverse mass отбрасываются.
6. Direction строится от центра взрыва к center of mass тела.
7. Тела дальше radius пропускаются.
8. Нулевая дистанция получает направление вверх.
9. `upwardBias` поднимает траекторию.
10. Falloff равен единице минус нормализованная дистанция.
11. Квадрат falloff делает край взрыва мягче.
12. Body активируется и получает central impulse.
13. Cross product создаёт torque axis для небольшого вращения.
14. Счётчик увеличивается только после фактического импульса.
15. Метод возвращает число затронутых тел для теста и debug UI.

## 17 HUD классы

`GameHud`, `GeneratorHud` и `OxygenGeneratorHud` должны только читать snapshots и рисовать.

1. UI получает player vitals или component telemetry.
2. Значения clamp-ятся для progress bars.
3. Цвет выбирается по thresholds.
4. Кнопка управления создаёт command/event, но не должна дублировать формулы GeneratorSystem.
5. Отсутствующий component показывает понятное состояние, а не разыменовывает null.

## 18 Что сохранить и что переименовать

Сохранить стоит room membership, atmosphere equalization, decompression, power grid, battery, repair, particles и alerts. Они не зависят от подлодки как жанра.

Переименовать со временем можно:

1. `ShipPowerGridSystem` в `PowerNetworkSystem`.
2. `SubmarineJointSystem` в `StructuralJointSystem`.
3. `HullPanelComponent` в более общий `DamageablePanelComponent`, если панели встречаются не только на корпусе.
4. `PressureSystem` в `ExternalPressureSystem` с нейтральным входом environment pressure.

Переименование следует делать отдельной механической миграцией, не одновременно с изменением поведения.

## 19 SubmarineSystem и WorldArea исторические helpers

`SubmarineSystem` оставлен в книге, потому что пользователь исключил затопление, а не весь сохранившийся structural runtime. Название историческое, поведение можно читать как составную конструкцию из отсеков.

### 19.1 AddCompartmentToSubmarine

1. Root обязан иметь TransformComponent и SubmarineComponent.
2. Ошибка возвращает INVALID_ENTITY до создания частичной сущности.
3. Создаётся entity отсека.
4. Transform получает позицию root; local offset хранится в geometry компонента.
5. Local min/max строятся как половины размера со знаками.
6. `localCenter` получает offset.
7. Volume пересчитывается, текущий размер кешируется.
8. Child SubmarineComponent ссылается на root.
9. Active tag делает entity участником scene/runtime.
10. Entity добавляется в `allCompartments` root.
11. `needsPhysicsUpdate` откладывает дорогой rebuild до update.

Параметр `compartmentMass` сейчас не используется — это явный долг API, а не скрытая физика.

### 19.2 RecalculateSubmarineMass

1. Требуются root rigid body, buoyancy и group component.
2. Volume суммируется по существующим compartments.
3. Историческая константа корпуса добавляет фиксированный объём.
4. Neutral mass вычисляется как density умножить на volume.
5. Коэффициент 0.95 делает массу чуть меньше нейтральной.
6. RigidBody mass и Buoyancy volume обновляются вместе.
7. Итоги кешируются в group component.

Для общего structural assembly формулу плавучести следует вынести из группировки, иначе космический корабль продолжает зависеть от плотности воды.

### 19.3 RefreshSubmarinePhysics

1. Старые linear и angular velocities сохраняются.
2. Body удаляется из Bullet world.
3. Motion state и body освобождаются.
4. Масса пересчитывается.
5. PhysicsSystem создаёт новый body.
6. Сохранённые velocities возвращаются, чтобы rebuild не останавливал объект.

### 19.4 NeedsPhysicsUpdate и UpdateSubmarine

1. Явный dirty flag даёт немедленный true.
2. Изменение количества compartments тоже требует rebuild.
3. Каждый compartment сообщает изменение размера относительно cache.
4. Update при false сразу выходит.
5. После refresh dirty flag снимается.
6. Count и размеры кешируются только после успешного обновления.

### 19.5 WorldAreaComponent

World area — data-only классификация области. Type сообщает смысл зоны, bounds/transform задают объём, enabled разрешает участие. Systems должны читать этот компонент как описание и не прятать внутри него pointers на Bullet или renderer resources.

