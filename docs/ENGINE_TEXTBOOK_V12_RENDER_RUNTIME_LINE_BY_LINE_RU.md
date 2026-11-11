# Построчный разбор Direct3D ресурсов моделей и render проходов

Предыдущие главы объясняют общий renderer и HLSL. Здесь разобраны вспомогательные runtime классы, которые создают Direct3D 11 устройство, текстуры, render targets, тени, 2D geometry, grass, text и конкретные shader wrappers.

## 1 D3DClass Initialize validation

Первая проверка отвергает null `HWND`, неположительные размеры, near plane не дальше нуля и far plane не дальше near. Без неё последующие DXGI calls могли бы вернуть менее понятную ошибку.

`Shutdown()` очищает предыдущий device. Локальный `InitializationGuard` вызовет повторный shutdown при любом раннем `return false`. Флаг `committed` ставится только в самом конце.

Локальные DXGI объекты используют `ComPtr`, поэтому временные factory, adapter, output и back buffer освобождаются автоматически. Основные долгоживущие поля класса пока остаются raw COM pointers.

## 2 Получение adapter и display mode

`CreateDXGIFactory` создаёт точку перечисления видеоустройств.

`EnumAdapters(0)` выбирает первый adapter. Это не всегда дискретная GPU; для расширения нужен явный выбор по памяти или preference.

`EnumOutputs(0)` выбирает первый monitor данного adapter.

Первый `GetDisplayModeList` с null array получает количество режимов. Vector изменяет размер. Второй вызов заполняет descriptors.

Цикл ищет mode с точными width и height и сохраняет refresh numerator/denominator. Последнее совпадение побеждает; при нескольких частотах лучше выбирать максимальную или текущую.

`GetDesc` возвращает dedicated video memory и UTF-16 имя. Память переводится в мегабайты, имя — в узкую строку через `wcstombs_s`.

## 3 Swap chain descriptor

Структура обнуляется, чтобы неинициализированные flags не меняли поведение.

`BufferCount = 1` создаёт старую discard swap chain с одним back buffer. Width, height и `R8G8B8A8_UNORM` задают изображение.

При vsync найденная refresh rate записывается в descriptor. Без vsync numerator 0 и denominator 1 означают unspecified.

`DXGI_USAGE_RENDER_TARGET_OUTPUT` разрешает использовать back buffer как RTV. `OutputWindow` связывает его с `HWND`.

MSAA отключён: sample count 1. `Windowed = !fullscreen` инвертирует пользовательский флаг.

`DXGI_SWAP_EFFECT_DISCARD` соответствует старому swap model. Более современный flip model уменьшает стоимость present, но требует отдельной миграции.

Feature level фиксируется на 11.0.

## 4 Device creation

`D3D11_CREATE_DEVICE_BGRA_SUPPORT` нужен для совместимости Direct2D с swap-chain surface.

Debug build добавляет `D3D11_CREATE_DEVICE_DEBUG`. Если debug layer не установлен, первый вызов может вернуть `DXGI_ERROR_SDK_COMPONENT_MISSING`.

Fallback удаляет debug flag и повторяет создание. Поэтому debug executable запускается и на машине без Graphics Tools.

`D3D11CreateDeviceAndSwapChain` заполняет четыре долгоживущих поля: swap chain, device, feature-level output и immediate context.

`DX_CHECK` прекращает инициализацию при failure. Guard очистит уже созданные поля.

## 5 Back buffer и depth buffer

`GetBuffer(0)` получает texture interface back buffer. Временный `ComPtr` удерживает его до создания RTV.

`CreateRenderTargetView` создаёт представление, которое Output Merger может привязать как цветовую цель.

Depth texture имеет те же размеры, один mip, один array slice, формат D24S8, sample count 1 и bind `DEPTH_STENCIL`.

Texture сама хранит память. Depth stencil view определяет, как pipeline интерпретирует эту память.

## 6 Depth states

3D state включает test и запись depth. `LESS` пропускает fragment, если его глубина ближе уже записанной.

Stencil включён и получает read/write mask 0xFF. Front/back operations увеличивают или уменьшают stencil при depth failure. Если stencil не используется осознанным алгоритмом, это лишняя работа; состояние стоит упростить.

Второй state выключает depth test для 2D. Поле write mask остаётся ALL, но при `DepthEnable = false` depth не участвует. Stencil всё ещё включён; это тоже можно пересмотреть.

`OMSetDepthStencilState` устанавливает 3D state по умолчанию.

Depth stencil view получает typed D24S8 format поверх texture.

`OMSetRenderTargets` одновременно привязывает color RTV и DSV.

## 7 Rasterizer blend viewport

Rasterizer использует solid fill, no culling, depth clipping и без MSAA/scissor. `CULL_NONE` рисует обе стороны треугольника и может скрывать ошибки winding, а также увеличивает нагрузку.

Alpha blend state использует стандартную формулу `src * alpha + dst * (1-alpha)` для RGB. Alpha channel берётся из source единицей и игнорирует destination alpha.

Viewport покрывает всё окно и диапазон depth `[0,1]`.

## 8 Матрицы D3DClass

Класс сохраняет размеры, near, far и FOV 75 градусов.

`XMMatrixPerspectiveFovLH` создаёт left-handed perspective matrix.

World matrix по умолчанию identity.

Ortho matrix строится off-center с origin в верхнем левом углу: x растёт вправо, y вниз. Это удобно для pixel coordinates UI.

Guard получает `committed = true`, поэтому destructor больше не вызывает cleanup.

## 9 D3DClass Shutdown

Immediate context сначала `ClearState` и `Flush`. Это отвязывает resources и отправляет накопленные команды.

RTV и DSV освобождаются раньше underlying depth texture и device.

Rasterizer, depth states и blend state release и обнуляются.

Swap chain принудительно переводится в windowed state перед release.

Context освобождается перед device. В debug build device запрашивает `ID3D11Debug` и печатает live objects; это показывает утечки COM ресурсов.

Повторный вызов безопасен за счёт null checks.

## 10 BeginScene EndScene и переключатели

`BeginScene` собирает четыре компонента clear color, очищает back buffer и depth до 1. Stencil не очищается, потому что flag содержит только `D3D11_CLEAR_DEPTH`. Если stencil реально используется между кадрами, требуется добавить clear stencil.

`EndScene` вызывает `Present(1,0)` при vsync и `Present(0,0)` без него.

`SetRenderDistance` отвергает NaN/Infinity, ограничивает far между near и 10000, не пересоздаёт матрицы при почти неизменном числе и обновляет perspective/ortho.

`SetBackBufferRenderTarget` восстанавливает main RTV/DSV после offscreen pass. `ResetViewport` восстанавливает размер окна.

`TurnZBufferOn/Off` меняют depth state. `TurnAlphaBlendingOn` привязывает созданный blend state, Off передаёт null и возвращает default no blending.

## 11 TextureClass

### 11.1 Initialize

`Shutdown` позволяет повторно загрузить объект.

`LoadImage` вызывает `stbi_load` с requested channels 4. Независимо от исходных channels возвращается RGBA memory. Отдельное output channels сообщает исходное число, но row pitch всегда использует четыре.

Ширина, высота и channels сохраняются.

Texture имеет `MipLevels = 0`, bind как shader resource и render target, misc `GENERATE_MIPS`. Render-target bind требуется Direct3D для автоматической генерации mip chain.

Format выбирается SRGB для color texture и UNORM для data texture. Metalness, roughness и normal не должны получать SRGB decoding.

`CreateTexture2D` создаёт пустую mip chain. При failure CPU image освобождается.

`UpdateSubresource` загружает mip 0. Row pitch равен width × 4 bytes.

SRV с `MipLevels = -1` охватывает всю chain. После создания `GenerateMips` заполняет нижние уровни.

CPU image освобождается ровно после upload.

### 11.2 Lifetime

Shutdown освобождает SRV раньше texture. GetTexture возвращает невладеющий pointer.

Copy constructor сейчас пустой. Это опасный контракт: копия становится объектом с неопределёнными полями. Копирование надо удалить либо реализовать shared resource semantics.

## 12 RenderTextureClass

Constructor обнуляет пять COM pointers. Destructor вызывает Shutdown.

Initialize выбирает format, сохраняет размеры и создаёт texture с двумя bind flags: RTV и SRV. Одна память сначала принимает render output, потом читается shader.

RTV указывает на mip 0. SRV также читает один mip.

Отдельная D24S8 texture и DSV дают корректную 3D глубину в offscreen pass.

Viewport соответствует texture, а не окну. Projection использует aspect texture, ortho — её physical size.

Текущий код не вызывает `Shutdown` в начале и не имеет rollback guard: failure после первого resource оставляет частичный объект до destructor. Улучшение — транзакционная инициализация как в `D3DClass`.

`SetRenderTarget` меняет Output Merger и viewport. После pass вызывающий обязан вернуть back buffer и main viewport.

`ClearRenderTarget` очищает color и depth. Getters возвращают SRV, matrices и размеры.

Shutdown освобождает view перед underlying texture.

## 13 ShadowMap

Depth texture создаётся typeless `R24G8`, потому что одна память имеет две интерпретации.

DSV видит её как `D24_UNORM_S8_UINT` для depth writing.

SRV видит depth bits как `R24_UNORM_X8_TYPELESS` для sample в pixel shader; stencil bits игнорируются.

Bind flags одновременно разрешают depth-stencil и shader resource.

Viewport соответствует shadow resolution.

`SetRenderTarget` передаёт ноль color targets и только DSV. Shadow pass пишет глубину без цвета.

`ClearRenderTarget` возвращает depth к 1.

`SetLightViewProjection` хранит две matrices для последующей передачи shader.

Перед привязкой SRV в main pass DSV должен быть отвязан, иначе Direct3D выдаст resource hazard.

## 14 ShadowShaderClass

Initialize компилирует shadow vertex/pixel файлы, создаёт shader objects, input layout и matrix buffer.

Хотя depth-only pass может обходиться без pixel shader, текущий wrapper сохраняет общий путь.

`Render` сначала вызывает `SetShaderParameters`; failure не продолжает draw.

Matrix buffer получает world, light view и light projection. Матрицы транспонируются или передаются в соответствии с HLSL соглашением класса.

`VSSetConstantBuffers` привязывает buffer к vertex stage.

`RenderShader` устанавливает input layout, VS/PS и вызывает `DrawIndexed(indexCount,0,0)`.

Shutdown освобождает собственный buffer и shader resources. Базовые ресурсы не должны освобождаться дважды.

## 15 BitmapClass

Класс представляет screen-space quad с динамическим vertex buffer.

Initialize сохраняет screen и bitmap sizes, создаёт buffers и при texture overload загружает texture.

Index buffer статичен: два треугольника всегда используют одинаковые индексы.

`SetRenderLocation` меняет screen x/y. `UpdateBuffers` вычисляет left/right/top/bottom относительно screen center, формирует шесть vertices и обновляет dynamic buffer только если позиция изменилась.

`Map(D3D11_MAP_WRITE_DISCARD)` отдаёт новую область для полного переписывания. `memcpy` копирует vertices, `Unmap` публикует их GPU.

`RenderBuffers` привязывает buffers и triangle list. Сам shader вызывается внешним кодом.

Shutdown освобождает texture, index и vertex buffers.

## 16 WorldSpriteClass

В отличие от Bitmap, sprite живёт в 3D мире и billboard-подобно строит world matrix.

InitializeBuffers создаёт quad заданной ширины/высоты и статический index buffer.

Position, scale и rotation setters помечают matrix dirty. Это избегает повторной математики, если transform не менялся.

`SetTransform` меняет все параметры одной операцией.

Render принимает view/projection, texture shader и color. Он вычисляет world matrix, привязывает geometry и вызывает shader.

Если sprite должен всегда смотреть на camera, rotation basis строится из camera direction. Если это обычная world plane, используется сохранённый rotation.

Класс содержит повреждённые комментарии кодировки; runtime поведение не страдает, но исходник надо перевести в UTF-8.

## 17 ModelClass legacy

`ModelClass` создаёт жёстко заданную или внутренне построенную geometry, загружает две textures и предоставляет index count.

`Initialize` вызывает InitializeBuffers, затем LoadTextures. Если textures не загрузились, buffers должны быть очищены.

`Render` только привязывает buffers. Lighting/material logic находится снаружи.

Две texture slots соответствуют старому multi-texture/light-map pipeline. Для GLB и PBR используется `AssimpModelClass`.

Класс является legacy path, но пока его вызывает renderer или тестовая geometry, его lifetime contract остаётся важным.

## 18 ModelManager

`LoadModel` сначала ищет string key. Повторная загрузка возвращает существующий pointer.

Новая модель создаётся, инициализируется device/context и textures, затем помещается в map.

При failure локальный объект уничтожается, а map не получает broken entry.

`GetModel` возвращает невладеющий pointer либо null.

`Shutdown` проходит map, вызывает Shutdown моделей, удаляет владельцев и очищает map. Если используются raw pointers, копирование Manager должно быть запрещено.

## 19 TinyGltfModelClass

Текущий `.cpp` почти пуст и не реализует самостоятельный runtime loader. Наличие файла не означает существующую систему.

Не следует описывать его как альтернативу Assimp, пока нет parse, buffer creation, materials и lifetime. Лучшее действие — либо удалить заготовку, либо дать ей явный experimental status.

## 20 GrassRenderer data

Grass renderer хранит shared quad geometry, instance buffers, shaders, constant buffers, sampler, blend/raster states и batches по texture/material.

`GrassInstance` содержит world position, rotation и scale. `GrassBatch` объединяет instances одной texture, GPU instance buffer и visible count.

Это instanced rendering: quad vertices не копируются для каждой травинки. Один draw использует общий mesh и множество instance records.

## 21 GrassRenderer Initialize

Null device/context отвергаются. Повторная инициализация очищает старые resources.

`InitializeShaders` компилирует VS/PS, создаёт input layout с per-vertex и per-instance slots, constant buffers, sampler и pipeline states.

Ошибка любой стадии вызывает Shutdown, чтобы не оставить половину renderer.

`InitializeBatch` загружает texture и создаёт dynamic instance buffer подходящей capacity.

## 22 Grass generation

`GenerateAreas` очищает batches и вызывает `AppendArea` для каждого `WorldAreaComponent` с grass settings.

Количество instances выводится из area size и density.

`std::mt19937 gen(area.seed)` делает размещение детерминированным. Одна сцена создаёт одинаковую траву между запусками.

Distributions выбирают x/z offset, rotation, scale и batch texture.

Max attempts больше target count, потому что не каждая случайная точка попадает на terrain.

`TryProjectToTerrain` посылает вертикальный Bullet ray в пределах area height. Hit point задаёт y и surface normal.

Фильтры могут отклонить слишком крутой slope или неподходящий collision object.

Принятая instance добавляется в CPU batch. После generation создаётся или обновляется GPU buffer.

## 23 Grass visibility и render

`UpdateVisibleInstances` проверяет distance/frustum и собирает только видимые records.

Dynamic instance buffer отображается с write discard и получает visible array.

`UpdateFrameBuffers` обновляет view/projection, camera, wind/time, light и ambient.

`Render` устанавливает shaders, input layout, sampler, blend/raster states и для каждого batch вызывает instanced draw.

`RenderBatch` привязывает vertex buffer в slot 0 и instance buffer в slot 1, texture SRV и выполняет `DrawIndexedInstanced`.

После прохода изменённые pipeline states и SRV должны быть восстановлены/отвязаны, чтобы grass не влиял на следующий material.

## 24 DWriteTextSystem

### 24.1 Factory и formats

`DWriteCreateFactory` создаёт shared DirectWrite factory.

`CreateTextFormat` задаёт family, weight, style, stretch, size и locale. Alignment и paragraph alignment настраивают baseline текста.

Основной и small format существуют отдельно.

### 24.2 Render target

`InitializeRenderTarget` создаёт Direct2D factory, получает DXGI surface swap chain и создаёт D2D render target с premultiplied alpha.

Brushes создаются на конкретном render target и должны пересоздаваться вместе с ним после device loss.

### 24.3 MeasureText

`CreateTextLayout` строит layout в заданном max box. Metrics возвращают width/height уже после font shaping и wrapping.

### 24.4 Draw calls

`DrawTextLineColor` меняет brush color и вызывает DrawText с нужным format.

Filled rect и outline используют D2D geometry primitives.

Bitmap loader создаёт WIC factory/decoder/frame/converter и D2D bitmap. Каждый промежуточный COM object имеет отдельное владение.

`BeginBatch` вызывает BeginDraw один раз перед серией HUD элементов. `EndBatch` вызывает EndDraw и обрабатывает `D2DERR_RECREATE_TARGET` как сигнал пересоздать target resources.

## 25 ParticleRenderer pipeline

ParticleRenderer получает уже подготовленные `ParticleRenderItem`, а не симулирует particles.

Initialize создаёт shaders и pipeline states. Geometry строится CPU как billboards и отправляется динамическими buffers.

Render вычисляет camera right/up из inverse view, группирует items по blend mode/texture, сортирует alpha particles по расстоянию и вызывает material batches.

`AppendBillboard` создаёт четыре world-space corners вокруг center, применяет rotation и size, записывает color/UV и шесть indices.

`ResolveTexture` кэширует SRV по path. Отсутствующая texture использует fallback white resource.

`RenderBatch` обновляет buffers, matrices, texture, sampler, blend/depth/raster states и вызывает draw indexed.

Additive particles не обязаны сортироваться так строго, alpha particles сортируются back-to-front.

Shutdown очищает texture cache, states, shaders, layouts и buffers.

## 26 Shader wrapper общий шаблон

Почти каждый старый shader class повторяет шесть фаз.

1. `Initialize` выбирает HLSL files.
2. `InitializeShader` компилирует blobs и создаёт VS/PS.
3. По VS blob создаётся input layout.
4. Создаются constant buffers и sampler.
5. `SetShaderParameters` map/unmap buffers и привязывает SRV.
6. `RenderShader` устанавливает layout/shaders и вызывает DrawIndexed.

При чтении конкретного wrapper надо искать только различия в данных и slots.

## 27 ColorShaderClass

Matrix buffer содержит world/view/projection. Color buffer содержит override color.

Перегрузка Render без color использует default, перегрузка с color передаёт выбранный цвет.

Это минимальный shader для debug geometry и одноцветных объектов. Texture SRV не нужен.

Метод называется `ShutDown`, тогда как base использует `Shutdown`. Из-за несовпадения это не override. Имена стоит унифицировать.

## 28 TextureShaderClass

Matrix buffer отправляется vertex stage. Texture SRV привязывается pixel stage t0. Sampler берётся из класса.

Wrapper не вычисляет lighting. Он выводит sampled texture.

После draw SRV полезно отвязывать перед использованием той же texture как render target.

## 29 LightShaderClass

Класс поддерживает arrays до `NUM_LIGHTS`, positions/colors, spot parameters, directional light, selected factor и shadow map.

`PrepareBeforeRender` обновляет общие light buffers один раз перед серией objects.

`SetShaderParameters` обновляет per-object matrices и texture.

`SetSelectedFactor` выделяет выбранный объект множителем.

`SetDirectionalLight` заполняет direction/color/ambient/intensity buffer.

`SetShadowMap` привязывает SRV, comparison sampler и light view-projection.

Публичный Render с большим числом parameters является признаком, что per-frame и per-object данные стоит разделить структурами.

## 30 PBRShaderClass

Initialize создаёт основной PBR pipeline и default 1×1 textures для отсутствующих normal, metalness и roughness.

`SetMatrixBuffers` обновляет b0.

Render получает mesh textures, material scalars, arrays lights, selected factor и bone buffer/model state. Missing textures заменяются default SRV.

`SetDirectionalLight`, `SetShadowMap` и `SetDamageDecals` обновляют независимые buffers/resources.

Damage buffer передаёт до ограниченного числа decals или cracks; crack texture хранится отдельно.

Environment map привязывается для отражения/ambient.

После skinned draw bone constant buffer b8 должен быть корректен или vertex shader должен выбрать static fallback.

## 31 NormalMapShaderClass

Input layout требует tangent и binormal кроме position/UV/normal.

SetShaderParameters передаёт matrix, diffuse/normal textures, light direction/color и camera-related данные.

Pixel shader восстанавливает world normal через TBN. Модель без tangents должна использовать fallback shader или importer-generated tangent space.

## 32 LightMap и MultiTexture

`LightMapShaderClass` привязывает base texture и light map в разные texture slots. Результат обычно перемножает albedo и baked light.

`MultiTextureShaderClass` привязывает несколько color textures и смешивает их по фиксированному правилу shader.

Оба используют общий matrix buffer и старый material path. Метод `ShutDown` снова не совпадает с base virtual name.

## 33 FogShaderClass

Matrix buffer трансформирует geometry. Fog buffer хранит start/end или другие параметры эффекта.

Vertex shader вычисляет factor по глубине, pixel shader смешивает texture и fog color.

Этот класс частично дублирует общий fog contract `ShaderBaseClass`. Стоит выбрать один источник buffer slots.

## 34 ClipPlaneShaderClass

Clip plane buffer содержит `float4 plane`.

VS вычисляет signed distance world point до plane и передаёт `SV_ClipDistance` либо pixel shader отбрасывает сторону.

`SetClipPlane` сохраняет plane для последующих draws. Render может принять plane явно.

Эффект нужен reflection/refraction passes, чтобы не рисовать geometry с неправильной стороны воды.

## 35 ReflectionShaderClass

Matrix buffer дополнен reflection matrix. Отдельные buffers передают blend/translation и reflect color.

Shader sample base texture и reflection texture, полученную offscreen pass.

Reflection matrix переводит world position в projective UV отражённой camera.

Класс зависит от правильного порядка: сначала reflection render target, затем main object.

## 36 WaterShaderClass

Water wrapper принимает reflection/refraction/normal textures, water translation, reflect/refract scale, tint и camera position.

Matrix buffer обрабатывает geometry plane. Reflection buffer даёт отражённую camera transform. Water buffer управляет distortion.

Хотя FloodingSystem исключён как исторический gameplay рудимент, visual water shader остаётся отдельным render effect и поэтому разобран.

Если вода больше не нужна направлению игры, класс можно пометить optional asset path, а не оставлять обязательной renderer зависимостью.

## 37 GlassShaderClass

Glass buffer хранит refraction scale, tint/opacity и padding. Render получает base texture, normal map и scene/refraction texture.

Pipeline должен включить alpha blending и решить depth write policy. Wrapper задаёт shader parameters, но global state находится в renderer.

Glass objects требуют сортировки после opaque pass.

## 38 ProjectionShaderClass

Matrix buffer трансформирует object. Light buffer даёт освещение. Дополнительная projected texture использует projector view/projection coordinates.

SetShaderParameters привязывает base и projected resources и matrices.

Это decal/projector-like путь. Не путать с обычной camera projection matrix.

## 39 TerrainBlendShaderClass

Terrain buffer хранит параметры высотных или slope bands. Несколько terrain textures и normal maps привязываются по slots.

Render также поддерживает shadow map и directional light.

Pixel shader вычисляет weights по высоте/slope и нормализует сумму, чтобы яркость не росла от наложения слоёв.

## 40 SkyShaderClass

InitializeGeometry создаёт fullscreen triangle или cube-like geometry.

Render получает inverse view-projection. Vertex/pixel shader восстанавливает direction луча для каждого screen pixel и sample sky texture.

Camera translation из view должна быть удалена, чтобы sky не двигался относительно мира.

Depth state обычно рисует sky позади geometry, а culling/raster state отличается от обычного object pass.

## 41 ShaderManager

`Load<T>` проверяет существующее имя, создаёт `unique_ptr<T>`, вызывает Initialize и только после успеха кладёт его в map.

`Get` возвращает base pointer. `GetAs<T>` использует dynamic/static проверку в зависимости реализации и позволяет вызвать специфичные методы.

`GetNames` перечисляет keys для UI/material selector.

`ForEach` даёт callback по загруженным shaders.

`Shutdown` вызывает virtual shutdown и очищает владельцев.

Singleton удобен, но renderer dependency скрыта. Следующий шаг — передавать manager ссылкой в systems, сохранив один owner в Application.

## 42 RenderCandidateSystem и FrustumCulling

Candidate system перебирает entities с transform и mesh, проверяет active/visibility flags, строит world bounds и формирует компактный список для renderer.

Frustum extraction получает шесть planes из view-projection. AABB тест отклоняет object только если он полностью снаружи хотя бы одной plane.

Skinned mesh bounds могут изменяться. Текущий static bound должен иметь запас либо обновляться.

Room culling и frustum culling являются независимыми фильтрами: object должен пройти оба.

## 43 Порядок render ресурсов

Полный путь одного opaque object:

1. Candidate system подтверждает видимость.
2. Model привязывает vertex/index buffers.
3. Material выбирает shader и textures.
4. Renderer обновляет per-frame lights при необходимости.
5. Shader wrapper обновляет per-object matrices/material.
6. Context получает topology, layout, VS, PS, buffers, SRV и samplers.
7. DrawIndexed отправляет command.
8. Следующий pass явно меняет необходимые states.

## 44 Что исправлять первым в render runtime

1. Удалить пустой copy constructor `TextureClass`.
2. Перевести raw COM fields на `ComPtr` постепенно.
3. Добавить rollback guards в RenderTexture и ShadowMap.
4. Централизовать shader slots и reflection validation.
5. Унифицировать `Shutdown` и `ShutDown`.
6. Сделать state ownership renderer явным, чтобы wrapper не оставлял blend/depth state.
7. Пометить ModelClass и TinyGltf path как legacy/experimental.
8. Выбрать adapter/output осознанно.
9. Перейти к flip swap effect отдельным проверяемым этапом.
10. Исправить кодировку комментариев без изменения поведения.

## 45 Renderer главный кадр

### 45.1 InspectSceneRenderFeatures

1. Null SceneManager даёт пустой набор features.
2. Mesh pool перебирается без рисования.
3. Material и shader type определяют, нужны ли reflection, refraction, water или preview passes.
4. Результат позволяет не выполнять дорогой offscreen pass, если в сцене нет его потребителя.

### 45.2 RenderFrame

1. Проверяются context, camera и SceneManager.
2. Scene features вычисляются один раз.
3. Directional light и shadow matrices обновляются до passes.
4. Shadow pass рисует depth с точки зрения света.
5. Reflection texture строится только при `needsReflectionPass`.
6. Refraction texture строится только при `needsRefractionPass`.
7. Main back buffer снова назначается render target.
8. Sky рисуется с удалённым translation view, чтобы оставаться бесконечно далёким.
9. Opaque scene получает shadow/environment resources.
10. Water и transparent surfaces используют уже готовые offscreen textures.
11. Grass, particles и debug geometry добавляются после непрозрачного мира.
12. First-person presentation рисуется отдельным pass.
13. SRV slots очищаются перед тем, как те же textures станут render targets следующего кадра.

### 45.3 Shadow helpers

1. `UpdateShadowLightViewProjection` нормализует направление света.
2. Почти нулевой direction заменяется безопасным.
3. Up vector меняется, если свет почти параллелен world up.
4. Shadow distance ограничивается разумным минимумом.
5. Light view и orthographic projection сохраняются для всех shadow-aware shaders.
6. `BindShadowState` связывает shadow SRV в одинаковый slot Light и PBR shaders.

### 45.4 ClearSRVSlots

1. Создаётся массив null SRV.
2. Он привязывается к заданному диапазону pixel shader slots.
3. D3D reference на старые SRV снимается.

Без этого Direct3D выдаёт conflict warning при попытке назначить texture одновременно input и render target.

## 46 ShaderBaseClass

1. Constructor зануляет shader, layout, sampler и common buffers.
2. Destructor вызывает `Shutdown`, поэтому повторный Shutdown обязан быть безопасен.
3. Compile helper вызывает D3DCompileFromFile с entry point и profile.
4. Error blob записывается в debug output или файл диагностики.
5. Успешный blob используется для CreateVertexShader или CreatePixelShader.
6. Input layout создаётся из vertex shader signature.
7. `BindShader` связывает VS, PS, layout и default sampler.
8. `CreateConstantBuffer` выравнивает byte width по требованию 16 байт.
9. `UpdateFog` map-ит dynamic buffer с WRITE_DISCARD, копирует settings, unmap-ит и связывает slot.
10. Shutdown освобождает buffers раньше shader objects и зануляет pointers.

## 47 MaterialClass

1. `Initialize` сохраняет name и подготавливает arrays texture slots.
2. `SetTexture` записывает non-owning pointer; вызывающий код сохраняет lifetime.
3. `AdoptTexture` принимает `unique_ptr` и одновременно обновляет удобный raw view.
4. `SetTexturePath` хранит сериализуемый путь отдельно от runtime SRV.
5. `Bind` собирает SRV всех занятых slots и передаёт их pixel shader.
6. Пустой texture slot получает null, а не случайный старый SRV.
7. `Unbind` очищает те же slots.
8. `Shutdown` очищает owned textures, но не удаляет borrowed textures.

Различие Set и Adopt является контрактом владения. Его нельзя скрывать за одним неоднозначным setter.

## 48 AssimpModelClass краткий порядок исполнения

Полный импорт и animation pipeline разобраны в специальной главе. Здесь порядок конкретного runtime-класса:

1. Importer читает scene с выбранными post-process flags.
2. Mesh vertices получают position, UV, normal, tangent и bone weights.
3. Materials загружают embedded или внешние textures, а отсутствующие карты получают 1x1 fallback.
4. Texture cache делает AddRef для совместно используемых SRV.
5. CPU arrays сохраняются для physics, picking и runtime damage.
6. GPU vertex/index buffers создаются отдельно для mesh parts.
7. `LoadBones` строит bone name -> index/offset.
8. `LoadAnimations` копирует channels и keys из Assimp во внутренние structs.
9. `UpdateAnimation` продвигает time и обходит node hierarchy.
10. InterpolatePosition использует lerp, InterpolateRotation — quaternion slerp, InterpolateScale — lerp.
11. Final bone matrix объединяет global inverse, node transform и offset в принятом в shader порядке.
12. `SetBoneMatricesFromComponent` загружает palette в constant/structured buffer.
13. `Render` привязывает buffers одного part за раз и вызывает его shader/material path.
14. `Shutdown` освобождает per-part buffers/SRV, owned textures и bone buffer ровно один раз.

