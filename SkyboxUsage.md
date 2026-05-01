# Как использовать Skybox в движке

## 1. Инициализация

```cpp
// В заголовочном файле
#include "src/Render/Camera/cameraclass.h"
#include "src/Render/Skybox/SkyboxClass.h"
#include "src/Render/Shader/SkyboxShaderClass.h"

CameraClass* m_Camera = nullptr;
SkyboxClass* m_Skybox = nullptr;
SkyboxShaderClass* m_SkyboxShader = nullptr;

// В функции инициализации (после создания Device и DeviceContext)
m_Camera = new CameraClass();
m_Camera->SetPosition(0.0f, 0.0f, -5.0f);

m_Skybox = new SkyboxClass();
bool result = m_Skybox->Initialize(
    m_d3dDevice, 
    m_d3dDeviceContext,
    "assets/texture/skybox_right.jpg",   // +X
    "assets/texture/skybox_left.jpg",    // -X
    "assets/texture/skybox_top.jpg",     // +Y
    "assets/texture/skybox_bottom.jpg",  // -Y
    "assets/texture/skybox_front.jpg",   // +Z
    "assets/texture/skybox_back.jpg"     // -Z
);

m_SkyboxShader = new SkyboxShaderClass();
result = m_SkyboxShader->Initialize(m_d3dDevice, hwnd);
```

## 2. Рендеринг

```cpp
// В цикле рендеринга (перед отрисовкой основной сцены)

// Обновляем камеру
m_Camera->SetRotation(rotationX, rotationY, rotationZ);
m_Camera->Render();

// Получаем матрицы
XMMATRIX worldMatrix = XMMatrixIdentity();
XMMATRIX viewMatrix, projectionMatrix;
m_Camera->GetViewMatrix(viewMatrix);
m_Camera->GetProjectionMatrix(projectionMatrix); // если есть такой метод

// === РЕНДЕРИНГ SKYBOX ===
// 1. Отключаем глубину для skybox
m_d3dDeviceContext->OMSetDepthStencilState(NULL, 0);

// 2. Получаем матрицу вида БЕЗ трансляции (только вращение камеры)
XMMATRIX skyboxViewMatrix;
m_Camera->GetViewMatrixWithoutTranslation(skyboxViewMatrix);

// 3. Рендерим skybox
m_Skybox->Render(m_d3dDeviceContext);
m_SkyboxShader->Render(
    m_d3dDeviceContext, 
    m_Skybox->GetIndexCount(),  // или 36 (6 граней * 2 треугольника * 3 индекса)
    worldMatrix, 
    skyboxViewMatrix,  // ← Важно: используем матрицу без трансляции!
    projectionMatrix,
    m_Skybox->GetTexture(0)  // Текстура для первой грани (остальные внутри шейдера по UV)
);

// 4. Включаем глубину обратно для основной сцены
// m_d3dDeviceContext->OMSetDepthStencilState(m_depthStencilState, 0);

// === РЕНДЕРИНГ ОСНОВНОЙ СЦЕНЫ ===
// ... ваша обычная отрисовка ...
```

## 3. Очистка

```cpp
// В Shutdown()
if (m_Skybox) {
    m_Skybox->Shutdown();
    delete m_Skybox;
    m_Skybox = nullptr;
}

if (m_SkyboxShader) {
    m_SkyboxShader->Shutdown();
    delete m_SkyboxShader;
    m_SkyboxShader = nullptr;
}

if (m_Camera) {
    delete m_Camera;
    m_Camera = nullptr;
}
```

## Важные моменты

1. **Порядок рендеринга**: Skybox рендерится ПЕРЕД основной сценой
2. **Z-буфер**: Для skybox можно отключить тест глубины или установить функцию сравнения на LESS_EQUAL
3. **Матрица вида**: Используйте `GetViewMatrixWithoutTranslation()` вместо обычной `GetViewMatrix()`
4. **Размер куба**: Skybox куб имеет размер 2x2x2 единицы (-1 до +1), что достаточно для охвата всей сцены

## Текстуры

Текстуры должны быть в формате JPG/PNG и располагаться по путям:
- `assets/texture/skybox_right.jpg`  - правая грань (+X)
- `assets/texture/skybox_left.jpg`   - левая грань (-X)
- `assets/texture/skybox_top.jpg`    - верхняя грань (+Y)
- `assets/texture/skybox_bottom.jpg` - нижняя грань (-Y)
- `assets/texture/skybox_front.jpg`  - передняя грань (+Z)
- `assets/texture/skybox_back.jpg`   - задняя грань (-Z)
