# Gameplay runtime построчный разбор

Эта глава читает gameplay-код сверху вниз. Под «строкой» здесь понимается значимая операция: проверка, чтение компонента, формула, изменение состояния или вызов другой подсистемы. Скобки, `#include` и одинаковые служебные перегрузки объединены, иначе объяснение стало бы длиннее исходника и хуже показывало бы мысль.

## 1 GameplayController роль класса

`GameplayController` — не ещё один игрок и не место хранения мира. Это координатор локального gameplay-кадра.

1. Конструктор сохраняет ссылки на registry, input, camera и physics services. Он не должен становиться владельцем объектов, созданных другими подсистемами.
2. `Initialize` создаёт или находит локального игрока, подготавливает input bindings и приводит runtime-флаги к известному состоянию.
3. `Update` сначала получает снимок ввода, затем обновляет сессию, движение, взгляд, взаимодействие, оружие и ремонт.
4. Порядок важен. Нажатие, полученное в этом кадре, должно попасть в системы до очистки event queue.
5. Результаты взаимодействий и урона обрабатываются после producer-систем, но до `events.Clear()`.
6. Камера получает итоговое положение уже после движения character controller, иначе изображение отстаёт на кадр.

Главная опасность: добавлять в Controller собственную копию состояния, которое уже принадлежит компоненту. Controller должен передавать данные, а не создавать второй источник истины.

## 2 GameplayInputSystem bindings

### 2.1 IsSessionAction

`IsSessionAction(GameAction action)` отделяет команды самой игровой сессии от действий персонажа.

1. `switch` сравнивает action с pause, сменой камеры и масштабом времени.
2. Возвращаемый `true` разрешает обрабатывать действие даже тогда, когда обычный gameplay-ввод заблокирован UI.
3. Остальные действия получают `false` и не должны двигать или стрелять при захваченном интерфейсом вводе.

### 2.2 IsBindingDown

1. Проверка `binding.device` выбирает keyboard или mouse path.
2. Для клавиатуры вызывается состояние конкретной клавиши.
3. Для мыши `switch` переводит абстрактную кнопку binding в API `InputClass`.
4. Неизвестная кнопка возвращает `false`; это безопаснее случайного срабатывания.

### 2.3 Конструктор

Последовательные `AddBinding` задают таблицу управления по умолчанию. Здесь нет gameplay-логики: конструктор только связывает `GameAction` с физической кнопкой. Поэтому переназначение клавиш позднее сможет заменить таблицу, не переписывая движение и оружие.

### 2.4 Sample

1. Метод получает `playMode` и `gameplayEnabled` отдельно. Первый говорит, запущена ли игра, второй — можно ли сейчас управлять персонажем.
2. Для каждого action вычисляется `down`.
3. `pressed` получается как текущий `down` при прошлом `false`.
4. `released` получается обратным переходом.
5. Session actions сохраняются при блокировке gameplay, остальные принудительно выключаются.
6. В конце прошлое состояние обновляется для следующего кадра.

Такой edge detection нужен, чтобы reload или pause происходили один раз, а движение продолжалось всё время удержания.

### 2.5 UpdateSession

1. `Pressed(ToggleView)` меняет режим камеры только на фронте нажатия.
2. `Pressed(TogglePause)` переключает `GameTimer`.
3. Slower и Faster изменяют time scale в допустимом диапазоне.
4. Session update не должен напрямую менять компоненты оружия или движения.

## 3 CharacterSpawnSystem

Spawn собирает сущность персонажа из ECS-компонентов и Bullet-объектов. Destroy делает обратную операцию.

### 3.1 Spawn порядок

1. Проверяются registry и physics world.
2. Создаётся entity и базовые компоненты: transform, active tag, player или NPC data.
3. Создаётся capsule shape. Радиус и высота берутся из настроек персонажа, а не из render mesh.
4. Ghost object получает transform и collision flags.
5. Kinematic character controller связывается с ghost object.
6. Ghost и controller добавляются в Bullet world с нужными collision masks.
7. Указатели записываются в `CharacterController` component.
8. ECS transform синхронизируется с фактической стартовой позицией Bullet.

Если операция оборвётся после пункта 4, уже созданный Bullet-объект нужно освободить. Поэтому spawn следует рассматривать как транзакцию.

### 3.2 Destroy

1. `GetComponent<CharacterController>` проверяет, действительно ли entity имеет физического персонажа.
2. Получается world.
3. Controller сначала удаляется из world, потому что он ссылается на ghost object.
4. Ghost object удаляется после controller.
5. Shapes и heap objects освобождаются только после detach.
6. Затем уничтожается ECS entity.

Именно такой обратный порядок предотвращает висячие ссылки Bullet.

## 4 CharacterMovementSystem

### 4.1 Поиск локального игрока

1. `INVALID_ENTITY` означает, что подходящий entity ещё не найден.
2. Перебор player tags выбирает локального управляемого персонажа.
3. Ранний `return`, если игрока нет, делает систему пригодной для NPC-only симуляции.

### 4.2 Направление ввода

1. Forward/back и left/right превращаются в две оси.
2. Вектор нормализуется только если длина больше единицы. Диагональ поэтому не быстрее прямого движения.
3. `cameraYaw` поворачивает локальное направление в world space.
4. `isMoving` вычисляется по длине, а не по отдельной клавише.

### 4.3 Ходьба бег и stamina

1. `wantsRun` отражает желание игрока, но ещё не гарантирует бег.
2. `hasEnoughStaminaToRun` проверяет ресурс.
3. `isRunning` становится true только при движении, удержании run и достаточной stamina.
4. Скорость выбирается между walk и run.
5. При нехватке stamina флаг бега снимается, но `wantsRun` сохраняет причину для UI.
6. Итоговая скорость записывается в movement component; character controller исполняет её отдельно.

Разделение желания и факта особенно полезно для NPC: AI может хотеть спринт, а stamina policy запретит его.

## 5 CharacterControllerSystem общий Update

Это один из самых насыщенных runtime-файлов. Его верхний `Update` не просто перемещает capsule: он связывает ECS, Bullet, automatic step, опору, толкание динамических тел и восстановление после смены настроек.

1. Невалидный или отрицательный `dt` приводится к безопасному значению.
2. Система перебирает entities с controller и transform.
3. Проверяется существование `controller`, `ghostObj` и `capsuleShape`.
4. Если параметры physics tuning изменились, `Reconfigure` перестраивает controller.
5. Movement velocity переводится из метры в секунду в displacement для текущего шага.
6. External velocity прибавляет атмосферный толчок, взрыв или другой независимый импульс.
7. Automatic step может временно заменить обычный walk direction траекторией подъёма.
8. Bullet controller получает итоговый direction.
9. После physics шага transform считывает world transform ghost object.
10. Ground support и idle lock обновляются по фактическому контакту.

### 5.1 Reconfigure

1. Старые controller и ghost отсоединяются от world.
2. Текущая позиция сохраняется.
3. Capsule пересоздаётся с новыми radius, height и step height.
4. Collision flags и masks восстанавливаются.
5. Новый controller возвращается в прежнюю позицию.
6. Runtime traversal state сбрасывается, потому что старая ступень могла зависеть от прежней геометрии capsule.

### 5.2 PushDynamicBody

1. Sweep или ray определяет объект перед персонажем.
2. Static, kinematic и нулевая inverse mass отбрасываются.
3. Surface normal нормализуется.
4. `approach` показывает, действительно ли персонаж движется в поверхность.
5. Target push speed ограничивается tuning-параметрами.
6. Из текущей скорости тела вычитается уже имеющаяся скорость вдоль толчка.
7. Оставшаяся скорость переводится в impulse с учётом массы.
8. Impulse ограничивается, body активируется и получает воздействие.

Это не «телепорт коробки», а попытка довести её скорость до целевой, поэтому контакт устойчивее.

### 5.3 Automatic step detection

1. Сначала проверяется, включена ли функция и есть ли горизонтальное движение.
2. Forward sweep ищет препятствие на уровне capsule.
3. Нормаль должна смотреть достаточно против движения; касание боком не считается ступенью.
4. Верхний sweep проверяет свободное место над препятствием.
5. Down sweep ищет поверхность, на которую можно поставить персонажа.
6. Высота кандидата должна попасть между minimum и maximum step height.
7. Slope normal сравнивается с допустимым углом пола.
8. Predictive режим может начать traversal до непосредственного упора.
9. При успехе создаётся runtime traversal с start, apex, end и progress.

### 5.4 Traversal

1. Progress увеличивается через `dt / duration`.
2. Горизонтальная часть движется к конечной точке.
3. Вертикальная часть использует сглаженную кривую, а не мгновенный скачок.
4. Проверки свободного пространства продолжаются во время движения.
5. Отсутствие прогресса увеличивает stuck timer.
6. При блокировке traversal отменяется и запускает cooldown.
7. При достижении конца state очищается, обычное управление возвращается.

## 6 CharacterExternalVelocitySystem

Эта система — маленький сумматор внешних воздействий.

1. `BeginAtmosphereFrame` очищает накопление прошлого кадра.
2. Producer вызывает Add для entity: suction, взрыв или scripted force складываются.
3. Ограничение длины не даёт нескольким источникам разогнать персонажа до бесконечности.
4. `ConsumeVelocity` возвращает итог и удаляет запись.
5. Отсутствующая запись даёт нулевой вектор.

Важно вызывать Begin один раз до всех producers, а Consume — один раз внутри character movement. Иначе порядок систем меняет результат.

## 7 CharacterFacingSystem

1. Система пропускает мёртвых и неактивных персонажей.
2. Направление берётся из движения или отдельного aim direction.
3. Почти нулевой вектор не меняет ориентацию, чтобы idle-персонаж не дрожал.
4. `atan2` переводит XZ-направление в yaw.
5. `NormalizeAngle` возвращает разницу в диапазон от минус pi до pi.
6. Максимальный поворот равен turn speed умножить на `deltaTime`.
7. Transform получает новый yaw постепенно.

Нормализация нужна для короткого пути через границу 180 градусов.

## 8 FirstPersonViewSystem

1. Отсутствующая camera или local player завершает метод.
2. Transform игрока даёт базовую world position.
3. Camera height добавляет положение глаз.
4. Look yaw и pitch формируют forward vector.
5. Pitch ограничивается, чтобы камера не переворачивалась.
6. Camera получает position и rotation.
7. First-person arms получают отдельный transform, не совпадающий с world body mesh.
8. `UpdateBoneAttachments` читает итоговые bone matrices после animation update.
9. `XMMatrixDecompose` извлекает scale, rotation и translation attachment bone.
10. Ошибка decomposition оставляет прошлую корректную позу вместо мусора.

Порядок кадра: animation вычисляет pose, затем attachments читают кости, затем render рисует оружие.

## 9 PlayerVitalsSystem

### 9.1 Oxygen

1. Невалидный player и неположительный `deltaTime` отбрасываются.
2. `PlayerComponent` является владельцем текущего oxygen.
3. Система ищет комнату, содержащую игрока.
4. Если комната найдена, её oxygen и pressure определяют target.
5. Вне комнаты используется внешний environment policy.
6. Если текущее значение ниже target, оно плавно восстанавливается.
7. Если выше — плавно уменьшается.
8. Итог ограничивается от нуля до максимума.
9. Нулевой oxygen запускает потерю health через damage path, а не тайное прямое удаление entity.

### 9.2 StaminaUpdate

1. Проверяются player и movement components.
2. Реальный `isRunning`, а не только кнопка, тратит stamina.
3. Когда игрок не пытается бежать, stamina восстанавливается.
4. Во время удержания run без возможности бега можно оставить задержку восстановления.
5. Clamp защищает диапазон от нуля до maximum.

## 10 WeaponSystem

### 10.1 GetEquippedWeaponEntity

1. Сначала читается `EquippedWeaponComponent` owner.
2. Entity проверяется на alive.
3. Затем подтверждается наличие `WeaponComponent`.
4. Любая ошибка возвращает `INVALID_ENTITY`.

Так каждая следующая операция начинает работу с валидного оружия.

### 10.2 RequestReload

1. Находится экипированное оружие.
2. Reload запрещается, если уже идёт, магазин полон или резерв пуст.
3. Флаг `isReloading` включается.
4. `reloadTimeRemaining` получает длительность из weapon data.
5. Патроны пока не переносятся: перенос произойдёт при завершении таймера.

### 10.3 GiveAmmo

1. Неположительное число сразу возвращает ноль.
2. Ищется оружие owner.
3. Вычисляется свободное место в reserve.
4. Добавляется минимум между requested и capacity.
5. Метод возвращает реально принятое количество, чтобы pickup мог оставить остаток.

### 10.4 UpdateTimers

1. `dt` ограничивается снизу нулём.
2. Для каждого weapon уменьшаются shot cooldown и muzzle flash timer.
3. Reload timer уменьшается только при активной перезарядке.
4. При достижении нуля вычисляется недостающее место магазина.
5. Из reserve переносится минимум между нехваткой и доступными патронами.
6. Reload flags очищаются.

### 10.5 TryFire

1. Проверяется weapon, cooldown, reload и ammo.
2. Пустой магазин может запустить reload request.
3. Ammo уменьшается ровно один раз после подтверждения возможности выстрела.
4. Cooldown получает задержку между выстрелами.
5. Muzzle flash state включается на короткий срок.
6. Ray начинается у muzzle, а не в центре NPC.
7. Направление получает spread, после чего нормализуется.
8. Bullet raycast игнорирует shooter и его собственную геометрию.
9. Debug ray записывается только если включён соответствующий флаг.
10. При попадании создаётся DamageEvent с source, target, point, normal и silent flag.
11. Hull panel может получить отдельный mesh damage path.
12. Particle system создаёт muzzle flash и impact effect.

Важный контракт: AI решает, можно ли стрелять; WeaponSystem ещё раз проверяет физическую возможность и выполняет один выстрел.

## 11 WeaponFiringRules

Этот модуль держит чистые правила, которые полезны игроку и NPC.

1. `CanFire` проверяет cooldown, reload и ammo без изменения состояния.
2. `BeginReload` меняет только reload state.
3. `FinishReload` переносит ammo арифметически и возвращает число перенесённых патронов.
4. `AdvanceTimers` уменьшает timers с clamp до нуля.
5. Spread строится в локальном базисе forward-right-up.
6. Recoil возвращает величину отдачи, но не обязан сам двигать camera.

Чистая функция легче тестируется: одинаковый вход всегда даёт одинаковый выход.

## 12 PlayerPunchSystem

1. В начале требуются player, animation, mesh, camera и world.
2. Наличие оружия запрещает punch path, если дизайн не допускает melee с оружием.
3. Удержание primary mouse запускает или продолжает атаку.
4. При старте выбирается weighted animation variant.
5. `punchTime` увеличивается каждый кадр.
6. До contact time система только проигрывает движение.
7. В contact window строится ray или короткий sweep из камеры.
8. Собственный body игрока игнорируется.
9. `punchHitApplied` гарантирует один урон за одну анимацию.
10. Hit point может корректировать IK кисти, чтобы контакт визуально совпал.
11. После duration flags очищаются и рука возвращается в idle.

## 13 DamageSystem

### 13.1 Очередь

1. Система читает `DamageEvent` по порядку создания.
2. INVALID target, неположительный damage и неактивная entity отбрасываются.
3. Source и hit data сохраняются для AI perception и effects.
4. Silent kill не создаёт обычный шумовой сигнал, но сам урон остаётся.

### 13.2 Invulnerable и типы цели

1. `InvulnerableComponent` блокирует изменение health, но отладочные hit facts могут сохраниться.
2. Player path изменяет player health.
3. NPC path изменяет NPC или общий Damageable health.
4. Hull panel path обновляет damage decals, cracks или repairable state.
5. Переход alive в dead создаёт событие смерти только один раз.
6. Repairable получает накопленный structural damage.

Для AI важно различать «попадание зарегистрировано» и «здоровье уменьшилось». На бессмертной цели тактика всё равно должна понимать, что по NPC попали.

### 13.3 Decals

1. Hit point переводится в локальные координаты поверхности.
2. Ищется ближайшая существующая трещина.
3. Близкая трещина усиливается вместо создания бесконечного числа элементов.
4. При достижении лимита заменяется наиболее подходящий старый элемент.
5. Normal и tangent сохраняют ориентацию decal.

## 14 InventorySystem

### 14.1 Validate owner

1. Owner должен быть alive.
2. Inventory component создаётся только там, где это разрешает вызывающая операция.
3. Любой публичный метод возвращает `InventoryResult`, а не только bool. Status объясняет причину отказа.

### 14.2 AddWeapon

1. Проверяется owner, weapon entity и `WeaponComponent`.
2. Если weapon уже находится в inventory, возвращается existing slot.
3. Destination может быть конкретным или `AnySlot`.
4. Конкретный slot проверяется по диапазону и занятости.
5. Для AnySlot ищется первое свободное место.
6. Entity записывается в slot.
7. Ownership components оружия обновляются.
8. При первом pickup или `equipOnPickup` активный slot переключается.
9. `SyncEquipment` приводит EquippedWeaponComponent и visibility к inventory.

### 14.3 Remove Drop Select Swap

1. Remove проверяет slot и сохраняет item до очистки.
2. Active slot после удаления нормализуется.
3. Drop дополнительно возвращает предмету world transform и pickup component.
4. Select запрещает пустой или несуществующий slot.
5. Swap меняет две slot records и корректирует active index.
6. После каждой структурной операции вызывается единый equipment sync.

### 14.4 SyncEquipment

1. Определяется active item.
2. Owner получает или очищает EquippedWeaponComponent.
3. У активного оружия выключается world pickup physics и включается attachment visibility.
4. У неактивных inventory items отключается world render.
5. Dropped item, напротив, получает world render и collision.

Без централизованного sync один и тот же предмет легко окажется одновременно в руке и на полу.

## 15 WeaponPickupSystem

1. Если interactions в этом кадре не обрабатываются, метод ничего не делает.
2. Проверяются local player и его alive state.
3. Из EventQueue выбираются interaction events.
4. Target должен иметь pickup и weapon components.
5. `InventorySystem::AddWeapon` выполняет реальную операцию.
6. Успех удаляет или деактивирует world pickup representation.
7. Ошибка сохраняет feedback text для HUD.
8. `ConsumeInventoryChanged` возвращает одноразовый флаг и сразу его сбрасывает.

## 16 EventQueue

1. `Emit(T&& event)` помещает типизированное событие в variant vector.
2. `GetEvents` отдаёт read-only список consumers.
3. `Clear` вызывается один раз после всех consumers текущего кадра.

Producer не должен очищать очередь. Consumer не должен хранить ссылку на event после Clear.

## 17 InteractionSystem

1. Отсутствие camera, registry или world завершает update.
2. Ray строится из camera position по forward на interaction distance.
3. Bullet callback игнорирует player body.
4. Hit object переводится обратно в ECS entity.
5. Entity без InteractionComponent и без NPC data не считается целью.
6. Disabled interaction отбрасывается.
7. Точная дистанция перепроверяется по transform, чтобы чужой collider не расширял интеракцию бесконечно.
8. Prompt выбирается из component, NPC name или default текста.
9. Hover data сохраняет entity, point, normal и тип цели.
10. Нажатие Interact создаёт событие или открывает NPC UI.
11. Debug ray рисуется только при включённой настройке.

## 18 TriggerZoneSystem

### 18.1 Сбор кандидатов

1. `deltaTime` принимается только если конечен и положителен.
2. Собираются активные trigger zones.
3. Отдельно собираются player, NPC и dynamic body actors.
4. Actor получает mask своего типа.
5. Сортировка по Entity делает результат стабильным между запусками.

### 18.2 Contains

1. Sphere сравнивает квадрат дистанции с квадратом radius.
2. Box переводит world point в local coordinates зоны.
3. Обратная матрица проверяется по determinant; невырожденность обязательна.
4. Local point сравнивается с half extents.

### 18.3 UpdateZone

1. Disabled zone очищает или корректно завершает прошлые overlaps.
2. Для каждого actor проверяется mask и contains.
3. Новое пересечение создаёт Enter event.
4. Уже активное пересечение может создать Stay event.
5. Исчезнувшее пересечение создаёт Exit event.
6. Cooldown и one-shot policy решают, можно ли активировать action.
7. Runtime map хранит overlap отдельно для каждого actor.

### 18.4 Reset Rearm query methods

`Reset` очищает все runtime states. `ResetZone` очищает одну зону. `RearmZone` возвращает one-shot зоне способность активироваться и при необходимости очищает cooldown. Query methods только читают состояние и не должны неожиданно менять симуляцию.

## 19 TriggerActionSystem

1. Система читает только trigger events из EventQueue.
2. Event type сопоставляется с разрешёнными action phases.
3. `switch(action.action)` выбирает конкретную команду: visibility, active state, door, spawn или другая операция.
4. До изменения проверяются target entity и нужный component.
5. Feedback хранит последнюю успешную или неуспешную активацию для отладки.
6. `m_activationsThisFrame` сбрасывается в начале update и увеличивается только после фактического действия.

## 20 DoorSystem

### 20.1 ProcessEvents

1. Из очереди выбираются interaction и trigger actions для дверей.
2. Locked door отклоняет toggle.
3. Целевой `openAmount` меняется между нулём и единицей.
4. События `DoorOpenedEvent` откладываются до завершения перебора, чтобы не менять очередь во время чтения.
5. После цикла новые события добавляются безопасно.

### 20.2 Update

1. `openAmount` движется к target с заданной speed.
2. Transform двери вычисляется из закрытой позы и оси движения.
3. Collision state обновляется при прохождении порога.
4. Связанные комнаты используют openAmount как коэффициент обмена среды.

## 21 Правильный порядок gameplay кадра

Рекомендуемый порядок текущих систем:

1. Input sample.
2. Session actions.
3. Character intent: movement, facing desire, weapon request.
4. Environment producers и external velocities.
5. Character controller execution.
6. Physics step.
7. Transform sync и first-person attachments.
8. Interaction и trigger detection.
9. Weapon, punch, repair producers.
10. Damage processing.
11. AI perception получает факты попаданий и звуки.
12. Pickup и trigger action consumers.
13. Vitals.
14. Particle update.
15. Event queue clear.

Если producer работает после consumer, событие будет обработано только на следующем кадре. Иногда это допустимо, но должно быть осознанно.

## 22 Что улучшать в gameplay runtime

1. Разделить CharacterControllerSystem на ground probe, traversal, pushing и execution helpers без изменения порядка.
2. Сделать единый `HitFact`, который существует даже при invulnerability.
3. Покрыть WeaponFiringRules unit-тестами без D3D и Bullet world.
4. Явно документировать owner каждого input flag.
5. Добавить transactional guard в CharacterSpawnSystem.
6. Убрать скрытые static maps external velocity при переходе на несколько worlds.
7. Зафиксировать EventQueue phases, чтобы producers и consumers нельзя было случайно переставить.
8. Отделить first-person presentation от gameplay оружия окончательно.

