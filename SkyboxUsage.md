# Как использовать Skybox в вашем движке

## 1. Скачанные текстуры неба

Текстуры для skybox были загружены в папку `/workspace/assets/texture/`:
- `skybox_right.jpg` - правая грань (+X)
- `skybox_left.jpg` - левая грань (-X)
- `skybox_top.jpg` - верхняя грань (+Y)
- `skybox_bottom.jpg` - нижняя грань (-Y)
- `skybox_front.jpg` - передняя грань (+Z)
- `skybox_back.jpg` - задняя грань (-Z)

## 2. Созданные файлы

### Классы:
- `src/Render/Skybox/SkyboxClass.h` - заголовочный файл класса skybox
- `src/Render/Skybox/SkyboxClass.cpp` - реализация класса skybox

### Шейдеры:
- `src/Render/Shader/Vs/skybox.vs` - вершинный шейдер
- `src/Render/Shader/Ps/skybox.ps` - пиксельный шейдер
- `src/Render/Shader/SkyboxShaderClass.h` - заголовочный файл шейдера
- `src/Render/Shader/SkyboxShaderClass.cpp` - реализация шейдера

## 3. Пример использования в коде

```cpp
#include "Render/Skybox/SkyboxClass.h"
#include "Render/Shader/SkyboxShaderClass.h"

// В классе Renderer или SystemClass добавьте:
SkyboxClass* m_skybox;
SkyboxShaderClass* m_skyboxShader;

// Инициализация (после инициализации D3D):
m_skybox = new SkyboxClass();
m_skyboxShader = new SkyboxShaderClass();

bool result = m_skybox->Initialize(
    m_D3D->GetDevice(), 
    m_D3D->GetDeviceContext(),
    "assets/texture/skybox_right.jpg",   // right
    "assets/texture/skybox_left.jpg",    // left
    "assets/texture/skybox_top.jpg",     // top
    "assets/texture/skybox_bottom.jpg",  // bottom
    "assets/texture/skybox_front.jpg",   // front
    "assets/texture/skybox_back.jpg"     // back
);

result = m_skyboxShader->Initialize(m_D3D->GetDevice(), hwnd);

// В цикле рендеринга (Frame):
// 1. Отключаем Z-буфер для skybox (рисуется последним на бесконечности)
m_D3D->TurnZBufferOff();

// 2. Получаем матрицы камеры (без позиции, только вращение)
XMMATRIX worldMatrix = XMMatrixIdentity();
XMMATRIX viewMatrix = camera->GetViewMatrixWithoutTranslation(); // Только вращение
XMMATRIX projectionMatrix;
m_D3D->GetProjectionMatrix(projectionMatrix);

// 3. Рендерим skybox
m_skybox->Render(m_D3D->GetDeviceContext());
m_skyboxShader->Render(
    m_D3D->GetDeviceContext(),
    m_skybox->GetIndexCount(),
    worldMatrix,
    viewMatrix,
    projectionMatrix,
    m_skybox->GetTexture(0) // Или любую грань, либо используйте cube map
);

// 4. Включаем Z-буфер обратно
m_D3D->TurnZBufferOn();

// Очистка:
if(m_skybox) {
    m_skybox->Shutdown();
    delete m_skybox;
    m_skybox = nullptr;
}
if(m_skyboxShader) {
    m_skyboxShader->Shutdown();
    delete m_skyboxShader;
    m_skyboxShader = nullptr;
}
```

## 4. Важно!

Для правильной работы skybox:
1. Рендерьте skybox **первым** или **последним** в сцене
2. Используйте **только вращение камеры** для view матрицы skybox (без позиции)
3. Отключайте **Z-буфер** при отрисовке skybox
4. Убедитесь, что пути к текстурам правильные относительно исполняемого файла

## 5. Альтернатива: Cube Map

Для более продвинутого skybox можно использовать Cube Texture (cube map), 
который загружает все 6 граней в одну текстуру. Это требует изменений в 
шейдерах и использования `TextureCube` вместо `Texture2D`.
