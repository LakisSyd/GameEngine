# Инженерные практики для развития проекта

Эта часть добавляет темы, которые становятся важнее по мере роста движка. Они не создают новую видимую механику за один вечер, но не дают механикам превратиться в неуправляемый монолит.

## 1 Контракт до класса

Перед новым файлом ответь письменно.

| Вопрос | Пример для stamina |
|---|---|
| Кто владеет данными | `StaminaComponent` на персонаже |
| Кто меняет значение | `StaminaSystem` |
| Кто только читает | movement UI tactical decision |
| Когда обновляется | после intent до выбора фактической скорости |
| Какая единица | нормализованный диапазон или points |
| Какие инварианты | `0 <= current <= maximum` |
| Как сохраняется | authoring maximum да runtime current для save game |
| Как отлаживается | current drain reason regen delay |

Если два класса заявляют право окончательно писать одно поле, контракт ещё не готов.

## 2 Правило одного источника истины

У оружия magazine живёт в одном `WeaponComponent`. У комнаты pressure живёт в одном `RoomComponent`. У dynamic body физическая позиция после fixed step живёт в Bullet и копируется в Transform. Кэш допустим, если он помечен как derived и имеет понятную процедуру invalidation.

Плохой признак: баг исправляют синхронизацией двух почти одинаковых полей каждый кадр. Обычно лучше удалить одного владельца.

## 3 RAII и внешние библиотеки

DirectX и Bullet часто возвращают raw pointers, но это не означает отсутствие владельца. Для каждого runtime ресурса в компоненте или manager запиши destroy sequence.

```text
Bullet body
    removeRigidBody from world
    delete motion state
    delete body
    delete shape children
    delete shape
    delete backing triangle mesh
```

```text
D3D resource
    release references held by views
    release view
    release buffer or texture
```

Когда возможно, используй `unique_ptr` с custom deleter или небольшой wrapper. Если component остаётся move only, ECS уже имеет move overload `AddComponent`.

## 4 Инварианты сильнее комментариев

Комментарий сообщает намерение, но проверка ловит нарушение. Полезные debug assertions:

```cpp
assert(component.maximum >= 0.0f);
assert(component.current >= 0.0f);
assert(component.current <= component.maximum + epsilon);
```

В release код всё равно должен безопасно обрабатывать внешние данные из JSON. Assert помогает разработчику, validation защищает пользователя и save file.

## 5 Guard clauses

Ранний выход делает основной путь плоским.

```cpp
if (!world)
    return;
if (!registry.IsAlive(entity))
    return;
auto* transform = registry.GetComponent<TransformComponent>(entity);
if (!transform)
    return;

// основной путь начинается здесь
```

Не все ошибки равны. Ожидаемое отсутствие optional component допускает тихий return. Нарушение обязательного инварианта должно оставить diagnostic reason, assert или error log.

## 6 Именование величин и единиц

Добавляй единицу в имя там, где ошибка опасна: `reloadSeconds`, `outputMw`, `energyMwh`, `angleRadians`, `distanceMeters`. `value`, `speed` и `time` слишком легко смешать.

| Величина | Хорошее имя | Типичная ошибка |
|---|---|---|
| угол | `yawRadians` | передать degrees в `sin` |
| длительность | `cooldownSeconds` | уменьшать на milliseconds |
| мощность | `requiredMw` | считать её запасом энергии |
| квадрат расстояния | `distanceSquared` | сравнить с обычным radius |
| нормализованная доля | `powerRatio` | принять за проценты 0..100 |

## 7 Координатные пространства как часть типа

Имя `position` не сообщает пространство. Для сложного кода используй `worldPosition`, `localOffset`, `viewDirection`, `modelNormal`. Особенно это важно для muzzle, IK, picking ray и joint frames.

Минимальный комментарий перед формулой должен сообщать переход: local muzzle point умножается на bone matrix, затем на entity world matrix и становится world origin выстрела.

## 8 Численная устойчивость

Основные правила проекта:

- проверяй длину до нормализации;
- clamp аргумент `acos` в `[-1, 1]`;
- сравнивай squared distance со squared threshold;
- ограничивай большой `deltaTime` для простой симуляции;
- используй epsilon только с понятной единицей;
- проверяй `isfinite` на границе физики и файлов;
- не дели на количество элементов до проверки нуля.

Epsilon `0.0001` не является универсальной магией. Для позиции в метрах и для normalized dot это разные масштабы ошибки.

## 9 Детерминизм и случайность

Random влияет на spread, wandering, effects и utility tie break. Для воспроизводимого теста используй seed, который сохраняется в debug run.

```text
seed + sequence of calls + same inputs -> same decisions
```

Если разные системы используют один глобальный generator, добавление случайной частицы может изменить AI. Лучше иметь отдельные streams или deterministic sampler по Entity и event index.

## 10 State machine и причина перехода

Храни не только state, но и `lastTransitionReason` для debug. Переход должен выполняться в одном месте или через функцию.

```cpp
void SetState(NpcBrainComponent& brain, NpcState next, std::string reason)
{
    if (brain.state == next)
        return;
    brain.previousState = brain.state;
    brain.state = next;
    brain.lastTransitionReason = std::move(reason);
}
```

Так журнал показывает факт, а не пытается восстановить причину по текущим полям спустя несколько кадров.

## 11 Decision и execution

Decision выбирает цель и намерение. Execution проверяет возможность и выполняет. Результат execution возвращается как success, running или failure reason.

```text
tactical module -> MoveToCover intent
navigation      -> path or no path
movement        -> displacement intent
Bullet          -> actual position
feedback        -> reached blocked stuck
```

Если decision каждый кадр перезаписывает intent до получения feedback, NPC дёргается между решениями.

## 12 Cooldown hysteresis и commitment

Cooldown ограничивает частоту повторной оценки. Hysteresis требует, чтобы новый вариант был заметно лучше текущего. Commitment запрещает смену на короткой фазе, например резкое выглядывание.

```text
switch when
newScore > currentScore + hysteresis
and decisionCooldown <= 0
and not in committed action
```

Эти три механизма решают разные проблемы. Один огромный timer скрывает симптомы, но делает AI медленным.

## 13 Ring buffer журналов

Журнал решения ограничивается последними N событиями. Поля события: world time, Entity, category, previous state, next state, reason и важные числа.

Не форматируй всю строку заранее, если UI может переключать язык. Храни структурированные поля и локализуй представление.

## 14 Structured debug snapshot

Inspector не должен читать десятки компонентов несогласованно и придумывать смысл. Система может публиковать компактный snapshot.

```text
NpcDebugSnapshot
    state
    target and visibility
    tactical order and reason
    move target and path status
    weapon ammo cooldown reload
    cover candidate rejection counts
```

Snapshot является derived debug data и не участвует в решениях.

## 15 Тестовая пирамида движка

| Уровень | Пример | Что ловит |
|---|---|---|
| чистая функция | cone direction limited flow utility score | математику и границы |
| component system | PowerSystem на маленьком registry | порядок чтения и записи |
| scenario | два NPC и одна стена | интеграцию нескольких систем |
| soak | 30 минут симуляции | утечки рост журнала редкие застревания |
| save load | round trip сцены | пропущенные поля и remap |
| visual regression | shader test scene | presentation изменения |

Не каждый тест должен запускать DirectX window. Чистые правила выноси в функции без renderer и world, чтобы они проверялись быстро.

## 16 Arrange act assert

```text
Arrange
    создать registry и минимальные компоненты
Act
    вызвать одну систему с известным dt
Assert
    проверить поля и события
```

В Arrange не копируй целую игровую сцену. Чем меньше обязательных данных, тем лучше тест показывает контракт системы.

## 17 Golden scenario для NPC

Создай несколько фиксированных сцен.

- прямая видимость без укрытий;
- низкая стена и высокая стена;
- два подходящих укрытия с разной дистанцией;
- цель исчезает за углом;
- empty magazine во время движения;
- friendly в firing lane;
- defender и assault profile;
- входящий near miss без урона;
- повреждение во время exposed phase;
- путь временно недоступен.

Для каждого сценария запиши ожидаемую последовательность состояний, а не только конечную позицию.

## 18 Property tests для математики

Полезные свойства:

- нормализованный ненулевой vector имеет length около единицы;
- dot normalized vectors находится в `[-1,1]`;
- ray hit fraction находится в `[0,1]`;
- `LimitedFlow` не переносит больше половины разницы за один парный обмен;
- utility score finite и ограничен заданным диапазоном;
- matrix inverse возвращает исходную точку в пределах epsilon;
- power supplied не превышает generation;
- сумма transfer между комнатами сохраняется без утечки до clamp.

## 19 Save version и migration

Добавь номер schema. При загрузке старой версии применяй migration defaults. Не определяй версию по отсутствию случайного поля, если формат уже развивается.

```text
version 3
    add faction = Neutral when absent
version 4
    convert reloadTimeMs to reloadSeconds
version 5
    split coverPoint into cover and peek positions
```

После migration runtime resources всё равно пересоздаются обычным load pipeline.

## 20 Атомарное сохранение

Безопасная запись:

1. записать временный файл рядом;
2. flush и закрыть;
3. проверить что JSON читается;
4. заменить основной файл;
5. при необходимости сохранить backup.

Падение во время прямой перезаписи не должно уничтожать единственный save.

## 21 Совместимость компонентов

При добавлении обязательного компонента старые сцены его не имеют. Выбери одну политику: default при load, editor migration tool или система `EnsureComponent`. Не размазывай три разных default по Inspector, load и runtime.

## 22 Профилирование до оптимизации

Сначала измеряй время систем и количество элементов.

```text
Frame
    AI 2.1 ms for 84 NPC
    Physics 3.8 ms for 612 bodies
    Render preparation 1.4 ms for 2300 candidates
    GPU 7.6 ms
```

Среднее скрывает spikes. Записывай median, 95 percentile и maximum. Отдельно считай query counts: raycasts NPC, cover candidates, path rebuilds и draw calls.

## 23 Пространственное ускорение

Если каждый NPC проверяет каждого кандидата, стоимость растёт квадратично. Spatial hash или grid уменьшает набор соседей.

```text
cell = floor(position / cellSize)
query radius -> neighboring cells
exact distance -> final filter
```

Grid только создаёт кандидатов. Точная проверка distance, FOV и raycast всё равно выполняется после.

## 24 Бюджетирование AI

Не все решения требуют 60 обновлений в секунду.

| Слой | Частота пример | Причина |
|---|---|---|
| движение controller | каждый simulation frame | плавность и контакт |
| weapon cooldown | каждый simulation frame | точное время |
| target candidate scan | 5 или 10 Hz | дорогая пространственная оценка |
| cover rescore | по событию и cooldown | стабильность решения |
| дальний idle NPC | редкий tick | simulation LOD |

Распределяй NPC по фазам, чтобы все не делали дорогой scan в одном кадре.

## 25 Memory и invalidation

Raw pointer на component действителен только пока pool не изменён способом, который перераспределяет storage, и пока Entity жива. Не сохраняй component pointer между кадрами без контракта контейнера. Сохраняй Entity и повторно получай pointer после `IsAlive`.

Модель, Bullet object и D3D resource имеют отдельные жизни. Смена сцены обязана инвалидировать caches, debug selection и async requests предыдущей сцены.

## 26 Ошибки фоновых задач

Worker error должен стать состоянием задачи, а не исчезнуть в debug output. Main thread получает success или structured error и может показать asset path, stage и message.

При закрытии сцены long job либо отменяется, либо результат проверяет generation token и не применяется к новому миру.

## 27 Generation token

```text
sceneGeneration = 12
job captures 12
scene reload increments to 13
job completes with 12
main thread rejects stale result
```

Этот простой механизм предотвращает применение старой async геометрии к Entity новой сцены с совпавшим ID.

## 28 Dependency direction

Желательное направление:

```text
Core and math
    <- ECS data
        <- gameplay rules
            <- presentation and editor
```

Renderer может читать gameplay snapshot, но gameplay не должен включать EditorUI для принятия решения. Debug hooks оформляй интерфейсом или данными, а не обратной зависимостью.

## 29 Модульная тактика NPC

Каждый tactical module реализует общий контракт.

```text
CanEvaluate(context)
Score(context)
BuildIntent(context)
Explain(context)
```

Context является read only snapshot восприятия, оружия, расстояния, stamina и укрытий. Module не двигает NPC напрямую. Arbiter выбирает intent, execution layers выполняют.

Новые модули: hold distance, assault, retreat, suppress, flank, reload safety, investigate sound, protect ally. Групповая координация может добавлять context facts, но не должна быть обязательной для одиночного NPC.

## 30 Capability компоненты

Не раздувай один `NPCComponent` сотней bool. Возможности оформляй компонентами или data profiles.

```text
CanUseCoverComponent
CanLeanComponent
StaminaComponent
HearingComponent
SquadMemberComponent
MedicCapability
```

Отсутствие компонента означает отсутствие возможности. System перебирает только нужный pool.

## 31 Data driven параметры

Порог дистанции, время exposed phase и скорость stamina не должны быть случайными literals в нескольких системах. Собери profile с понятными единицами и validation. Editor меняет profile, runtime читает его.

Баланс параметров не заменяет алгоритм. Если NPC меняет укрытие каждый две секунды из-за отсутствия hysteresis, увеличение таймера лишь маскирует причину.

## 32 Телеметрия решений

Для каждого дорогого выбора полезны counters.

- candidates total;
- rejected by distance;
- rejected by protection;
- rejected by path;
- rejected reserved;
- chosen score;
- evaluation time microseconds;
- time since last change;
- execution failure reason.

Так видно, что NPC не тупит, а все кандидаты отвергнуты конкретным фильтром.

## 33 Локализация журналов

Храни event id и параметры, например `CoverRejectedLowProtection` и `protectionHeight=0.82`. Русский и английский UI форматируют их отдельно. Если хранить только готовую английскую строку, перевод быстро станет неполным.

## 34 Комментарии которые помогают

Хороший комментарий отвечает почему, указывает единицы или контракт внешней библиотеки.

```cpp
// Bullet transform хранит центр капсулы, а ECS позиция находится у ног.
```

Плохой комментарий повторяет синтаксис.

```cpp
// Увеличиваем i на один.
++i;
```

После изменения поведения обновляй комментарий вместе с кодом. Устаревшее объяснение опаснее отсутствующего.

## 35 Маленький рефакторинг

Безопасная последовательность:

1. зафиксировать scenario и debug output;
2. вынести чистую функцию без изменения результата;
3. сравнить поведение;
4. вынести данные;
5. сравнить снова;
6. только потом менять правило.

Не совмещай перенос архитектуры, новый алгоритм и баланс параметров в одном непроверяемом шаге.

## 36 Definition of Done

Механика готова, когда:

- работает основной путь;
- отказ имеет понятную причину;
- runtime resources удаляются;
- save load сохраняет нужные данные;
- Inspector показывает важные поля;
- есть минимальный scenario test;
- стоимость измерена;
- комментарии объясняют нетривиальный контракт;
- старые сцены либо совместимы, либо мигрируются;
- место расширения записано в атласе.

## 37 Рекомендуемый порядок развития проекта

1. Закончить наблюдаемость: единый журнал, counters, debug rays и state reasons.
2. Закрепить golden scenarios NPC и физики.
3. Ввести profiles и единицы для разбросанных параметров.
4. Довести save version и migration.
5. Разделить длинные системы по decision, query и execution.
6. Добавить stamina как учебный вертикальный срез.
7. Добавить lean и crouch как capability и animation action.
8. Добавить hearing и investigate sound.
9. Добавить новые одиночные tactical modules.
10. Только после стабильного одиночного AI подключать squad coordination.

## 38 Практикум по самостоятельной разработке

### Задание 1 Stamina

Самостоятельно опиши component, system order, units, invariants, debug fields и save policy. Код начинай только после схемы.

### Задание 2 Physics ownership

Для двери, ящика, NPC и отсека заполни таблицу: тип body, владелец Transform до и после fixed step, group, mask, cleanup.

### Задание 3 Scenario test

Создай тест: NPC идёт к новому укрытию, стреляет в движении, начинает reload, доходит, прячется и только потом выглядывает. Запиши ожидаемые состояния по времени.

### Задание 4 Save migration

Придумай старую версию `WeaponComponent` без reserve ammo и новую с reserve ammo. Опиши migration и default.

### Задание 5 Performance

Добавь counters raycast, cover query и path rebuild. Сравни два NPC и двадцать NPC, не меняя алгоритм.

## 39 Финальный чек лист разработчика

- Я знаю владельца каждого изменяемого поля.
- Я знаю единицу каждой физической величины.
- Я могу назвать порядок системы в кадре.
- Я отделяю intent от подтверждённого факта.
- Я не сохраняю runtime pointers.
- Я удаляю внешний ресурс до уничтожения его владельца.
- Я могу воспроизвести случайный тест через seed.
- Я вижу причину отказа в журнале.
- Я измеряю прежде чем оптимизировать.
- Я изменяю одно правило за проверяемый шаг.

Эти привычки полезнее любого отдельного паттерна. Они позволяют добавлять новые механики как модули, а не как новые ветви одного бесконечного `Update`.

