# Построчный разбор ядра времени задач файлов и ресурсов

Эта глава проходит путь от `WinMain` до готового GPU mesh. Она дополняет прежний разбор `ApplicationClass`: здесь подробно разобраны окружающие его службы, которые раньше были только пунктами архитектурной карты.

## 1 main cpp

`WinMain` является точкой входа Windows приложения.

1. Параметры `HINSTANCE` и командной строки передаёт Windows. Текущий код не использует старый instance и строку запуска, но сохраняет стандартную сигнатуру.
2. Создаётся локальный `SystemClass system`. Его время жизни охватывает весь запуск программы.
3. `system.Initialize()` последовательно создаёт окно, ввод и `ApplicationClass`.
4. Если инициализация вернула `false`, выполнение завершается без входа в цикл.
5. `system.Run()` держит message loop и кадры до `WM_QUIT` или отказа `Frame`.
6. `system.Shutdown()` безопасно повторяет очистку. Деструктор также вызывает `Shutdown`, поэтому операция обязана быть идемпотентной.
7. Возвращаемое значение сообщает Windows код завершения процесса.

Главное: `main.cpp` не должен знать о сцене, NPC или renderer. Он владеет только корневым объектом приложения.

## 2 SystemClass Initialize построчно

Файл `src/Core/SystemClass.cpp` является мостом WinAPI и объектной части движка.

### 2.1 Глобальный мост

`SystemClass* g_applicationHandle = nullptr` хранится в безымянном namespace. Внешние translation units не видят символ.

Глобальная `WndProc` нужна потому, что Windows вызывает C-style callback. Она не может напрямую вызвать нестатический метод без адреса объекта. `g_applicationHandle` предоставляет этот адрес.

### 2.2 Конструктор и деструктор

Default constructor полагается на инициализаторы полей заголовка. Деструктор вызывает `Shutdown`. Поэтому любой ресурс обязан либо быть пустым, либо корректно переживать повторную очистку.

### 2.3 Начало Initialize

`Shutdown()` в первой строке делает повторную попытку детерминированной. Если предыдущая инициализация завершилась на середине, старые окно и подсистемы не смешаются с новыми.

`screenWidth` и `screenHeight` создаются нулевыми. `InitializeWindows` заполняет их фактическим размером.

Проверка `!m_hwnd` отделяет ошибку окна от ошибки Direct3D. Повторный `Shutdown` очищает зарегистрированный класс окна и глобальный bridge.

### 2.4 Ввод

`std::make_unique<InputClass>()` создаёт единственного владельца ввода. `Initialize` приводит массивы клавиш и mouse state в начальное состояние.

`unique_ptr` важен при раннем выходе: если последующая графическая инициализация не удастся, `reset` уничтожит input без ручного `delete`.

### 2.5 Application

Следующий `make_unique` создаёт `ApplicationClass`. Его `Initialize` получает размеры и `HWND`, потому что swap chain привязывается к окну.

Проверка результата не пытается продолжать с частично готовым renderer. Она вызывает полную обратную очистку.

`SetInput(m_Input.get())` передаёт невладеющий указатель. `ApplicationClass` пользуется вводом, но не удаляет его. Владелец остаётся `SystemClass`.

## 3 SystemClass Shutdown

`m_Application.reset()` выполняется раньше input и окна. Это соответствует зависимостям: Application освобождает swap chain, ImGui и ресурсы, которым ещё нужны `HWND` и контекст.

`m_Input.reset()` уничтожает input после прекращения кадров.

`ShutdownWindows()` восстанавливает display mode, уничтожает `HWND`, снимает регистрацию window class и обнуляет bridge.

Повторный вызов безопасен, потому что каждая операция проверяет наличие ресурса и обнуляет поле после освобождения.

## 4 SystemClass Run

`MSG msg = {}` обнуляет WinAPI структуру. `done` является единственным условием внешнего цикла.

`TimeManager::UpdateFrame()` вызывается в начале каждой итерации, даже если в очереди есть сообщение. Поэтому unscaled time продолжает измеряться во время UI и pause.

`PeekMessage(... PM_REMOVE)` неблокирующе забирает одно сообщение. В отличие от `GetMessage`, игра не ждёт пользовательского события перед следующим кадром.

В editor mode Space переключает `Running` и `Paused`. Эта ветвь намеренно недоступна в Play, где Space принадлежит Jump.

Клавиша K в неработающем времени ставит `StepMode` и вызывает `StepFrame`. На текущей итерации появляется фиксированный игровой delta.

`WM_QUIT` ставит `done`. `TranslateMessage` создаёт текстовые сообщения из клавиш, а `DispatchMessage` вызывает глобальную `WndProc`.

Даже при обработанном сообщении вызывается `Frame`. Это поддерживает непрерывный render. Если `Frame` возвращает `false`, цикл заканчивается.

`ResetStep` выполняется после кадра, чтобы фиксированный delta не протёк в следующую итерацию.

## 5 SystemClass Frame и сообщения

`TimeManager::DeltaTime()` возвращает масштабированное игровое время. Это значение передаётся в `ApplicationClass::Frame`.

Escape закрывает приложение только в editor mode. В Play та же клавиша сначала принадлежит action frame и может закрыть interaction panel.

`MessageHandler` переводит WinAPI события в состояние `InputClass`.

- `WM_KEYDOWN` и `WM_KEYUP` меняют массив клавиш.
- `WM_MOUSEMOVE` извлекает signed координаты из `LPARAM`.
- При captured mouse позиция не обновляется из сообщений: движение сэмплируется один раз за кадр. Это предотвращает очередь искусственных сообщений от recenter.
- В свободном режиме координаты сохраняются для UI и picking.
- Шесть сообщений кнопок мыши меняют соответствующие флаги.
- Неизвестные сообщения передаются `DefWindowProc`.

Глобальная `WndProc` сначала отдаёт сообщение ImGui. Если UI обработал его, gameplay bridge не получает повторное событие.

`WM_DESTROY` и `WM_CLOSE` публикуют `WM_QUIT`. Остальные сообщения идут в `SystemClass`, если bridge существует.

## 6 Создание окна

`InitializeWindows` сохраняет `this` в bridge и получает module handle текущего exe.

`WNDCLASSEX` заполняется явно:

- `CS_HREDRAW | CS_VREDRAW` просит перерисовку при изменении размеров;
- `CS_OWNDC` закрепляет device context за окном;
- callback указывает на глобальную `WndProc`;
- иконка, cursor и чёрный background получают стандартные WinAPI ресурсы;
- menu отсутствует;
- имя класса и размер структуры обязательны для регистрации.

`RegisterClassEx` регистрирует описание до создания окна.

Размер desktop читается через `GetSystemMetrics`. В fullscreen код меняет display settings и ставит позицию `(0,0)`. В оконном режиме желаемые 1920×1280 ограничиваются desktop и центрируются.

`CreateWindowEx` использует popup style без рамки. `WS_CLIPSIBLINGS` и `WS_CLIPCHILDREN` уменьшают лишнюю перерисовку пересекающихся окон.

После создания окно показывается, становится foreground и получает focus. Cursor остаётся видимым до явного captured режима.

## 7 TimeManager

### 7.1 Singleton

`Instance` содержит function local static object. Начиная с C++11 его первое создание thread-safe. Копирование и присваивание удалены, поэтому второго таймера создать нельзя.

### 7.2 Constructor

Поля получают нулевые delta, scale 1, состояние Running и сброшенный step flag.

`QueryPerformanceCounter` запоминает стартовую отметку. Без этого первый delta измерялся бы от нуля системы.

`QueryPerformanceFrequency` сохраняет число тиков high-resolution counter в секунду.

### 7.3 UpdateFrame

Новая отметка считывается в `currentTime`.

`CalculateDelta` вычитает прошлый counter и делит на frequency. Результат — реальные секунды.

`m_lastTime = currentTime` передвигает начало следующего интервала.

В Running `m_gameDt = realDt * m_timeScale`. В Paused и StepMode обычный update даёт ноль. StepMode получает ненулевое время только через явный `StepFrame`.

`m_unscaledDt = realDt` сохраняется независимо от pause. UI, cursor animation и диагностика могут продолжать обновляться.

### 7.4 StepFrame и ResetStep

`StepFrame` работает только в StepMode. Он записывает фиксированное значение, по умолчанию 1/60 секунды, и ставит `m_wasStepped`.

`ResetStep` после кадра возвращает delta к нулю и очищает флаг. Без него один запрос выполнял бы много simulation frames.

### 7.5 Риск SetTimeScale

Текущий setter принимает любое число. Безопасное развитие — отвергать `NaN`, отрицательный scale и ограничивать максимум. Иначе системы с обратным временем или бесконечностью нарушат таймеры.

## 8 GameTimer

`GameTimer` является независимыми часами конкретной игровой симуляции или Studio.

`Update` сначала заменяет нечисловой или отрицательный delta нулём. Если paused, итоговый delta равен нулю; иначе умножается на неотрицательный scale.

Результат накапливается в `m_totalSeconds`, а также возвращается вызывающему коду.

`SetTimeScale` проверяет конечность и насыщает отрицательное значение до нуля.

`SetPaused` меняет только флаг. Он не стирает total time.

`Reset` задаёт начальное total time и обнуляет delta. Это создаёт новую логическую сессию без пересоздания объекта.

Различие: `TimeManager` обслуживает корневую итерацию приложения, `GameTimer` — управляемую simulation clock внутри `ApplicationClass` и NPC Studio.

## 9 PhysicsWorld

### 9.1 Initialize

Проверка существующего `m_dynamicsWorld` не создаёт второй мир.

Локальные `unique_ptr` последовательно создают collision configuration, dispatcher, broadphase, solver, ghost callback и world. Пока инициализация не завершена, поля singleton не изменяются.

Gravity устанавливается в Bullet world. Solver parameters задают число итераций, split impulse, penetration threshold и ERP. Эти числа управляют устойчивостью контактов и constraints, а не игровым ускорением персонажа.

Ghost pair callback регистрируется в broadphase, чтобы character ghost objects получали overlapping pairs.

Только после полного успеха локальные владельцы перемещаются в поля. Внутренние невладеющие указатели Bullet остаются корректны, потому что перемещение `unique_ptr` не перемещает сами объекты.

### 9.2 Update

Отсутствующий world завершает метод.

Фиксированный timestep равен 1/60. Положительный `deltaTime` добавляется в accumulator.

Accumulator ограничивается 0.25 секунды. Это защищает от spiral of death после долгого зависания: движок не пытается отыграть бесконечное число старых шагов.

Цикл выполняет Bullet step, пока накоплено не меньше одного timestep. Каждый шаг уменьшает accumulator и увеличивает монотонный `stepIndex`.

После цикла `interpolationAlpha = accumulator / timestep`. Renderer может интерполировать между прошлым и текущим fixed state на дисплее быстрее 60 Гц.

### 9.3 Shutdown

Сначала из мира удаляются constraints в обратном порядке. Затем удаляются collision objects. Мир не владеет ECS objects, поэтому он только снимает регистрации.

После этого уничтожаются world, callback, solver, broadphase, dispatcher и configuration в порядке обратных зависимостей.

Accumulator, alpha и step index обнуляются для повторной инициализации.

## 10 JobSystem

### 10.1 Initialize

Worker count ограничивается хотя бы одним потоком. Если система уже работала, повторное создание не допускается.

`stopping` сбрасывается под mutex. Затем в vector создаются threads, каждый выполняет `WorkerLoop`.

Если создание потока выбрасывает исключение, система должна остановить уже созданных workers. Этот путь важно сохранять при рефакторинге.

### 10.2 SubmitJob

Под queue mutex проверяется `stopping`. Новая `std::function<void()>` кладётся в FIFO очередь.

После освобождения mutex `condition_variable.notify_one` будит одного worker. Уведомление вне lock уменьшает немедленную конкуренцию за mutex.

Возвращаемый `bool` сообщает, принята ли задача. `AssetLoadSystem` превращает отказ в `AssetState::Error`.

### 10.3 Main thread queue

`SubmitMainThread` кладёт callback в отдельную очередь под отдельным mutex.

`ExecuteMainThreadJobs` меняет общую очередь с локальной. Lock удерживается только на swap, а сами callbacks выполняются без mutex. Поэтому callback может поставить новую main-thread задачу без deadlock; она останется до следующего вызова.

### 10.4 WorkerLoop

Worker ждёт condition variable, пока не появится job или `stopping`.

Если stopping и очередь пуста, поток заканчивается. Если jobs ещё есть, он завершает накопленную работу.

Job извлекается под lock, затем исполняется без lock. После выполнения уменьшается active count и condition сообщает `Wait`.

### 10.5 Wait и Shutdown

`Wait` блокируется, пока очередь не пуста или есть active workers. Он не обязан выполнять main-thread callbacks: их должен качать главный поток.

`Shutdown` ставит stopping, уведомляет всех, join каждого thread, очищает containers и main-thread queue.

Опасность: job захватывает `this`. Владелец `AssetLoadSystem` обязан дождаться jobs до собственного уничтожения.

## 11 FileSystem

### 11.1 Constructor и root

Root path нормализуется через `weakly_canonical`. Ошибка хранится и выводится, а объект не должен выполнять операции вне корректного root.

### 11.2 ScanFileSystem

Старый cache очищается. Recursive directory iterator проходит root с `error_code`, чтобы ошибка разрешений не выбросила приложение.

Для каждой записи формируется `FileEntry`: full path, relative path, name, extension, directory flag и размер.

Extension переводится в lower case. Это делает поиск `.PNG` и `.png` одинаковым.

После прохода entries сортируются для стабильного UI. Стабильность важна: один scan не должен случайно менять selection.

### 11.3 Поиск

`FindByPath` нормализует разделители и сравнивает cached paths.

`FindByName` вызывает case-insensitive `MatchesName` и возвращает невладеющие указатели. Они действуют до следующего `ScanFileSystem`, который перераспределит vector.

`GetFilesByExtension` нормализует точку и регистр расширения.

`GetEntriesInDirectory` сначала безопасно разрешает relative dir внутри root, затем сравнивает parent path, а не простой prefix. Иначе `assets/a` ошибочно включал бы `assets/ab`.

### 11.4 ResolvePath

Абсолютный input отвергается. `root / relative` проходит `lexically_normal`, затем `weakly_canonical`.

`IsWithinRoot` сравнивает path components case-insensitive на Windows. Простая строковая проверка prefix небезопасна: `C:\game\assets2` начинается похоже на `C:\game\assets`.

Флаг `allowRoot` запрещает операции удаления над самим root.

### 11.5 DeleteFile и DeleteDirectory

Обе функции сначала получают безопасный resolved path. Ошибка resolution завершает операцию до filesystem mutation.

`DeleteFile` вызывает `fs::remove`, проверяет `error_code` и результат, затем удаляет entry из cache.

`DeleteDirectory` использует `remove_all`, после чего удаляет все cached entries внутри подтверждённого пути.

Эти операции принадлежат editor/file tooling, но сам boundary безопасности является runtime core и разобран полностью.

### 11.6 CreateDirectory

Resolved path проверяется внутри root. `create_directories` создаёт промежуточные каталоги. Если каталог уже существовал, функция всё равно может считать результат успешным через `exists`.

Cache получает directory entry только после подтверждённого filesystem результата.

## 12 AssetLoadSystem структуры

`AssetState` задаёт конечный автомат: `Unloaded → Loading → CpuLoaded → Ready` или `Error`.

`MeshCpuVertex` повторяет GPU input: position, UV, normal, tangent, binormal и четыре bone channels.

`MeshLoadTracker` использует atomics, потому что worker пишет progress, а main/UI читает. `memory_order_relaxed` достаточен для независимых диагностических чисел; готовность данных передаётся через main-thread queue.

`MeshGpuData` владеет сырыми COM buffers. `MeshAsset` объединяет path, state, CPU/GPU данные, error и shared tracker.

Поле `gpuModel` сейчас не участвует в показанном пути и остаётся legacy заготовкой. Это стоит либо удалить, либо оформить владельцем.

## 13 RequestMeshLoad построчно

Карта сначала ищет path.

Если asset уже существует и не Error, повторный запрос игнорируется. Если Error, GPU ресурсы освобождаются, запись удаляется и разрешается retry того же пути.

Новый `MeshAsset` получает path, Loading, null model и новый shared tracker. Перемещение в map устанавливает runtime запись до запуска worker.

`tracker` копируется отдельно, чтобы worker не хранил ссылку на map element, который может перераспределиться.

`SubmitJob` захватывает `this`, path и tracker. Worker вызывает `LoadMeshFromFile`, создавая только CPU данные.

После завершения worker ставит main-thread callback. Result перемещается в lambda capture, чтобы не копировать большие vectors.

Main-thread callback снова получает map asset. При failure записывает Error и строку. При успехе перемещает mesh, ставит CpuLoaded и progress 0.90.

Если job queue отвергла задачу, asset немедленно становится Error. Запись не зависает в Loading.

## 14 AssetLoadSystem Update и snapshots

`Update(device)` перебирает assets только на главном потоке. Для CpuLoaded вызывается `CreateGpuMesh`, потому что Direct3D device integration не оставляется worker без контракта.

Успех переводит state в Ready и progress 1. Ошибка переводит в Error и создаёт fallback message.

`GetLoadSnapshots` копирует только диагностические поля. Atomics читаются relaxed. Строка stage выводится из state, а не хранится как вторая потенциально противоречивая истина.

## 15 CreateGpuMesh построчно

Null device, пустые vertices или indices немедленно возвращают false.

Counts преобразуются в `uint32_t`. До production масштаба полезно проверить переполнение `size_t`.

Vertex descriptor задаёт `D3D11_USAGE_DEFAULT`, byte width и `D3D11_BIND_VERTEX_BUFFER`. Initial data указывает на непрерывный vector.

После успешного vertex buffer аналогично создаётся index buffer с 32-bit indices.

Если второй CreateBuffer падает, `ReleaseGpuMesh` освобождает первый. Asset не остаётся частично готовым.

После успеха CPU vectors очищаются и `shrink_to_fit` освобождает память. Это экономит RAM, но запрещает последующее CPU mesh editing для этого loader.

## 16 LoadMeshFromFile построчно

Progress 0.03 сообщает начало.

Локальный `Assimp::Importer` владеет scene до конца функции.

Flags триангулируют, генерируют normals и tangents, объединяют одинаковые vertices и применяют node transforms к геометрии. `PreTransformVertices` делает этот loader статическим: hierarchy и полноценный skinning здесь не сохраняются. Для анимированных моделей используется `AssimpModelClass`.

Scene, root и incomplete flag проверяются вместе. Error string копируется до уничтожения importer.

Total mesh count и progress 0.12 публикуются после validation.

Для каждого mesh запоминается `baseVertex`, потому что все meshes складываются в один общий vertex vector.

Каждая вершина value-initialized. Position копируется обязательно. UV, normal, tangent и bitangent копируются только при наличии channel; иначе остаются нули.

Четыре bone slots обнуляются. Из-за `PreTransformVertices` этот путь не загружает weights и предназначен для статической геометрии.

Face с числом индексов не равным трём пропускается, хотя triangulate должен был устранить такой случай. Три индекса получают `baseVertex`.

После каждого mesh atomics обновляют count и progress от 0.12 до 0.85.

В конце `success = true`. Пустая геометрия будет отклонена позднее `CreateGpuMesh`; улучшение — сообщить более точную ошибку здесь.

## 17 Bind и RenderMeshAsset

`BindMeshAsset` требует Ready и два buffers. Затем IA получает vertex buffer, stride, index buffer R32 и triangle list. Out count заполняется только при успехе.

`RenderMeshAsset` требует material и shader. Texture выбирается из slot 0 либо main texture.

Switch по `ShaderType` выполняет типизированный вызов Light или Texture shader. Для Light дополнительно передаётся selected factor и arrays lights.

Остальные shader types возвращают false. Это осознанное ограничение helper, а не универсальный renderer.

## 18 Shutdown AssetLoadSystem

Каждый asset освобождает GPU buffers. Затем map очищается.

Перед этим владелец обязан дождаться worker jobs: lambda захватила `this`. Сам `Shutdown` не вызывает `JobSystem::Wait`, потому что порядок владения определяется `ApplicationClass`.

## 19 WorldEventLog

`Push` добавляет `WorldEvent` в deque. Если превышен предел, старое событие удаляется спереди.

`GetEvents` возвращает const reference: UI читает, но не меняет историю.

`Clear` начинает новую сессию.

World event отличается от NPC debug event: это общий gameplay факт. Для масштабирования им нужны sequence, simulation time и typed payload вместо одной строки.

## 20 DebugHud

Singleton `DebugHud` хранит один краткий message, kind, duration и enabled flag.

`Show` заменяет текущий message и timer. Это deliberately last-message HUD, не журнал.

`Update` уменьшает timer только положительным delta. По нулю очищает текст.

`Render` ограничивает panel width и позицию экраном, выбирает цвет по kind, преобразует UTF-8 в UTF-16 и рисует через `DWriteTextSystem`.

`Utf8ToWide` сначала спрашивает нужную длину `MultiByteToWideChar`, затем выделяет string и выполняет conversion. Ошибка возвращает пустую строку.

## 21 Что исправлять первым в core

1. Привести повреждённые комментарии `TimeManager` и части runtime к UTF-8.
2. Добавить validation time scale.
3. Сделать lifetime barrier между AssetLoadSystem и JobSystem явным.
4. Заменить raw COM buffers в assets на `ComPtr`.
5. Разделить статический async loader и animated Assimp loader понятными именами.
6. Записывать точную stage/error при пустой модели.
7. Добавить unit tests path containment для FileSystem.

