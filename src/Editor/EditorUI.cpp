// EditorUI.cpp
#include "EditorUI.h"
#include <string>
#include "SceneManager.h"
#include <backends/imgui_impl_win32.h>
#include <imgui-master/backends/imgui_impl_dx11.h>
EditorUI::~EditorUI()
{
    Shutdown();
}

bool EditorUI::Initialize(HWND hwnd, ID3D11Device* device, ID3D11DeviceContext* context)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    if (!ImGui_ImplWin32_Init(hwnd)) return false;
    if (!ImGui_ImplDX11_Init(device, context)) return false;

    return true;
}

void EditorUI::Shutdown()
{
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void EditorUI::NewFrame()
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void EditorUI::Render()
{
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

//=============================================================================
// 🔹 ГЛАВНЫЙ МЕТОД UPDATE: Вкладки + свойства
//=============================================================================
void EditorUI::Update(SceneManager& sceneManager)
{
    if (ImGui::Begin("Scene Editor", nullptr, ImGuiWindowFlags_NoCollapse))
    {
        // === ВКЛАДКИ: Объекты / Спрайты / Свет ===
        if (ImGui::BeginTabBar("EntityTabs"))
        {
            if (ImGui::BeginTabItem("📦 Objects"))
            {
                RenderObjectsTab(sceneManager);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("🖼️ Sprites"))
            {
                RenderSpritesTab(sceneManager);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("💡 Lights"))
            {
                RenderLightsTab(sceneManager);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        // === ПАНЕЛЬ СВОЙСТВ (появляется при выборе) ===
        if (m_selectedType != SelectedType::None && m_selectedIndex >= 0)
        {
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::Begin("Properties"))
            {
                switch (m_selectedType)
                {
                case SelectedType::Object:
                    if (GameObject* obj = sceneManager.GetObject(m_selectedIndex))
                        RenderObjectProperties(obj);
                    break;
                case SelectedType::Sprite:
                    if (WorldSpriteClass* sprite = sceneManager.GetSprite(m_selectedIndex))
                        RenderSpriteProperties(sprite);
                    break;
                case SelectedType::Light:
                    RenderLightProperties(sceneManager, m_selectedIndex);
                    break;
                default:
                    break;
                }
            }
            ImGui::End();
        }
    }
    // В конце EditorUI::Update():
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.6f);
    if (ImGui::Begin("##DebugSelection", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Selected: ");
        ImGui::SameLine();
        switch (m_selectedType)
        {
        case SelectedType::Object: ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1), "Object #%d", m_selectedIndex); break;
        case SelectedType::Light:  ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.3f, 1), "Light #%d", m_selectedIndex); break;
        case SelectedType::Sprite: ImGui::TextColored(ImVec4(0.3f, 0.6f, 1.0f, 1), "Sprite #%d", m_selectedIndex); break;
        default: ImGui::Text("None");
        }
        ImGui::End();
    }
    ImGui::End();
}

//=============================================================================
// 🔹 ВКЛАДКА: Объекты
//=============================================================================
void EditorUI::RenderObjectsTab(SceneManager& sceneManager)
{
    ImGui::Text("Game Objects:");
    ImGui::BeginChild("ObjectList", ImVec2(0, 150), true);

    for (size_t i = 0; i < sceneManager.GetObjectCount(); ++i)
    {
        GameObject* obj = sceneManager.GetObject(static_cast<int>(i));
        if (!obj) continue;

        char label[64];
        snprintf(label, sizeof(label), "Object #%zu @ (%.1f, %.1f, %.1f)",
            i, obj->GetPosition().x, obj->GetPosition().y, obj->GetPosition().z);

        bool isSelected = (m_selectedType == SelectedType::Object && m_selectedIndex == static_cast<int>(i));
        if (ImGui::Selectable(label, isSelected))
        {
            m_selectedType = SelectedType::Object;
            m_selectedIndex = static_cast<int>(i);
        }
    }
    ImGui::EndChild();
}

//=============================================================================
// 🔹 ВКЛАДКА: Спрайты
//=============================================================================
void EditorUI::RenderSpritesTab(SceneManager& sceneManager)
{
    ImGui::Text("World Sprites:");
    ImGui::BeginChild("SpriteList", ImVec2(0, 150), true);

    for (size_t i = 0; i < sceneManager.GetSpriteCount(); ++i)
    {
        WorldSpriteClass* sprite = sceneManager.GetSprite(static_cast<int>(i));
        if (!sprite) continue;

        char label[128];
        XMFLOAT3 pos = sprite->GetPosition();
        snprintf(label, sizeof(label), "Sprite #%zu @ (%.1f, %.1f, %.1f)",
            i, pos.x, pos.y, pos.z);

        bool isSelected = (m_selectedType == SelectedType::Sprite && m_selectedIndex == static_cast<int>(i));
        if (ImGui::Selectable(label, isSelected))
        {
            m_selectedType = SelectedType::Sprite;
            m_selectedIndex = static_cast<int>(i);
        }
    }
    ImGui::EndChild();
}

//=============================================================================
// 🔹 ВКЛАДКА: Источники света
//=============================================================================
void EditorUI::RenderLightsTab(SceneManager& sceneManager)
{
    ImGui::Text("Light Sources:");
    ImGui::BeginChild("LightList", ImVec2(0, 150), true);

    for (int i = 0; i < NUM_LIGHTS; ++i)
    {
        if (!sceneManager.IsLightActive(i)) continue;  // Пропускаем выключенные

        XMFLOAT4 pos = sceneManager.GetLightPosition(i);
        XMFLOAT4 color = sceneManager.GetLightColor(i);

        char label[128];
        snprintf(label, sizeof(label), "Light #%d @ (%.1f, %.1f, %.1f) [R:%.2f G:%.2f B:%.2f]",
            i, pos.x, pos.y, pos.z, color.x, color.y, color.z);

        bool isSelected = (m_selectedType == SelectedType::Light && m_selectedIndex == i);
        if (ImGui::Selectable(label, isSelected))
        {
            m_selectedType = SelectedType::Light;
            m_selectedIndex = i;
        }
    }
    ImGui::EndChild();
}

//=============================================================================
// 🔹 СВОЙСТВА: GameObject
//=============================================================================
void EditorUI::RenderObjectProperties(GameObject* obj)
{
    ImGui::Text("📦 Object Properties");

    XMFLOAT3 pos = obj->GetPosition();
    if (ImGui::DragFloat3("Position", &pos.x, 0.1f))
        obj->SetPosition(pos.x, pos.y, pos.z);

    // 🔹 Можно добавить вращение и масштаб, если есть геттеры/сеттеры
    // XMFLOAT3 rot = obj->GetRotation();
    // if (ImGui::DragFloat3("Rotation", &rot.x, 1.0f)) ...
}

//=============================================================================
// 🔹 СВОЙСТВА: WorldSprite
//=============================================================================
void EditorUI::RenderSpriteProperties(WorldSpriteClass* sprite)
{
    ImGui::Text("🖼️ Sprite Properties");

    // Позиция
    XMFLOAT3 pos = sprite->GetPosition();
    if (ImGui::DragFloat3("Position", &pos.x, 0.1f))
        sprite->SetPosition(pos.x, pos.y, pos.z);

    // Масштаб
    XMFLOAT2 scale = sprite->GetScale();
    if (ImGui::DragFloat2("Scale", &scale.x, 0.01f, 0.01f, 10.0f))
        sprite->SetScale(scale.x, scale.y);
}

//=============================================================================
// 🔹 СВОЙСТВА: LightSource
//=============================================================================
//=============================================================================
// 🔹 СВОЙСТВА: LightSource (ИСПРАВЛЕННАЯ ВЕРСИЯ)
//=============================================================================
//=============================================================================
// 🔹 СВОЙСТВА: LightSource (ПОЛНОСТЬЮ ИСПРАВЛЕННАЯ ВЕРСИЯ)
//=============================================================================
void EditorUI::RenderLightProperties(SceneManager& sceneManager, int lightIndex)
{
    ImGui::Text("💡 Light #%d Properties", lightIndex);

    // === ПОЗИЦИЯ ===
    // 1. Получаем позицию из менеджера (возвращает XMFLOAT4)
    XMFLOAT4 lightPos4 = sceneManager.GetLightPosition(lightIndex);

    // 2. 🔹 Конвертируем XMFLOAT4 → XMFLOAT3 (берём только xyz)
    XMFLOAT3 lightPos3;
    lightPos3.x = lightPos4.x;
    lightPos3.y = lightPos4.y;
    lightPos3.z = lightPos4.z;

    // 3. Редактируем в UI
    if (ImGui::DragFloat3("Position", &lightPos3.x, 0.1f))
    {
        // 4. 🔹 Обновляем свет — передаём позицию как XMFLOAT3!
        sceneManager.SetLight(lightIndex, lightPos3, sceneManager.GetLightColor(lightIndex), true);
    }

    // === ЦВЕТ ===
    // 1. Получаем цвет (XMFLOAT4: rgba)
    XMFLOAT4 lightColor4 = sceneManager.GetLightColor(lightIndex);

    // 2. 🔹 Конвертируем XMFLOAT4 → XMFLOAT3 (берём только rgb)
    XMFLOAT3 lightColor3;
    lightColor3.x = lightColor4.x;
    lightColor3.y = lightColor4.y;
    lightColor3.z = lightColor4.z;

    // 3. Редактируем в UI
    if (ImGui::ColorEdit3("Color", &lightColor3.x))
    {
        // 4. 🔹 Собираем новый цвет обратно в XMFLOAT4 (сохраняем альфа-канал!)
        XMFLOAT4 newColor4;
        newColor4.x = lightColor3.x;
        newColor4.y = lightColor3.y;
        newColor4.z = lightColor3.z;
        newColor4.w = lightColor4.w;  // ← Важно: сохраняем интенсивность/альфа

        // 5. 🔹 Обновляем свет — позицию снова конвертируем в XMFLOAT3!
        XMFLOAT3 posForUpdate = { lightPos4.x, lightPos4.y, lightPos4.z };
        sceneManager.SetLight(lightIndex, posForUpdate, newColor4, true);
    }

    // === АКТИВНОСТЬ ===
    bool active = sceneManager.IsLightActive(lightIndex);
    if (ImGui::Checkbox("Active", &active))
    {
        // 🔹 Позицию конвертируем в XMFLOAT3 перед передачей!
        XMFLOAT3 posForUpdate = { lightPos4.x, lightPos4.y, lightPos4.z };
        sceneManager.SetLight(lightIndex, posForUpdate, sceneManager.GetLightColor(lightIndex), active);
    }

    // === КНОПКА: ОТВЯЗАТЬ ОТ ОБЪЕКТА ===
    if (ImGui::Button("Detach from Object"))
    {
        sceneManager.DetachLight(lightIndex);
    }
}