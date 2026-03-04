// EditorUI.h
#pragma once
#include <imgui.h>
#include "SceneManager.h"

class EditorUI
{
public:
    EditorUI() = default;
    ~EditorUI();

    bool Initialize(HWND hwnd, ID3D11Device* device, ID3D11DeviceContext* context);
    void Shutdown();
    void NewFrame();
    void Update(SceneManager& sceneManager);  // SceneManager по ссылке
    void Render();

    // 🔹 Геттеры для ApplicationClass
    int GetSelectedIndex() const { return m_selectedIndex; }
    enum class SelectedType { None, Object, Sprite, Light };
    void SetSelection(SelectedType type, int index)
    {
        m_selectedType = type;
        m_selectedIndex = index;
    }


    // 🔹 Новый enum для типа выбранной сущности
  
    SelectedType GetSelectedType() const { return m_selectedType; }

private:
    // 🔹 Текущий выбор: тип + индекс
    SelectedType m_selectedType = SelectedType::None;
    int m_selectedIndex = -1;

    // 🔹 Вкладки UI
    void RenderObjectsTab(SceneManager& sceneManager);
    void RenderSpritesTab(SceneManager& sceneManager);
    void RenderLightsTab(SceneManager& sceneManager);

    // 🔹 Панели свойств для каждого типа
    void RenderObjectProperties(GameObject* obj);
    void RenderSpriteProperties(WorldSpriteClass* sprite);
    void RenderLightProperties(SceneManager& sceneManager, int lightIndex);
};