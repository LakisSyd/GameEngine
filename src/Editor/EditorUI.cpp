// EditorUI.cpp
#include "EditorUI.h"
#include <string>
#include "SceneManager.h"
#include <backends/imgui_impl_win32.h>
#include <imgui-master/backends/imgui_impl_dx11.h>
#include "PhysicsSystem.h"
#include "SubmarineJointSystem.h"
#include "FloodingSystem.h"
#include "SubmarineBuilder.h"
#include "JointConnectionSystem.h"
#include "../../SubmarineSystem.h"
#include <imgui_demo.cpp>
inline btQuaternion XMFloat4ToBtQuaternion(const DirectX::XMFLOAT4& q)
{
    return btQuaternion(q.x, q.y, q.z, q.w);
}
// ── Цвета темы (UE5-стиль) ───────────────────────────────────────────────────
static constexpr ImVec4 COL_HEADER = ImVec4(0.13f, 0.13f, 0.13f, 1.0f);
static constexpr ImVec4 COL_SECTION = ImVec4(0.18f, 0.18f, 0.18f, 1.0f);
static constexpr ImVec4 COL_ACCENT = ImVec4(0.06f, 0.53f, 0.98f, 1.0f);
static constexpr ImVec4 COL_ACCENT_HOV = ImVec4(0.20f, 0.65f, 1.00f, 1.0f);
static constexpr ImVec4 COL_GREEN = ImVec4(0.13f, 0.69f, 0.30f, 1.0f);
static constexpr ImVec4 COL_GREEN_HOV = ImVec4(0.18f, 0.85f, 0.40f, 1.0f);
static constexpr ImVec4 COL_RED = ImVec4(0.75f, 0.18f, 0.18f, 1.0f);
static constexpr ImVec4 COL_TEXT_DIM = ImVec4(0.55f, 0.55f, 0.55f, 1.0f);
static constexpr ImVec4 COL_SELECTED = ImVec4(0.06f, 0.45f, 0.80f, 0.35f);
static constexpr ImVec4 COL_X = ImVec4(0.86f, 0.27f, 0.27f, 1.0f);
static constexpr ImVec4 COL_Y = ImVec4(0.27f, 0.72f, 0.27f, 1.0f);
static constexpr ImVec4 COL_Z = ImVec4(0.27f, 0.47f, 0.86f, 1.0f);

EditorUI::~EditorUI() { Shutdown(); }

bool EditorUI::Initialize(HWND hwnd, ID3D11Device* device, ID3D11DeviceContext* context, FileSystem* fs)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    m_fileSystem = fs;
    if (m_fileSystem)
    {
        m_currentDir = "";
        m_fileSystem->Refresh();
    }

    m_debugSettings.showColliders = true;
    m_debugSettings.showContacts = false;
    m_debugSettings.showAABB = false;
    m_debugSettings.colorDynamic = { 0.2f, 0.8f, 0.2f, 0.6f };
    m_debugSettings.colorStatic = { 0.8f, 0.2f, 0.2f, 0.6f };

    ImGui::StyleColorsDark();
    ImGuiStyle& s = ImGui::GetStyle();

    m_hwnd = hwnd;
    m_device = device;
    m_context = context;

    s.WindowRounding = 4.0f;
    s.FrameRounding = 3.0f;
    s.PopupRounding = 3.0f;
    s.ScrollbarRounding = 3.0f;
    s.GrabRounding = 3.0f;
    s.WindowBorderSize = 0.0f;
    s.FrameBorderSize = 0.0f;
    s.ItemSpacing = ImVec2(6, 5);
    s.FramePadding = ImVec2(8, 4);
    s.IndentSpacing = 16.0f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    c[ImGuiCol_ChildBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    c[ImGuiCol_Header] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    c[ImGuiCol_HeaderActive] = COL_ACCENT;
    c[ImGuiCol_Button] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    c[ImGuiCol_ButtonActive] = COL_ACCENT;
    c[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    c[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    c[ImGuiCol_TitleBgActive] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    c[ImGuiCol_Tab] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    c[ImGuiCol_TabHovered] = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
    c[ImGuiCol_TabActive] = COL_ACCENT;
    c[ImGuiCol_Separator] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    c[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    c[ImGuiCol_ScrollbarGrab] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    c[ImGuiCol_CheckMark] = COL_ACCENT;
    c[ImGuiCol_SliderGrab] = COL_ACCENT;
    c[ImGuiCol_SliderGrabActive] = COL_ACCENT_HOV;

    const float dpiScale = static_cast<float>(GetDpiForWindow(hwnd)) / 96.0f;
    io.FontGlobalScale = 1.0f;

    if (!ImGui_ImplWin32_Init(hwnd))           return false;
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

bool EditorUI::WantCaptureMouse() const { return ImGui::GetIO().WantCaptureMouse; }

void EditorUI::SectionHeader(const char* label)
{
    const float headerH = std::floor(24.0f * EditorUI::Scale());
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.16f, 0.16f, 0.16f, 1.0f));
    ImGui::BeginChild(label, ImVec2(-1, headerH), false);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + std::floor(4.0f * EditorUI::Scale()));
    ImGui::TextColored(ImVec4(0.75f, 0.75f, 0.75f, 1.0f), "  %s", label);
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

static bool DragFloat3Colored(const char* id, float v[3], float speed = 0.1f)
{
    bool changed = false;
    ImGui::PushID(id);
    const float w = (ImGui::GetContentRegionAvail().x - 6.0f) / 3.0f;

    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.30f, 0.08f, 0.08f, 1.0f));
    ImGui::SetNextItemWidth(w);
    if (ImGui::DragFloat("##X", &v[0], speed)) changed = true;
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 2);

    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.08f, 0.25f, 0.08f, 1.0f));
    ImGui::SetNextItemWidth(w);
    if (ImGui::DragFloat("##Y", &v[1], speed)) changed = true;
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 2);

    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.08f, 0.13f, 0.30f, 1.0f));
    ImGui::SetNextItemWidth(w);
    if (ImGui::DragFloat("##Z", &v[2], speed)) changed = true;
    ImGui::PopStyleColor();

    ImGui::PopID();
    return changed;
}

void EditorUI::RenderMainMenu(SceneManager& sceneManager)
{
    ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4(0.08f, 0.08f, 0.08f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.12f, 0.12f, 0.12f, 0.97f));
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.06f, 0.53f, 0.98f, 0.25f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.06f, 0.53f, 0.98f, 0.55f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, COL_ACCENT);

    if (ImGui::BeginMainMenuBar())
    {
        m_menuBarHeight = ImGui::GetWindowSize().y;

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 8.0f));

        ImGui::PushStyleColor(ImGuiCol_Text, COL_ACCENT);
        ImGui::Text(" DX Editor ");
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 0.35f, 0.35f, 1.0f));
        ImGui::Text(" | ");
        ImGui::PopStyleColor();

        if (ImGui::BeginMenu("  File  "))
        {
            const bool hasSavePath = !m_lastSavePath.empty();

            ImGui::PushStyleColor(ImGuiCol_Text, COL_GREEN);
            if (ImGui::MenuItem("  Save Scene", "Ctrl+S", false, hasSavePath))
            {
                m_saveRequest.pending = true;
                m_saveRequest.path = m_lastSavePath;
            }
            ImGui::PopStyleColor();

            if (ImGui::MenuItem("  Save As..."))
            {
                if (!m_lastSavePath.empty())
                    strncpy_s(m_savePathBuf, sizeof(m_savePathBuf),
                        m_lastSavePath.c_str(), _TRUNCATE);
                m_openSaveDialog = true;
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.78f, 1.00f, 1.00f));
            if (ImGui::MenuItem("  Load Scene...", "Ctrl+O"))
            {
                if (!m_lastSavePath.empty())
                    strncpy_s(m_loadPathBuf, sizeof(m_loadPathBuf),
                        m_lastSavePath.c_str(), _TRUNCATE);
                m_openLoadDialog = true;
            }
            ImGui::PopStyleColor();

            ImGui::EndMenu();
        }

        {
            char status[96];
            snprintf(status, sizeof(status), " %zu obj  |  %d lights  |  %d sprites  ",
                sceneManager.GetObjectCount(),
                NUM_LIGHTS,
                sceneManager.GetSpriteCount());

            const float textW = ImGui::CalcTextSize(status).x;
            const float barW = ImGui::GetContentRegionAvail().x;
            if (barW > textW + 10.0f)
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + barW - textW);

            ImGui::TextColored(ImVec4(0.45f, 0.45f, 0.45f, 1.0f), "%s", status);
        }

        ImGui::PopStyleVar(1);
        ImGui::EndMainMenuBar();
    }

    ImGui::PopStyleColor(5);

    if (m_openSaveDialog) { ImGui::OpenPopup("##SaveDialog"); m_openSaveDialog = false; }
    if (m_openLoadDialog) { ImGui::OpenPopup("##LoadDialog"); m_openLoadDialog = false; }

    RenderSaveDialog();
    RenderLoadDialog();
}

// ─────────────────────────────────────────────────────────────────────────────
// Диалог «Save As»
// ─────────────────────────────────────────────────────────────────────────────
void EditorUI::RenderSaveDialog()
{
    const ImVec2 dialogSz = ImVec2(std::floor(420.0f * ScaleX()),
        std::floor(160.0f * ScaleY()));

    ImGui::SetNextWindowSize(dialogSz, ImGuiCond_Always);
    ImGui::SetNextWindowPos(
        ImVec2((ImGui::GetIO().DisplaySize.x - dialogSz.x) * 0.5f,
            (ImGui::GetIO().DisplaySize.y - dialogSz.y) * 0.5f),
        ImGuiCond_Always);

    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.10f, 0.10f, 0.10f, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.06f, 0.53f, 0.98f, 1.00f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));

    if (ImGui::BeginPopupModal("##SaveDialog", nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, COL_GREEN);
        ImGui::Text("  Save Scene As");
        ImGui::PopStyleColor();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextColored(ImVec4(0.65f, 0.65f, 0.65f, 1.0f), "File path:");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputText("##SavePath", m_savePathBuf, sizeof(m_savePathBuf));

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        const float btnW = std::floor(120.0f * ScaleX());

        ImGui::PushStyleColor(ImGuiCol_Button, COL_GREEN);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COL_GREEN_HOV);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.08f, 0.45f, 0.18f, 1.0f));
        if (ImGui::Button("  Save  ", ImVec2(btnW, 0)))
        {
            m_saveRequest.pending = true;
            m_saveRequest.path = m_savePathBuf;
            m_lastSavePath = m_savePathBuf;
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(3);

        ImGui::SameLine(0, 8.0f);

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f, 0.22f, 0.22f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.32f, 0.32f, 0.32f, 1.0f));
        if (ImGui::Button("  Cancel  ", ImVec2(btnW, 0)))
            ImGui::CloseCurrentPopup();
        ImGui::PopStyleColor(2);

        ImGui::EndPopup();
    }

    ImGui::PopStyleVar(1);
    ImGui::PopStyleColor(2);
}

// ─────────────────────────────────────────────────────────────────────────────
// Диалог «Load Scene»
// ─────────────────────────────────────────────────────────────────────────────
void EditorUI::RenderLoadDialog()
{
    const ImVec2 dialogSz = ImVec2(std::floor(420.0f * ScaleX()),
        std::floor(160.0f * ScaleY()));

    ImGui::SetNextWindowSize(dialogSz, ImGuiCond_Always);
    ImGui::SetNextWindowPos(
        ImVec2((ImGui::GetIO().DisplaySize.x - dialogSz.x) * 0.5f,
            (ImGui::GetIO().DisplaySize.y - dialogSz.y) * 0.5f),
        ImGuiCond_Always);

    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.10f, 0.10f, 0.10f, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.06f, 0.53f, 0.98f, 1.00f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));

    if (ImGui::BeginPopupModal("##LoadDialog", nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.78f, 1.00f, 1.00f));
        ImGui::Text("  Load Scene");
        ImGui::PopStyleColor();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextColored(ImVec4(0.65f, 0.65f, 0.65f, 1.0f), "File path:");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputText("##LoadPath", m_loadPathBuf, sizeof(m_loadPathBuf));

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        const float btnW = std::floor(120.0f * ScaleX());

        ImGui::PushStyleColor(ImGuiCol_Button, COL_ACCENT);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COL_ACCENT_HOV);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.03f, 0.35f, 0.72f, 1.0f));
        if (ImGui::Button("  Load  ", ImVec2(btnW, 0)))
        {
            m_loadRequest.pending = true;
            m_loadRequest.path = m_loadPathBuf;
            m_lastSavePath = m_loadPathBuf;
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(3);

        ImGui::SameLine(0, 8.0f);

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f, 0.22f, 0.22f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.32f, 0.32f, 0.32f, 1.0f));
        if (ImGui::Button("  Cancel  ", ImVec2(btnW, 0)))
            ImGui::CloseCurrentPopup();
        ImGui::PopStyleColor(2);

        ImGui::EndPopup();
    }

    ImGui::PopStyleVar(1);
    ImGui::PopStyleColor(2);
}

//=============================================================================
// Update — главный метод
//=============================================================================
void EditorUI::Update(SceneManager& sceneManager)
{
    NewFrame();
    RenderMainMenu(sceneManager);
    RenderSidebar(sceneManager);
    RenderCompartmentEditor(sceneManager);
    RenderDetailsPanel(sceneManager);
    RenderScenePreview();
}

void EditorUI::BeginMainMenuBar(SceneManager& sceneManager)
{
}

//=============================================================================
// RenderSceneOutliner — список всех объектов, источников света и спрайтов
//=============================================================================
void EditorUI::RenderSceneOutliner(SceneManager& sceneManager)
{
    ImGui::Spacing();
    SectionHeader("Scene Hierarchy");
    ImGui::Spacing();

    const float avail = ImGui::GetContentRegionAvail().y - std::floor(4.0f * ScaleY());
    ImGui::BeginChild("##OutlinerList", ImVec2(-1, avail), false);

    // ✅ НОВОЕ: Итерируем ВСЕ сущности с MeshRendererComponent
    auto& meshPool = sceneManager.GetECSRegistry().GetComponents<MeshRendererComponent>();
    auto& registry = sceneManager.GetECSRegistry();

    for (auto& [entity, mesh] : meshPool)
    {
        // Пропускаем неактивные
        if (!sceneManager.IsObjectActive(entity)) continue;

        // 🔥 Проверяем, это свет или обычный объект
        auto* light = registry.GetComponent<LightComponent>(entity);

        char label[64];
        if (light)
        {
            // Это источник света
            snprintf(label, sizeof(label), "  💡 Light [%u]", entity);
        }
        else
        {
            // Обычный объект
            snprintf(label, sizeof(label), "  📦 Object [%u]", entity);
        }

        // Определяем тип выделения
        SelectedType typeForThis = light ? SelectedType::Light : SelectedType::Object;
        bool selected = (m_selectedType == typeForThis && m_selectedEntity == entity);

        if (selected) ImGui::PushStyleColor(ImGuiCol_Header, COL_SELECTED);

        if (ImGui::Selectable(label, selected))
        {
            SetSelection(typeForThis, entity);
        }

        if (selected) ImGui::PopStyleColor();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ── Спрайты (остаются как раньше) ───────────────────────────────────────
    for (int i = 0; i < sceneManager.GetSpriteCount(); ++i)
    {
        char label[64];
        snprintf(label, sizeof(label), "  🖼 Sprite #%d", i);

        bool selected = (m_selectedType == SelectedType::Sprite && m_selectedLegacyIndex == i);
        if (selected) ImGui::PushStyleColor(ImGuiCol_Header, COL_SELECTED);

        if (ImGui::Selectable(label, selected))
            SetSelection(SelectedType::Sprite, i);

        if (selected) ImGui::PopStyleColor();
    }

    ImGui::EndChild();
}

//=============================================================================
// RenderDetailsPanel — свойства выделенного объекта
//=============================================================================
void EditorUI::RenderDetailsPanel(SceneManager& sceneManager)
{
    if (m_selectedType == SelectedType::None) return;

    ImGui::Begin("Details", nullptr, ImGuiWindowFlags_NoCollapse);

    switch (m_selectedType)
    {
    case SelectedType::Object:
        if (m_selectedEntity != INVALID_ENTITY)
            RenderObjectProperties(sceneManager, m_selectedEntity);
        break;

    case SelectedType::Light:
        if (m_selectedEntity != INVALID_ENTITY)
            RenderLightPropertiesByEntity(sceneManager, m_selectedEntity);
        break;

    case SelectedType::Sprite:
        // ...
        break;
    }

    ImGui::End();
}



//=============================================================================
// RenderObjectProperties — Transform + менеджер компонентов
//=============================================================================
void EditorUI::RenderObjectProperties(SceneManager& sceneManager, Entity entity)
{
    auto* transform = sceneManager.GetObjectTransform(entity);
    if (!transform)
    {
        ImGui::TextColored(COL_RED, "Entity not found");
        return;
    }

    // ── Transform ─────────────────────────────────────────────────────────────
    SectionHeader("Transform");
    ImGui::Spacing();

    float pos[3] = { transform->position.x, transform->position.y, transform->position.z };
    ImGui::TextColored(COL_TEXT_DIM, " Position");
    if (DragFloat3Colored("##ObjPos", pos, 0.1f))
        transform->position = { pos[0], pos[1], pos[2] };

    ImGui::Spacing();

    float rot[3] = { transform->rotation.x, transform->rotation.y, transform->rotation.z };
    ImGui::TextColored(COL_TEXT_DIM, " Rotation");
    if (DragFloat3Colored("##ObjRot", rot, 0.01f))
        transform->rotation = { rot[0], rot[1], rot[2] };

    ImGui::Spacing();

    float scl[3] = { transform->scale.x, transform->scale.y, transform->scale.z };
    ImGui::TextColored(COL_TEXT_DIM, " Scale");
    if (DragFloat3Colored("##ObjScl", scl, 0.01f))
        transform->scale = { scl[0], scl[1], scl[2] };

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ── Менеджер компонентов ──────────────────────────────────────────────────
    RenderComponentManager(sceneManager, entity);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ── Удалить объект ────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_Button, COL_RED);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
    if (ImGui::Button("Remove Object", ImVec2(-1, 0)))
    {
        sceneManager.RemoveObject(entity);

        m_selectedEntity = INVALID_ENTITY;
        m_selectedType = SelectedType::None;
    }
    ImGui::PopStyleColor(2);
}

//=============================================================================
// RenderComponentManager — показывает компоненты сущности, позволяет
//                          добавлять и удалять их через UI
//=============================================================================
void EditorUI::RenderComponentManager(SceneManager& sceneManager, Entity entity)
{
    auto& registry = sceneManager.GetECSRegistry();

    if (registry.HasComponent<CompartmentComponent>(entity))
    {
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.10f, 0.25f, 0.35f, 1.0f)); // 🔵 Синий
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.14f, 0.35f, 0.48f, 1.0f));
        bool open = ImGui::CollapsingHeader("  Compartment Component##comp", ImGuiTreeNodeFlags_DefaultOpen);
        ImGui::PopStyleColor(2);

        if (open)
        {
            auto* comp = registry.GetComponent<CompartmentComponent>(entity);
            if (!comp) { ImGui::TextColored(COL_RED, "Component error"); return; }

            ImGui::Spacing();

            // ── AABB отсека ────────────────────────────────────────────
            ImGui::TextColored(COL_TEXT_DIM, " Compartment Bounds (Local)");
            float min[3] = { comp->localMin.x, comp->localMin.y, comp->localMin.z };
            float max[3] = { comp->localMax.x, comp->localMax.y, comp->localMax.z };

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextColored(COL_TEXT_DIM, " Compartment Bounds");

            // Переключатель: Центр+Размер ИЛИ Мин/Макс
            static bool useCenterSize = true;
            ImGui::Checkbox(" Use Center + Size", &useCenterSize);
            ImGui::SameLine();
            if (ImGui::SmallButton("Reset View")) { useCenterSize = true; }

            if (useCenterSize)
            {
                // 🔹 Вычисляем центр и размер из min/max (если ещё не кэшировано)
                if (m_editingCompartment != entity)
                {
                    StartCompartmentEdit(entity);
                }

                // Редактирование ЦЕНТРА (относительно подлодки)
                ImGui::TextColored(COL_TEXT_DIM, " Center Offset (Local)");
                float center[3] = { m_compartmentCenter.x, m_compartmentCenter.y, m_compartmentCenter.z };

                // Цветные поля для осей
                if (DragFloat3Colored("##CompCenter", center, 0.1f))
                {
                    m_compartmentCenter = { center[0], center[1], center[2] };
                    UpdateCompartmentFromCache(comp);  // Применяем изменения
                }

                // Редактирование РАЗМЕРА
                ImGui::Spacing();
                ImGui::TextColored(COL_TEXT_DIM, " Size (X, Y, Z)");
                float size[3] = { m_compartmentSize.x, m_compartmentSize.y, m_compartmentSize.z };
                if (DragFloat3Colored("##CompSize", size, 0.1f))
                {
                    // Не даём размеру стать отрицательным или нулевым
                    m_compartmentSize = {
                        fmaxf(0.5f, size[0]),
                        fmaxf(0.5f, size[1]),
                        fmaxf(0.5f, size[2])
                    };
                    XMFLOAT3 oldMin = comp->localMin;
                    XMFLOAT3 oldMax = comp->localMax;
                    UpdateCompartmentFromCache(comp);
                    UpdateBreachesOnResize(comp, oldMin, oldMax); // <--- добавьте этот вызов
                }

                // Пресеты размеров
                ImGui::Spacing();
                ImGui::TextColored(COL_TEXT_DIM, " Preset Sizes:");
                if (ImGui::SmallButton("Small (1×1×1)")) { m_compartmentSize = { 1,1,1 }; UpdateCompartmentFromCache(comp); }
                ImGui::SameLine();
                if (ImGui::SmallButton("Medium (2×2×3)")) { m_compartmentSize = { 2,2,3 }; UpdateCompartmentFromCache(comp); }
                ImGui::SameLine();
                if (ImGui::SmallButton("Large (4×3×5)")) { m_compartmentSize = { 4,3,5 }; UpdateCompartmentFromCache(comp); }
            }
            else
            {
                // 🔹 Старый режим: прямой редактирование Min/Max
                ImGui::TextColored(COL_TEXT_DIM, " Min Bounds (Local)");
                float min[3] = { comp->localMin.x, comp->localMin.y, comp->localMin.z };
                if (DragFloat3Colored("##CompMin", min, 0.1f))
                {
                    comp->localMin = { min[0], min[1], min[2] };
                    comp->RecalculateVolume();
                }

                ImGui::Spacing();
                ImGui::TextColored(COL_TEXT_DIM, " Max Bounds (Local)");
                float max[3] = { comp->localMax.x, comp->localMax.y, comp->localMax.z };
                if (DragFloat3Colored("##CompMax", max, 0.1f))
                {
                    comp->localMax = { max[0], max[1], max[2] };
                    comp->RecalculateVolume();
                }
            }

            // 🔹 Кнопки быстрого позиционирования (только для режима Center+Size)
            if (useCenterSize)
            {
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                ImGui::TextColored(COL_TEXT_DIM, " Quick Position (relative to submarine):");

                // Кнопки-пресеты для позиционирования
                const float btnW = (ImGui::GetContentRegionAvail().x - 8.0f) / 3.0f;

                // Перед/Центр/Зад
                if (ImGui::Button("Front", ImVec2(btnW, 0))) { m_compartmentCenter.z = 4.0f; UpdateCompartmentFromCache(comp); }
                ImGui::SameLine(0, 4);
                if (ImGui::Button("Center", ImVec2(btnW, 0))) { m_compartmentCenter = { 0,0,0 }; UpdateCompartmentFromCache(comp); }
                ImGui::SameLine(0, 4);
                if (ImGui::Button("Rear", ImVec2(btnW, 0))) { m_compartmentCenter.z = -4.0f; UpdateCompartmentFromCache(comp); }

                // Верх/Средина/Низ
                if (ImGui::Button("Top", ImVec2(btnW, 0))) { m_compartmentCenter.y = 2.0f; UpdateCompartmentFromCache(comp); }
                ImGui::SameLine(0, 4);
                if (ImGui::Button("Middle", ImVec2(btnW, 0))) { m_compartmentCenter.y = 0.0f; UpdateCompartmentFromCache(comp); }
                ImGui::SameLine(0, 4);
                if (ImGui::Button("Bottom", ImVec2(btnW, 0))) { m_compartmentCenter.y = -2.0f; UpdateCompartmentFromCache(comp); }

                // Лево/Центр/Право
                if (ImGui::Button("Left", ImVec2(btnW, 0))) { m_compartmentCenter.x = -2.0f; UpdateCompartmentFromCache(comp); }
                ImGui::SameLine(0, 4);
                if (ImGui::Button("Center X", ImVec2(btnW, 0))) { m_compartmentCenter.x = 0.0f; UpdateCompartmentFromCache(comp); }
                ImGui::SameLine(0, 4);
                if (ImGui::Button("Right", ImVec2(btnW, 0))) { m_compartmentCenter.x = 2.0f; UpdateCompartmentFromCache(comp); }
            }

            // 🔹 Визуализация в 3D: кнопка включения гизмо
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            bool gizmoEnabled = (m_compartmentEditMode != CompartmentEditMode::None && m_editingCompartment == entity);
            if (ImGui::Checkbox("🖱️ Edit in Viewport (Gizmo)", &gizmoEnabled))
            {
                if (gizmoEnabled)
                    StartCompartmentEdit(entity);
                else
                    m_compartmentEditMode = CompartmentEditMode::None;
            }

            if (gizmoEnabled)
            {
                ImGui::SameLine();
                // Переключатель: перемещение или масштабирование
                static bool isMoveMode = true;
                if (ImGui::SmallButton(isMoveMode ? "Mode: Move" : "Mode: Scale"))
                    isMoveMode = !isMoveMode;

                m_compartmentEditMode = isMoveMode ? CompartmentEditMode::Move : CompartmentEditMode::Resize;

                ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f),
                    "  ← Drag axes in viewport to move/resize");
            }

            ImGui::Spacing();
            ImGui::Text("Volume: %.2f m³", comp->volume);

            // ── Состояние затопления ───────────────────────────────────
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextColored(COL_TEXT_DIM, " Flooding Status");

            float waterPercent = (comp->waterVolume / comp->volume) * 100.0f;
            ImGui::ProgressBar(waterPercent / 100.0f, ImVec2(-1, 0),
                (std::to_string((int)waterPercent) + "%% flooded").c_str());

            ImGui::Text("Water Level: %.2f m", comp->waterLevel);
            ImGui::Text("Breaches: %zu", comp->breaches.size());

            // ── Список пробоин ──────────────────────────────────────────
            if (comp->breaches.size() > 0)
            {
                ImGui::Spacing();
                ImGui::TextColored(COL_TEXT_DIM, " Breach List:");
                for (size_t i = 0; i < comp->breaches.size(); ++i)
                {
                    auto& breach = comp->breaches[i];
                    ImGui::PushID((int)i);

                    ImGui::Text("[%zu] Size: %.2fm Flow: %.3f m³/s %s",
                        i, breach.size, breach.flowRate,
                        breach.isSealed ? "(SEALED)" : "");

                    ImGui::SameLine();
                    if (!breach.isSealed && ImGui::SmallButton("Seal"))
                    {
                        FloodingSystem::SealBreach(registry, entity, i);
                    }

                    ImGui::PopID();
                }
            }

            // ── Кнопка создания пробоины (для теста) ───────────────────
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            static float testBreachSize = 0.5f;
            ImGui::DragFloat("Test Breach Size", &testBreachSize, 0.1f, 0.1f, 2.0f);

            if (ImGui::Button(" Create Test Breach", ImVec2(-1, 0)))
            {
                // 🔥 ВАЖНО: Пробоина в ЛОКАЛЬНЫХ координатах отсека!
                // Создаём в нижней части отсека
                XMFLOAT3 localBreachPos = {
                    (comp->localMin.x + comp->localMax.x) * 0.5f,  // Center X
                    comp->localMin.y,                        // 🔥 У ДНА отсека
                    (comp->localMin.z + comp->localMax.z) * 0.5f   // Center Z
                };

                FloodingSystem::CreateBreach(registry, entity, localBreachPos, testBreachSize);

                OutputDebugStringA(
                    ("✅ Created breach at local pos: " +
                        std::to_string(localBreachPos.x) + ", " +
                        std::to_string(localBreachPos.y) + ", " +
                        std::to_string(localBreachPos.z) + "\n").c_str()
                );
            }

            // ── Настройки герметичности и систем ────────────────────────
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Checkbox("Sealed (Doors Closed)", &comp->isSealed);
            ImGui::Checkbox("Has Power", &comp->hasPower);

            ImGui::DragFloat("Pump Rate (m³/s)", &comp->pumpRate, 0.01f, 0.0f, 1.0f);

            // ── Удаление компонента ─────────────────────────────────────
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Button, COL_RED);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
            if (ImGui::Button(" Remove Compartment Component", ImVec2(-1, 0)))
            {
                registry.RemoveComponent<CompartmentComponent>(entity);
            }
            ImGui::PopStyleColor(2);
        }
    }

    // ── LightComponent ────────────────────────────────────────────────────────
    if (registry.HasComponent<LightComponent>(entity))
    {
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.10f, 0.30f, 0.10f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.14f, 0.40f, 0.14f, 1.0f));
        bool open = ImGui::CollapsingHeader("  Light Component##lc", ImGuiTreeNodeFlags_DefaultOpen);
        ImGui::PopStyleColor(2);

        if (open)
        {
            auto* light = registry.GetComponent<LightComponent>(entity);

            ImGui::Spacing();
            float col[4] = { light->color.x, light->color.y, light->color.z, light->color.w };
            ImGui::TextColored(COL_TEXT_DIM, " Color  (w = intensity)");
            ImGui::SetNextItemWidth(-1);
            if (ImGui::ColorEdit4("##LCColor", col))
                light->color = { col[0], col[1], col[2], col[3] };

            ImGui::Spacing();
            ImGui::TextColored(COL_TEXT_DIM, " Radius");
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat("##LCRadius", &light->radius, 0.1f, 0.0f, 500.0f);

            ImGui::Spacing();
            ImGui::Checkbox("Enabled##LC", &light->enabled);

            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Button, COL_RED);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
            if (ImGui::Button("Remove##LightComp", ImVec2(-1, 0)))
                registry.RemoveComponent<LightComponent>(entity);
            ImGui::PopStyleColor(2);
            ImGui::Spacing();
        }
    }


    // ── SubmarineComponent ────────────────────────────────────────────────────────
    if (registry.HasComponent<SubmarineComponent>(entity))
    {
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.10f, 0.20f, 0.40f, 1.0f)); // 🔵 Синий для подлодки
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.14f, 0.28f, 0.55f, 1.0f));
        bool open = ImGui::CollapsingHeader("  Submarine Component##sub", ImGuiTreeNodeFlags_DefaultOpen);
        ImGui::PopStyleColor(2);

        if (open)
        {
            auto* subComp = registry.GetComponent<SubmarineComponent>(entity);
            if (!subComp) { ImGui::TextColored(COL_RED, "Component error"); return; }

            ImGui::Spacing();

            // 🔹 Если это корень подлодки
            if (subComp->submarineRoot == entity)
            {
                ImGui::TextColored(COL_ACCENT, "🚢 Submarine Root");
                ImGui::Spacing();

                ImGui::TextColored(COL_TEXT_DIM, "Entity ID: %u", entity);
                ImGui::TextColored(COL_GREEN, "This is the submarine root (self-reference)");

                // Список всех отсеков
                ImGui::TextColored(COL_TEXT_DIM, " Compartments (%zu):", subComp->allCompartments.size());
                for (size_t i = 0; i < subComp->allCompartments.size(); ++i)
                {
                    Entity compEnt = subComp->allCompartments[i];
                    auto* compTransform = registry.GetComponent<TransformComponent>(compEnt);
                    auto* compData = registry.GetComponent<CompartmentComponent>(compEnt);

                    char label[128];
                    if (compData)
                    {
                        float floodPercent = (compData->volume > 0) ? (compData->waterVolume / compData->volume) * 100.f : 0.f;
                        snprintf(label, sizeof(label), "  [%zu] Compartment #%u (%.0f%% flooded)",
                            i, compEnt, floodPercent);
                    }
                    else
                    {
                        snprintf(label, sizeof(label), "  [%zu] Compartment #%u (no data)", i, compEnt);
                    }

                    // Подсветка, если этот отсек выбран
                    bool isSelected = (m_selectedType == SelectedType::Object && m_selectedEntity == compEnt);
                    if (isSelected) ImGui::PushStyleColor(ImGuiCol_Header, COL_SELECTED);

                    if (ImGui::Selectable(label, isSelected))
                    {
                        SetSelection(SelectedType::Object, compEnt);
                    }

                    if (isSelected) ImGui::PopStyleColor();

                    // Кнопка быстрого перехода
                    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60);
                    if (ImGui::SmallButton("Focus##Comp"))
                    {
                        if (compTransform)
                        {
                            // Можно добавить камеру, которая летит к отсеку
                            // m_camera->SetTarget(compTransform->position);
                        }
                    }
                }

                // Кнопка добавить отсек
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                if (ImGui::Button("➕ Add New Compartment", ImVec2(-1, 0)))
                {
                    CreateNewCompartment(registry, entity);
                }

                // Кнопка проверить целостность
                ImGui::SameLine();
                if (ImGui::Button("🔍 Check Integrity"))
                {
                    
                   
                }
            }
            else
            {
                // 🔹 Если это отсек, привязанный к подлодке
                ImGui::TextColored(COL_ACCENT, "📦 Compartment (linked)");
                ImGui::Spacing();

                
                ImGui::TextColored(COL_TEXT_DIM, "Parent Submarine Entity ID:");
                ImGui::SetNextItemWidth(-1);

                Entity parentID = subComp->submarineRoot;
                if (ImGui::DragInt("##ParentSubID", (int*)&parentID, 1.0f, 0, 10000))
                {
                    subComp->submarineRoot = parentID;
                    OutputDebugStringA(("🔧 Updated submarineRoot to: " + std::to_string(parentID) + "\n").c_str());
                }

                ImGui::SameLine();
                if (ImGui::SmallButton("Set to Self"))
                {
                    subComp->submarineRoot = entity;
                    OutputDebugStringA(("✅ Set submarineRoot to self (entity " + std::to_string(entity) + ")\n").c_str());
                }

                ImGui::Text("Current: %u", subComp->submarineRoot);
                // Кнопка открепить
                if (ImGui::Button("Unlink from Submarine", ImVec2(-1, 0)))
                {
                    subComp->submarineRoot = INVALID_ENTITY;
                    // Можно также удалить из списка allCompartments у родителя
                }

                // 🔹 Редактирование локальных границ (если есть CompartmentComponent)
                auto* compData = registry.GetComponent<CompartmentComponent>(entity);
                if (compData)
                {
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();

                    ImGui::TextColored(COL_TEXT_DIM, " Local Bounds (relative to submarine)");
                    float min[3] = { compData->localMin.x, compData->localMin.y, compData->localMin.z };
                    float max[3] = { compData->localMax.x, compData->localMax.y, compData->localMax.z };

                    if (ImGui::DragFloat3("##CompMin", min, 0.1f))
                    {
                        compData->localMin = { min[0], min[1], min[2] };
                        compData->RecalculateVolume();
                    }
                    if (ImGui::DragFloat3("##CompMax", max, 0.1f))
                    {
                        compData->localMax = { max[0], max[1], max[2] };
                        compData->RecalculateVolume();
                    }

                    ImGui::Text("Volume: %.2f m³", compData->volume);
                }
            }

            // Кнопка удаления компонента
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Button, COL_RED);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
            if (ImGui::Button(" Remove Submarine Component", ImVec2(-1, 0)))
            {
                // Если это корень — удаляем связь у всех отсеков
                if (subComp->submarineRoot == entity)
                {
                    for (Entity compEnt : subComp->allCompartments)
                    {
                        auto* link = registry.GetComponent<SubmarineComponent>(compEnt);
                        if (link) link->submarineRoot = INVALID_ENTITY;
                    }
                }
                registry.RemoveComponent<SubmarineComponent>(entity);
            }
            ImGui::PopStyleColor(2);
        }
    }


    if (registry.HasComponent<RigidBodyComponent>(entity))
    {
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.10f, 0.20f, 0.30f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.14f, 0.28f, 0.40f, 1.0f));
        bool open = ImGui::CollapsingHeader("  RigidBody Component##rbc", ImGuiTreeNodeFlags_DefaultOpen);
        ImGui::PopStyleColor(2);

        if (open)
        {
            auto* rb = registry.GetComponent<RigidBodyComponent>(entity);
            bool needsRecreate = false;

            ImGui::Spacing();

            // 🔥 1. ПЕРЕКЛЮЧАТЕЛЬ COMPOUND MODE
            ImGui::Checkbox(" Compound Collision", &rb->isCompound);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Enable to build collision from multiple primitive shapes");

            if (rb->isCompound)
            {
                // ═══════════════════════════════════════════════════════════════
                // 🔥 РЕЖИМ СОСТАВНОЙ КОЛЛИЗИИ
                // ═══════════════════════════════════════════════════════════════
                ImGui::Separator();
                ImGui::TextColored(COL_TEXT_DIM, " Compound Elements");

                // Кнопки управления списком
                if (ImGui::Button("+ Add Element", ImVec2(-1, 0)))
                {
                    RigidBodyComponent::CompoundElement newElem;
                    newElem.type = RigidBodyComponent::CompoundElement::Box;
                    newElem.localPosition = { 0, 0, 0 };
                    newElem.localRotation = { 0, 0, 0, 1 }; // Euler углы
                    newElem.size = 1.0f;
                    newElem.radius = 0.5f;
                    newElem.height = 1.0f;
                    rb->compoundElements.push_back(newElem);
                    needsRecreate = true;
                }

                ImGui::Spacing();

                // Отрисовка списка элементов
                // Стало (ПРАВИЛЬНО):
                for (size_t i = 0; i < rb->compoundElements.size(); ++i)
                {
                    auto& elem = rb->compoundElements[i]; // <-- Ссылка! Изменения применятся к оригиналу

                    ImGui::PushID(i); // Важно для уникальности ID виджетов

                    // Заголовок элемента
                    char label[64];
                    sprintf_s(label, "Element %zu", i);

                    bool open = ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen);

                    if (open)
                    {
                        ImGui::Indent();

                        // Тип формы
                        const char* types[] = { "Box", "Sphere", "Cylinder", "Capsule" };
                        int typeIdx = (int)elem.type;
                        if (ImGui::Combo("Type", &typeIdx, types, 4))
                        {
                            elem.type = (RigidBodyComponent::CompoundElement::ShapeType)typeIdx;
                            needsRecreate = true;
                        }

                        // Позиция
                        if (ImGui::DragFloat3("Position", &elem.localPosition.x, 0.1f))
                        {
                            needsRecreate = true; // Помечаем, что нужно пересоздать тело
                        }

                        // Вращение (если используете Euler)
                        // Если храните кватернион, нужно конвертировать туда-сюда для DragFloat3
                        XMFLOAT3 euler = QuaternionToEuler(XMFloat4ToBtQuaternion(elem.localRotation));
                        if (ImGui::DragFloat3("Rotation", &euler.x, 0.05f))
                        {
                            XMFloat4ToBtQuaternion(elem.localRotation) = EulerToQuaternion(euler);
                            needsRecreate = true;
                        }

                        // Размеры в зависимости от типа
                        bool sizeChanged = false;
                        if (elem.type == RigidBodyComponent::CompoundElement::Box)
                        {
                            if (ImGui::DragFloat("Size", &elem.size, 0.1f, 0.1f, 10.0f)) sizeChanged = true;
                        }
                        else if (elem.type == RigidBodyComponent::CompoundElement::Sphere)
                        {
                            if (ImGui::DragFloat("Radius", &elem.radius, 0.1f, 0.1f, 10.0f)) sizeChanged = true;
                        }
                        else // Cylinder, Capsule
                        {
                            if (ImGui::DragFloat("Radius", &elem.radius, 0.1f, 0.1f, 10.0f)) sizeChanged = true;
                            if (ImGui::DragFloat("Height", &elem.height, 0.1f, 0.1f, 10.0f)) sizeChanged = true;
                        }

                        if (sizeChanged) needsRecreate = true;

                        // Кнопка удаления
                        ImGui::Spacing();
                        if (ImGui::Button("Delete", ImVec2(-1, 0)))
                        {
                            rb->compoundElements.erase(rb->compoundElements.begin() + i);
                            needsRecreate = true;
                            ImGui::PopID();
                            break; // Выход из цикла, так как индекс сместился
                        }
                        ImGui::Spacing();
                        ImGui::Separator();
                        ImGui::TextColored(COL_TEXT_DIM, " Advanced Size & Offsets");
                        ImGui::Indent();

                        bool shapeChanged = false;

                        // В зависимости от типа фигуры показываем разные настройки
                        if (elem.type == RigidBodyComponent::CompoundElement::Box)
                        {
                            // Для бокса: отдельные оффсеты для Min и Max (как у Compartment)
                            // Это позволяет сдвигать грани независимо от центра
                            ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), " Half-Extents Adjustment");

                            // Оффсет минимума (отрицательная сторона)
                            if (ImGui::DragFloat3(" Min Offset (Neg)", &elem.boxOffsetMin.x, 0.05f, -100.0f, 100.0f))
                            {
                                shapeChanged = true;
                            }
                            ImGui::SameLine();
                            HelpMarker("Сдвигает отрицательные грани внутрь/наружу");

                            // Оффсет максимума (положительная сторона)
                            if (ImGui::DragFloat3(" Max Offset (Pos)", &elem.boxOffsetMax.x, 0.05f, -100.0f, 100.0f))
                            {
                                shapeChanged = true;
                            }
                            ImGui::SameLine();
                            HelpMarker("Сдвигает положительные грани внутрь/наружу");
                        }
                        else if (elem.type == RigidBodyComponent::CompoundElement::Cylinder ||
                            elem.type == RigidBodyComponent::CompoundElement::Capsule)
                        {
                            // Для цилиндров и капсул: радиус и высота могут иметь свои смещения
                            ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), " Dimension Offsets");

                            float radiusOffset = elem.radiusOffset;
                            if (ImGui::DragFloat(" Radius Offset", &radiusOffset, 0.05f, -0.5f, 0.5f))
                            {
                                elem.radiusOffset = radiusOffset;
                                shapeChanged = true;
                            }

                            float heightOffsetTop = elem.heightOffsetTop;
                            if (ImGui::DragFloat(" Top Height Offset", &heightOffsetTop, 0.05f, -1.0f, 1.0f))
                            {
                                elem.heightOffsetTop = heightOffsetTop;
                                shapeChanged = true;
                            }

                            float heightOffsetBottom = elem.heightOffsetBottom;
                            if (ImGui::DragFloat(" Bottom Height Offset", &heightOffsetBottom, 0.05f, -1.0f, 1.0f))
                            {
                                elem.heightOffsetBottom = heightOffsetBottom;
                                shapeChanged = true;
                            }
                        }

                        ImGui::Unindent();

                        // Если что-то изменилось в размерах/оффсетах — пересоздаем тело
                        if (shapeChanged)
                        {
                            needsRecreate = true;
                        }

                        ImGui::Unindent();
                    }

                    ImGui::PopID();

                }

                if (needsRecreate && rb->body)
                {
                    RefreshRigidBody(registry, entity, rb);
                }

            }
            else
            {
                // ═══════════════════════════════════════════════════════════════
                // 🔥 РЕЖИМ ОДИНОЧНОЙ КОЛЛИЗИИ (Старый код)
                // ═══════════════════════════════════════════════════════════════
                ImGui::Separator();
                ImGui::TextColored(COL_TEXT_DIM, " Mass");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::DragFloat("##RBMass", &rb->mass, 0.1f, 0.0f, 10000.0f))
                {
                    if (rb->body) RefreshRigidBody(registry, entity, rb);
                }

                ImGui::Spacing();
                ImGui::TextColored(COL_TEXT_DIM, " Shape Type");
                const char* shapeNames[] = {
                    "Box", "Sphere", "Cylinder", "Capsule",
                    "Plane", "ConvexHull", "TriangleMesh"
                };
                int shapeIdx = static_cast<int>(rb->shapeType);
                if (ImGui::Combo("##RBShape", &shapeIdx, shapeNames, 7))
                {
                    rb->shapeType = static_cast<RigidBodyComponent::ShapeType>(shapeIdx);
                    if (rb->body) RefreshRigidBody(registry, entity, rb);
                }

                ImGui::Spacing();

                // ПАРАМЕТРЫ ЗАВИСЯТ ОТ ТИПА КОЛЛИЗИИ
                switch (rb->shapeType)
                {
                case RigidBodyComponent::Sphere:
                {
                    auto* meshRenderer = registry.GetComponent<MeshRendererComponent>(entity);
                    if (ImGui::Checkbox(" Manual Size##SphereManual", &rb->useManualSize))
                    {
                        if (rb->body) RefreshRigidBody(registry, entity, rb);
                    }
                    ImGui::SameLine();
                    ImGui::TextColored(COL_TEXT_DIM, "(ignore mesh bounds)");

                    if (rb->useManualSize)
                    {
                        ImGui::Spacing();
                        ImGui::TextColored(COL_TEXT_DIM, " Radius");
                        ImGui::SetNextItemWidth(-1);
                        float radius = rb->shapeRadius;
                        if (ImGui::DragFloat("##Radius", &radius, 0.05f, 0.01f, 100.0f))
                        {
                            rb->shapeRadius = radius;
                            if (rb->body) RefreshRigidBody(registry, entity, rb);
                        }
                    }
                    else
                    {
                        if (meshRenderer && meshRenderer->model)
                        {
                            XMFLOAT3 center;
                            float meshRadius;
                            meshRenderer->model->GetBoundingSphere(&center, &meshRadius);
                            rb->shapeRadius = meshRadius;
                            ImGui::TextColored(COL_TEXT_DIM, " Auto radius from mesh: %.3f", meshRadius);
                        }
                        else
                        {
                            ImGui::TextColored(COL_TEXT_DIM, " No mesh - using shapeRadius: %.3f", rb->shapeRadius);
                        }
                    }
                    break;
                }
                case RigidBodyComponent::Box:
                {
                    auto* meshRenderer = registry.GetComponent<MeshRendererComponent>(entity);
                    if (!rb->useManualSize && meshRenderer && meshRenderer->model)
                    {
                        XMFLOAT3 min, max;
                        meshRenderer->model->GetBoundingBox(&min, &max);
                        rb->shapeSize = btVector3(
                            (max.x - min.x) * 0.5f,
                            (max.y - min.y) * 0.5f,
                            (max.z - min.z) * 0.5f
                        );
                        ImGui::TextColored(COL_TEXT_DIM, " Auto box from mesh: (%.2f, %.2f, %.2f)",
                            rb->shapeSize.x(), rb->shapeSize.y(), rb->shapeSize.z());
                    }

                    ImGui::TextColored(COL_TEXT_DIM, " Half Extents (X, Y, Z)");
                    float size[3] = { rb->shapeSize.x(), rb->shapeSize.y(), rb->shapeSize.z() };
                    if (ImGui::DragFloat3("##BoxSize", size, 0.05f, 0.01f, 100.0f))
                    {
                        rb->shapeSize = btVector3(size[0], size[1], size[2]);
                        rb->useManualSize = true;
                        if (rb->body) RefreshRigidBody(registry, entity, rb);
                    }
                    break;
                }
                case RigidBodyComponent::Cylinder:
                {
                    float radius = rb->shapeRadius;
                    float height = rb->shapeSize.y();
                    bool changed = false;
                    ImGui::TextColored(COL_TEXT_DIM, " Radius");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat("##CylRadius", &radius, 0.05f, 0.01f, 100.0f)) { rb->shapeRadius = radius; changed = true; }
                    ImGui::Spacing();
                    ImGui::TextColored(COL_TEXT_DIM, " Height");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat("##CylHeight", &height, 0.05f, 0.01f, 100.0f)) { rb->shapeSize.setY(height); changed = true; }
                    if (changed && rb->body) RefreshRigidBody(registry, entity, rb);
                    break;
                }
                case RigidBodyComponent::Capsule:
                {
                    float radius = rb->shapeRadius;
                    float height = rb->shapeSize.y();
                    bool changed = false;
                    ImGui::TextColored(COL_TEXT_DIM, " Radius");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat("##CapsRadius", &radius, 0.05f, 0.01f, 100.0f)) { rb->shapeRadius = radius; changed = true; }
                    ImGui::Spacing();
                    ImGui::TextColored(COL_TEXT_DIM, " Height");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat("##CapsHeight", &height, 0.05f, 0.01f, 100.0f)) { rb->shapeSize.setY(height); changed = true; }
                    if (changed && rb->body) RefreshRigidBody(registry, entity, rb);
                    break;
                }
                case RigidBodyComponent::ConvexHull:
                {
                    ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), " Auto-calculated from mesh geometry");
                    auto* meshRenderer = registry.GetComponent<MeshRendererComponent>(entity);
                    if (!meshRenderer || !meshRenderer->model)
                        ImGui::TextColored(COL_RED, "⚠️ No mesh assigned");
                    break;
                }
                case RigidBodyComponent::TriangleMesh:
                    ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), " Exact mesh collision (static only)");
                    rb->mass = 0.0f;
                    break;
                case RigidBodyComponent::Plane:
                    ImGui::TextColored(COL_TEXT_DIM, " Y");
                    ImGui::SetNextItemWidth(-1);
                    float y = rb->shapeSize.y();
                    if (ImGui::DragFloat("##Y", &y, 0.05f, 0.01f, 100.0f))
                    {
                        rb->shapeSize.setY(y);
                        if (rb->body) RefreshRigidBody(registry, entity, rb);
                    }
                    ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), " Infinite plane at Y=0");
                    rb->mass = 0.0f;
                    break;
                }

                ImGui::Spacing();
                ImGui::Checkbox(" Enable CCD##RB", &rb->enableCCD);
            }
        }
    }

    if (registry.HasComponent<BuoyancyComponent>(entity))
    {
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.10f, 0.30f, 0.20f, 1.0f)); // 💚 Зеленоватый оттенок для воды
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.14f, 0.40f, 0.28f, 1.0f));
        bool open = ImGui::CollapsingHeader("  Buoyancy Component##buoy", ImGuiTreeNodeFlags_DefaultOpen);
        ImGui::PopStyleColor(2);

        if (open)
        {
            auto* buoy = registry.GetComponent<BuoyancyComponent>(entity);
            if (!buoy) { ImGui::TextColored(COL_RED, "Component error"); return; }

            ImGui::Spacing();

            // ── Плотность жидкости ─────────────────────────────────────
            ImGui::TextColored(COL_TEXT_DIM, " Fluid Density (kg/m³)");
            ImGui::SetNextItemWidth(-1);
            if (ImGui::DragFloat("##FluidDensity", &buoy->fluidDensity, 10.0f, 0.0f, 5000.0f, "%.1f"))
            {
                // Плотность можно менять на лету, пересоздавать тело не нужно
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Reset##Density")) buoy->fluidDensity = 1000.0f;
            ImGui::TextColored(COL_TEXT_DIM, "  Water: 1000, Sea: 1025, Oil: ~900");

            ImGui::Spacing();

            // ── Объём объекта ──────────────────────────────────────────
            ImGui::TextColored(COL_TEXT_DIM, " Volume (m³)");
            ImGui::SetNextItemWidth(-1);
            float oldVolume = buoy->volume;
            if (ImGui::DragFloat("##Volume", &buoy->volume, 0.05f, 0.01f, 1000.0f, "%.2f"))
            {
                // Объём влияет на силу Архимеда — можно менять динамически
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Auto##Volume"))
            {
                // 🔹 Попытка авто-расчёта из меша (если есть)
                auto* mesh = registry.GetComponent<MeshRendererComponent>(entity);
                if (mesh && mesh->model)
                {
                    XMFLOAT3 min, max;
                    mesh->model->GetBoundingBox(&min, &max);
                    buoy->volume = (max.x - min.x) * (max.y - min.y) * (max.z - min.z);
                    buoy->height = max.y - min.y; // Обновляем и высоту
                }
            }

            ImGui::Spacing();

            // ── Высота объекта (для расчёта % погружения) ───────────────
            ImGui::TextColored(COL_TEXT_DIM, " Height (m) — для расчёта погружения");
            ImGui::SetNextItemWidth(-1);
            if (ImGui::DragFloat("##Height", &buoy->height, 0.05f, 0.01f, 100.0f, "%.2f"))
            {
                // Высота используется только для расчёта submersionRatio
            }

            ImGui::Spacing();

            // ── Множитель плавучести (тонкая настройка) ─────────────────
            ImGui::TextColored(COL_TEXT_DIM, " Buoyancy Multiplier (тонкая настройка)");
            ImGui::SetNextItemWidth(-1);
            if (ImGui::SliderFloat("##BuoyMult", &buoy->buoyancyMultiplier, 0.1f, 2.0f, "%.2f"))
            {
                // <1.0 = тонет быстрее, >1.0 = всплывает активнее
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Reset##Mult")) buoy->buoyancyMultiplier = 1.0f;
            ImGui::TextColored(COL_TEXT_DIM, "  0.5 = тяжелее, 1.0 = нормально, 1.5 = легче");

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // ── ОТЛАДОЧНАЯ ИНФОРМАЦИЯ (только чтение) ──────────────────
            ImGui::TextColored(COL_TEXT_DIM, " Debug Info (read-only)");
            ImGui::Indent();
            ImGui::Text("Smoothed Submersion: %.3f", buoy->smoothedSubmersion);
            // Можно добавить вывод текущей силы, если передаёшь её из PhysicsSystem
            ImGui::Unindent();

            ImGui::Spacing();

            // ── Кнопка удаления компонента ─────────────────────────────
            ImGui::PushStyleColor(ImGuiCol_Button, COL_RED);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
            if (ImGui::Button(" Remove Buoyancy Component", ImVec2(-1, 0)))
            {
                registry.RemoveComponent<BuoyancyComponent>(entity);
                // Не нужно пересоздавать физическое тело — плавучесть не в Bullet
            }
            ImGui::PopStyleColor(2);
        }
    }


    // ── Кнопка "+ Add Component" ──────────────────────────────────────────────
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.16f, 0.16f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
    if (ImGui::Button("  + Add Component", ImVec2(-1, 0)))
        ImGui::OpenPopup("##AddComponentPopup");


    ImGui::PopStyleColor(2);

    if (ImGui::BeginPopup("##AddComponentPopup"))
    {
        ImGui::TextColored(COL_TEXT_DIM, "Available Components");
        ImGui::Separator();
        ImGui::Spacing();

        // ── Submarine Component ──────────────────────────────────────────────────
        {
            const bool hasSub = registry.HasComponent<SubmarineComponent>(entity);
            ImGui::BeginDisabled(hasSub);
            if (ImGui::Selectable("  Submarine Component"))
            {
                SubmarineComponent sub{};
                sub.submarineRoot = entity;
                registry.AddComponent<SubmarineComponent>(entity, sub);
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
            if (hasSub)
            {
                ImGui::SameLine();
                ImGui::TextColored(COL_TEXT_DIM, "(already added)");
            }
        }

        // ── Light Component ─────────────────────────────────────────────────────
        {
            const bool hasLight = registry.HasComponent<LightComponent>(entity);
            ImGui::BeginDisabled(hasLight);
            if (ImGui::Selectable("  Light Component"))
            {
                registry.AddComponent<LightComponent>(entity, LightComponent{});
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
            if (hasLight)
            {
                ImGui::SameLine();
                ImGui::TextColored(COL_TEXT_DIM, "(already added)");
            }
        }

        // ── RigidBody Component ─────────────────────────────────────────────────
        {
            const bool hasRb = registry.HasComponent<RigidBodyComponent>(entity);
            ImGui::BeginDisabled(hasRb);
            if (ImGui::Selectable("  RigidBody Component"))
            {
                registry.AddComponent<RigidBodyComponent>(entity, RigidBodyComponent{});
                auto* rb = registry.GetComponent<RigidBodyComponent>(entity);
                if (rb && !rb->body)
                    PhysicsSystem::CreateRigidBody(registry, entity);
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
            if (hasRb)
            {
                ImGui::SameLine();
                ImGui::TextColored(COL_TEXT_DIM, "(already added)");
            }
        }

        // ── Buoyancy Component ──────────────────────────────────────────────────
        {
            const bool hasBuoy = registry.HasComponent<BuoyancyComponent>(entity);
            ImGui::BeginDisabled(hasBuoy);
            if (ImGui::Selectable("  Buoyancy Component"))
            {
                registry.AddComponent<BuoyancyComponent>(entity, BuoyancyComponent{});
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
            if (hasBuoy)
            {
                ImGui::SameLine();
                ImGui::TextColored(COL_TEXT_DIM, "(already added)");
            }
        }

        // ── Compartment Component ───────────────────────────────────────────────
        {
            const bool hasComp = registry.HasComponent<CompartmentComponent>(entity);
            ImGui::BeginDisabled(hasComp);
            if (ImGui::Selectable("  Compartment Component"))
            {
                CompartmentComponent comp{};
                comp.RecalculateVolume();
                registry.AddComponent<CompartmentComponent>(entity, comp);
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
            if (hasComp)
            {
                ImGui::SameLine();
                ImGui::TextColored(COL_TEXT_DIM, "(already added)");
            }
        }

        ImGui::Spacing();
        ImGui::EndPopup();
    }
}

//=============================================================================
// RenderSidebar — фиксированная правая боковая панель в стиле UE5
//=============================================================================
void EditorUI::RenderSidebar(SceneManager& sceneManager)
{
    const ImGuiIO& io = ImGui::GetIO();
    const float    menuBarH = m_menuBarHeight > 0.0f ? m_menuBarHeight : 20.0f;
    const float    height = io.DisplaySize.y - menuBarH;
    const float    sidebarW = SidebarWidth();
    const float    posX = io.DisplaySize.x - sidebarW;

    ImGui::SetNextWindowPos(ImVec2(posX, menuBarH), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(sidebarW, height), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.96f);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##Sidebar", nullptr, flags);
    ImGui::PopStyleVar();

    const float headerH = std::floor(100.0f * ScaleY());

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.07f, 0.07f, 0.07f, 1.0f));
    ImGui::BeginChild("##SidebarHeader", ImVec2(-1, headerH), false);
    ImGui::SetCursorPos(ImVec2(10, std::floor(9.0f * ScaleY())));
    ImGui::TextColored(COL_ACCENT, "◈");
    ImGui::SameLine();
    ImGui::Text("Scene Editor");
    ImGui::SameLine(sidebarW - std::floor(70.0f * ScaleX()));
    ImGui::TextColored(COL_TEXT_DIM, "%zu obj", sceneManager.GetObjectCount());
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

    const ImGuiTabBarFlags tabFlags =
        ImGuiTabBarFlags_FittingPolicyResizeDown |
        ImGuiTabBarFlags_NoTooltip;

    auto* clipShader = ShaderManager::Get().GetAs<ClipPlaneShaderClass>("clip");
    RenderKnifeTool(sceneManager, clipShader);

    if (ImGui::BeginTabBar("##SidebarTabs", tabFlags))
    {
        if (ImGui::BeginTabItem("Outliner"))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
            RenderSceneOutliner(sceneManager);
            ImGui::PopStyleVar();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("🔬 Debug"))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));

            // Получаем настройки из ApplicationClass (см. ниже)
            extern PhysicsDebugSettings g_physicsDebugSettings;  // Временный хак
            RenderPhysicsDebugPanel();

            ImGui::PopStyleVar();
            ImGui::EndTabItem();
        }

       /* if (ImGui::BeginTabItem("Details"))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
            RenderDetailsPanel(sceneManager);
            ImGui::PopStyleVar();
            ImGui::EndTabItem();
        }*/
        if (ImGui::BeginTabItem("Spawn"))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
            RenderSpawnPanel(sceneManager);
            ImGui::PopStyleVar();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("World"))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
            RenderFogPanel(sceneManager);
            ImGui::PopStyleVar();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Content"))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
            RenderContentBrowser();
            ImGui::PopStyleVar();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Joint "))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
			RenderJointConnectionUI(sceneManager.GetECSRegistry(), m_selectedEntity);
            ImGui::PopStyleVar();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Submarine"))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
            RenderSubmarineBuilder(sceneManager);
            ImGui::PopStyleVar();
            ImGui::EndTabItem();
        }

     
        RenderShaderSelector(sceneManager);

        ImGui::EndTabBar();
    }
    ImGui::PopStyleVar();

    ImGui::End();

    const float overlayOffsetX = std::floor(10.0f * ScaleX());
    const float overlayOffsetY = menuBarH + std::floor(6.0f * ScaleY());
    ImGui::SetNextWindowPos(ImVec2(overlayOffsetX, overlayOffsetY), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.70f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
    if (ImGui::Begin("##Selection", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
    {
        switch (m_selectedType)
        {
        case SelectedType::Object:
            ImGui::TextColored(COL_ACCENT, "◈"); ImGui::SameLine();
            ImGui::Text("Object  entity=%u", m_selectedEntity);
            break;
        case SelectedType::Light:
            ImGui::TextColored(ImVec4(1, 0.9f, 0.3f, 1), "◉"); ImGui::SameLine();
            ImGui::Text("Light #%d", m_selectedLegacyIndex);
            break;
        case SelectedType::Sprite:
            ImGui::TextColored(ImVec4(0.3f, 0.8f, 1, 1), "◧"); ImGui::SameLine();
            ImGui::Text("Sprite #%d", m_selectedLegacyIndex);
            break;
        default:
            ImGui::TextColored(COL_TEXT_DIM, "Nothing selected");
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

//=============================================================================
// SetScenePreviewTexture
//=============================================================================
void EditorUI::SetScenePreviewTexture(ID3D11ShaderResourceView* srv)
{
    m_scenePreviewSRV = srv;
}

//=============================================================================
// RenderScenePreview — показывает render-to-texture в окошке поверх сцены
//=============================================================================
void EditorUI::RenderScenePreview()
{
    if (!m_scenePreviewSRV) return;

    const float previewW = std::floor(BASE_WIDTH * PREVIEW_WIDTH_RATIO * ScaleX());
    const float previewH = std::floor(BASE_HEIGHT * PREVIEW_HEIGHT_RATIO * ScaleY());
    const float menuBarH = m_menuBarHeight > 0.0f ? m_menuBarHeight : 20.0f;
    const float sidebarW = SidebarWidth();
    const float posX = ImGui::GetIO().DisplaySize.x - sidebarW - previewW - std::floor(8.0f * ScaleX());
    const float posY = menuBarH + std::floor(8.0f * ScaleY());

    ImGui::SetNextWindowPos(ImVec2(posX, posY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(previewW, previewH + std::floor(22.0f * ScaleY())), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.85f);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
    if (ImGui::Begin("##ScenePreview", nullptr, flags))
    {
        ImGui::TextColored(COL_TEXT_DIM, "Scene Preview");
        ImGui::Image((ImTextureID)m_scenePreviewSRV, ImVec2(previewW - 8.0f, previewH));
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

//=============================================================================
// RenderSpawnPanel — панель для спавна объектов
//=============================================================================
void EditorUI::RenderSpawnPanel(SceneManager& sceneManager)
{
    ImGui::Spacing();
    SectionHeader("Spawn Object");
    ImGui::Spacing();

    ImGui::TextColored(COL_TEXT_DIM, " Model path");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##SpawnModel", m_spawnModelPath, sizeof(m_spawnModelPath));

    // Проверяем расширение — для GLB текстура не нужна
    const std::string modelExt = fs::path(m_spawnModelPath).extension().string();
    const bool isGLB = (modelExt == ".glb" || modelExt == ".gltf");

    ImGui::Spacing();

    // Для GLB показываем подсказку вместо поля текстуры
    if (isGLB)
    {
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f),
            " GLB: textures embedded, no path needed");
    }
    else
    {
        ImGui::TextColored(COL_TEXT_DIM, " Texture path");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##SpawnTex", m_spawnTexturePath, sizeof(m_spawnTexturePath));
    }

    ImGui::Spacing();
    ImGui::TextColored(COL_TEXT_DIM, " Position");
    ImGui::SetNextItemWidth(-1);
    ImGui::DragFloat3("##SpawnPos", m_spawnPos, 0.1f);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextColored(COL_TEXT_DIM, " Model presets");
    for (int i = 0; i < PRESET_COUNT; ++i)
    {
        if (i > 0) ImGui::SameLine(0, 4);
        if (ImGui::SmallButton(MODEL_PRESETS[i]))
            strncpy_s(m_spawnModelPath, sizeof(m_spawnModelPath), MODEL_PRESETS[i], _TRUNCATE);
    }

    ImGui::Spacing();

    // Пресеты текстур только для не-GLB моделей
    if (!isGLB)
    {
        ImGui::TextColored(COL_TEXT_DIM, " Texture presets");
        for (int i = 0; i < PRESET_COUNT; ++i)
        {
            if (i > 0) ImGui::SameLine(0, 4);
            if (ImGui::SmallButton(TEXTURE_PRESETS[i]))
                strncpy_s(m_spawnTexturePath, sizeof(m_spawnTexturePath), TEXTURE_PRESETS[i], _TRUNCATE);
        }
        ImGui::Spacing();
    }

    ImGui::Separator();
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Button, COL_GREEN);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COL_GREEN_HOV);
    if (ImGui::Button("  Spawn  ", ImVec2(-1, 0)))
    {
        m_spawnRequest.pending = true;
        m_spawnRequest.modelPath = m_spawnModelPath;
        // Для GLB передаём пустой путь — модель возьмёт вшитые текстуры
        m_spawnRequest.texturePath = isGLB ? "" : m_spawnTexturePath;
        m_spawnRequest.posX = m_spawnPos[0];
        m_spawnRequest.posY = m_spawnPos[1];
        m_spawnRequest.posZ = m_spawnPos[2];
    }
    ImGui::PopStyleColor(2);
}

void EditorUI::RenderFogPanel(SceneManager& sceneManager)
{
    ImGui::Spacing();
    SectionHeader("Fog Settings");
    ImGui::Spacing();

    // ── Переключатель тумана ──────────────────────────────────────────────────
    bool fogEnabled = sceneManager.GetFogSettings().enabled;
    if (ImGui::Checkbox("Enable Fog", &fogEnabled))
        sceneManager.GetFogSettings().enabled = fogEnabled;

    ImGui::Spacing();

    // Блокируем остальные настройки, если туман выключен
    ImGui::BeginDisabled(!fogEnabled);

    auto fogColor = sceneManager.GetFogColor();
    auto fogStart = sceneManager.GetFogStart();
    auto fogEnd = sceneManager.GetFogEnd();

    float col[3] = { fogColor.x, fogColor.y, fogColor.z };
    ImGui::TextColored(COL_TEXT_DIM, " Fog Color");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::ColorEdit3("##FogColor", col))
        sceneManager.SetFogColor({ col[0], col[1], col[2] });

    ImGui::Spacing();
    ImGui::TextColored(COL_TEXT_DIM, " Fog Start");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::DragFloat("##FogStart", &fogStart, 0.5f, 0.0f, 1000.0f))
        sceneManager.SetFogStart(fogStart);

    ImGui::Spacing();
    ImGui::TextColored(COL_TEXT_DIM, " Fog End");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::DragFloat("##FogEnd", &fogEnd, 0.5f, 0.0f, 1000.0f))
        sceneManager.SetFogEnd(fogEnd);

    ImGui::EndDisabled();
}

//=============================================================================
// RenderSpriteProperties
//=============================================================================
void EditorUI::RenderSpriteProperties(WorldSpriteClass* sprite)
{
    if (!sprite) return;

    SectionHeader("Sprite");
    ImGui::Spacing();

    XMFLOAT3 sprPos = sprite->GetPosition();
    float pos[3] = { sprPos.x, sprPos.y, sprPos.z };
    ImGui::TextColored(COL_TEXT_DIM, " Position");
    if (DragFloat3Colored("##SprPos", pos, 0.1f))
        sprite->SetPosition(pos[0], pos[1], pos[2]);

    ImGui::Spacing();

    XMFLOAT2 sprScale = sprite->GetScale();
    float w = sprScale.x, h = sprScale.y;
    ImGui::TextColored(COL_TEXT_DIM, " Size");
    ImGui::SetNextItemWidth((ImGui::GetContentRegionAvail().x - 4.0f) * 0.5f);
    ImGui::DragFloat("##SprW", &w, 0.01f, 0.001f, 100.0f);
    ImGui::SameLine(0, 4);
    ImGui::SetNextItemWidth(-1);
    ImGui::DragFloat("##SprH", &h, 0.01f, 0.001f, 100.0f);
    sprite->SetScale(w, h);
}

//=============================================================================
// RenderLightProperties
//=============================================================================
void EditorUI::RenderLightPropertiesByEntity(SceneManager& sceneManager, Entity lightEntity)
{
    auto& registry = sceneManager.GetECSRegistry();

    auto* light = registry.GetComponent<LightComponent>(lightEntity);
    auto* transform = registry.GetComponent<TransformComponent>(lightEntity);

    if (!light || !transform)
    {
        ImGui::Text("Invalid light entity");
        return;
    }

    ImGui::Text("Light Properties");

    // Цвет
    float color[4] = { light->color.x, light->color.y, light->color.z, light->color.w };
    if (ImGui::ColorEdit4("Color", color))
    {
        light->color = { color[0], color[1], color[2], color[3] };
    }

    // Позиция (если не привязан)
    if (light->attachedTo == INVALID_ENTITY)
    {
        float pos[3] = { transform->position.x, transform->position.y, transform->position.z };
        if (ImGui::DragFloat3("Position", pos, 0.1f))
        {
            transform->position = { pos[0], pos[1], pos[2] };
            // Обновляем и визуальную сферу, если нужно
        }
    }

    // Активность
    ImGui::Checkbox("Enabled", &light->enabled);

    // Кнопка удаления
    if (ImGui::Button("Delete Light"))
    {
        // Запрос на удаление (лучше через очередь, чтобы не удалить во время итерации)
         registry.DestroyEntity(lightEntity);
    }
}

//=============================================================================
// RenderContentBrowser
//=============================================================================
void EditorUI::RenderContentBrowser()
{
    ImGui::Spacing();
    RenderFileBrowserToolbar();
    ImGui::Spacing();
    RenderFileBrowserBreadcrumb();
    ImGui::Spacing();

    if (!m_fileSystem) { ImGui::TextColored(COL_TEXT_DIM, "FileSystem not initialized"); return; }

    const std::string query = m_searchQuery;

    // ✅ FIX: Нормализация пути
    std::string relativeDir = m_currentDir.string();
    if (relativeDir == ".") {
        relativeDir = "";
    }

    // ✅ DEBUG: Выводим пути перед поиском
#ifdef _DEBUG
    OutputDebugStringA(("[CB] relativeDir: '" + relativeDir + "'\n").c_str());
    OutputDebugStringA(("[CB] Root: " + m_fileSystem->GetRootPath().string() + "\n").c_str());
#endif

    auto entries = m_fileSystem->GetFilteredEntries(relativeDir, query, m_filterExtensions[m_activeFilterIndex]);

    // ✅ DEBUG: Сколько файлов найдено
#ifdef _DEBUG
    OutputDebugStringA(("[CB] Found " + std::to_string(entries.size()) + " entries\n").c_str());
#endif

    RenderFileBrowserList(entries);
}

void EditorUI::RenderFileBrowserToolbar()
{
    const float btnW = std::floor(28.0f * ScaleX());

    // ✅ FIX: Проверяем, можем ли идти назад
    bool canGoBack = !m_currentDir.empty() && m_currentDir != ".";

    ImGui::BeginDisabled(!canGoBack);
    if (ImGui::Button("<##Back", ImVec2(btnW, 0)) && m_fileSystem)
    {
        fs::path currentPath(m_currentDir);
        fs::path parentPath = currentPath.parent_path();

        // ✅ FIX: Если parent пустой — возвращаемся в корень (пустая строка)
        m_currentDir = parentPath.empty() ? "" : parentPath.generic_string();

        // ✅ DEBUG: Выводим путь после навигации
        OutputDebugStringA(("[NAV] Back to: '" + m_currentDir.string() + "'\n").c_str());

        // ✅ FIX: Refresh() НЕ нужен! Кэш уже загружен
        // m_fileSystem->Refresh(); 
    }
    if (ImGui::IsItemHovered() && !canGoBack) {
        ImGui::SetTooltip("Вы в корневой директории");
    }
    ImGui::EndDisabled();

    ImGui::SameLine(0, 4);
    if (ImGui::Button("R##Refresh", ImVec2(btnW, 0)) && m_fileSystem)
        m_fileSystem->Refresh();

    ImGui::SameLine(0, 8);
    ImGui::SetNextItemWidth(std::floor(90.0f * ScaleX()));
    if (ImGui::BeginCombo("##Filter", m_filterLabels[m_activeFilterIndex].c_str()))
    {
        for (int i = 0; i < (int)m_filterLabels.size(); ++i)
            if (ImGui::Selectable(m_filterLabels[i].c_str(), m_activeFilterIndex == i))
                m_activeFilterIndex = i;
        ImGui::EndCombo();
    }

    ImGui::SameLine(0, 8);
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##Search", m_searchBuf, sizeof(m_searchBuf));
    m_searchQuery = m_searchBuf;

    ImGui::SameLine(0, 8);
    ImGui::PushStyleColor(ImGuiCol_Button, m_viewModeGrid ? COL_ACCENT : ImVec4(0.22f, 0.22f, 0.22f, 1));
    if (ImGui::Button("##Grid", ImVec2(btnW, 0))) m_viewModeGrid = true;
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 2);
    ImGui::PushStyleColor(ImGuiCol_Button, !m_viewModeGrid ? COL_ACCENT : ImVec4(0.22f, 0.22f, 0.22f, 1));
    if (ImGui::Button("##List", ImVec2(btnW, 0))) m_viewModeGrid = false;
    ImGui::PopStyleColor();
}

void EditorUI::RenderFileBrowserBreadcrumb()
{
    ImGui::TextColored(COL_TEXT_DIM, "/");
    ImGui::TextColored(COL_TEXT_DIM, "/");
    ImGui::SameLine(0, 2);
    std::string dirStr = m_currentDir.string();
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1), "%s", dirStr.empty() ? "root" : dirStr.c_str());
}

void EditorUI::RenderFileBrowserList(const std::vector<const FileEntry*>& entries)
{
    const float avail = ImGui::GetContentRegionAvail().y - std::floor(4.0f * ScaleY());
    ImGui::BeginChild("##CBList", ImVec2(-1, avail), false);

    if (m_viewModeGrid)
    {
        const float cellW = std::floor(70.0f * ScaleX());
        int cols = (std::max)(1, (int)(ImGui::GetContentRegionAvail().x / cellW));
        if (ImGui::BeginTable("##CBGrid", cols))
        {
            for (size_t i = 0; i < entries.size(); ++i)
            {
                ImGui::TableNextColumn();
                RenderFileBrowserItemGrid(entries[i], i);
            }
            ImGui::EndTable();
        }
    }
    else
    {
        for (size_t i = 0; i < entries.size(); ++i)
            RenderFileBrowserItemList(entries[i], i);
    }

    ImGui::EndChild();
}

void EditorUI::RenderFileBrowserItemGrid(const FileEntry* entry, size_t index)
{
    if (!entry) return;
    ImGui::PushID((int)index);
    const bool sel = (m_selectedEntry == entry);
    if (sel) ImGui::PushStyleColor(ImGuiCol_Button, COL_ACCENT);
    if (ImGui::Button(entry->isDirectory ? "[D]" : "[F]", ImVec2(std::floor(56.0f * ScaleX()), std::floor(48.0f * ScaleY()))))
        OnEntrySelected(entry);
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
        OnEntryDoubleClicked(entry);
    if (sel) ImGui::PopStyleColor();
    ImGui::TextColored(COL_TEXT_DIM, "%s", entry->name.c_str());
    ImGui::PopID();
}

void EditorUI::RenderFileBrowserItemList(const FileEntry* entry, size_t index)
{
    if (!entry) return;
    ImGui::PushID((int)index);
    const bool sel = (m_selectedEntry == entry);
    if (sel) ImGui::PushStyleColor(ImGuiCol_Header, COL_SELECTED);
    char label[256];
    snprintf(label, sizeof(label), "%s  %s", entry->isDirectory ? "[D]" : "[F]", entry->name.c_str());
    if (ImGui::Selectable(label, sel, 0, ImVec2(0, std::floor(18.0f * ScaleY()))))
        OnEntrySelected(entry);
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
        OnEntryDoubleClicked(entry);
    if (sel) ImGui::PopStyleColor();
    ImGui::PopID();
}

void EditorUI::OnEntryDoubleClicked(const FileEntry* entry)
{
    if (!entry) return;
    if (entry->isDirectory)
    {
        m_currentDir = fs::path(m_currentDir) / entry->name;
        if (m_fileSystem) m_fileSystem->Refresh();
    }
    else
    {
        OnFileSelected(entry);
    }
}

void EditorUI::OnEntrySelected(const FileEntry* entry)
{
    m_selectedEntry = entry;
}

void EditorUI::OnFileSelected(const FileEntry* entry)
{
    if (!entry) return;
    const std::string ext = fs::path(entry->name).extension().string();

    // Модели — заполняем поле Model path
    if (ext == ".obj" || ext == ".fbx" || ext == ".glb" || ext == ".gltf")
        strncpy_s(m_spawnModelPath, sizeof(m_spawnModelPath), entry->path.c_str(), _TRUNCATE);
    // Текстуры — заполняем поле Texture path
    else if (ext == ".tga" || ext == ".png" || ext == ".jpg" || ext == ".dds")
        strncpy_s(m_spawnTexturePath, sizeof(m_spawnTexturePath), entry->path.c_str(), _TRUNCATE);
}
//=============================================================================
// RenderKnifeTool — инструмент разрезания (clip plane)
//=============================================================================
void EditorUI::RenderKnifeTool(SceneManager& sceneManager, ClipPlaneShaderClass* clipShader)
{
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.10f, 0.10f, 1.0f));
    ImGui::BeginChild("##KnifeToolBar", ImVec2(-1, std::floor(32.0f * ScaleY())), false);

    ImGui::PushStyleColor(ImGuiCol_Button, m_knifeEnabled ? COL_ACCENT : ImVec4(0.22f, 0.22f, 0.22f, 1));
    if (ImGui::Button("  Knife  ", ImVec2(0, 0)))
        m_knifeEnabled = !m_knifeEnabled;
    ImGui::PopStyleColor();

    if (m_knifeEnabled && clipShader)
    {
        ImGui::SameLine(0, 8);
        ImGui::SetNextItemWidth(std::floor(60.0f * ScaleX()));
        const char* axes[] = { "X", "Y", "Z" };
        ImGui::Combo("##KAxis", &m_knifeAxis, axes, 3);

        ImGui::SameLine(0, 8);
        ImGui::SetNextItemWidth(std::floor(80.0f * ScaleX()));
        ImGui::DragFloat("##KPos", &m_knifePosition, 0.1f);

        ImGui::SameLine(0, 8);
        ImGui::Checkbox("Flip", &m_knifeFlip);

        XMFLOAT4 plane = { 0, 0, 0, -m_knifePosition };
        if (m_knifeAxis == 0) plane.x = m_knifeFlip ? -1.0f : 1.0f;
        else if (m_knifeAxis == 1) plane.y = m_knifeFlip ? -1.0f : 1.0f;
        else                       plane.z = m_knifeFlip ? -1.0f : 1.0f;
        clipShader->SetClipPlane(plane);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

//=============================================================================
// RenderShaderSelector — выбор шейдера для выделенного объекта
//=============================================================================
void EditorUI::RenderShaderSelector(SceneManager& sceneManager)
{
    if (ImGui::BeginTabItem("Shader"))
    {
        ImGui::Spacing();
        SectionHeader("Shader Selector");
        ImGui::Spacing();

        if (m_shaderNames.empty())
            RefreshShaderList();

        if (m_shaderNames.empty())
        {
            ImGui::TextColored(COL_TEXT_DIM, "No shaders registered");
            ImGui::EndTabItem();
            return;
        }

        std::vector<const char*> names;
        names.reserve(m_shaderNames.size());
        for (auto& n : m_shaderNames) names.push_back(n.c_str());

        ImGui::SetNextItemWidth(-1);
        ImGui::ListBox("##ShaderList", &m_selectedShaderIndex, names.data(), (int)names.size(), 6);

        ImGui::Spacing();

        const bool hasObject = (m_selectedType == SelectedType::Object &&
            m_selectedEntity != INVALID_ENTITY);

        ImGui::BeginDisabled(!hasObject);
        ImGui::PushStyleColor(ImGuiCol_Button, COL_ACCENT);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COL_ACCENT_HOV);
        if (ImGui::Button("Apply Shader", ImVec2(-1, 0)) && hasObject)
        {
            auto* shader = ShaderManager::Get().Get(m_shaderNames[m_selectedShaderIndex]);
            if (shader) sceneManager.SetObjectShader(m_selectedEntity, shader, m_device);
        }
        ImGui::PopStyleColor(2);
        ImGui::EndDisabled();

        if (!hasObject)
            ImGui::TextColored(COL_TEXT_DIM, "Select an object first");

        ImGui::Spacing();
        if (ImGui::Button("Refresh List", ImVec2(-1, 0)))
            RefreshShaderList();

        // ── Назначение текстур ────────────────────────────────────────────────
               // ── Назначение текстур ────────────────────────────────────────────────
        if (hasObject)
        {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            SectionHeader("Textures");
            ImGui::Spacing();

            // Определяем выбранный шейдер — PBR требует особые подписи слотов
            const bool isPBR = (m_selectedShaderIndex >= 0 &&
                m_selectedShaderIndex < (int)m_shaderNames.size() &&
                m_shaderNames[m_selectedShaderIndex] == "pbr");

            // ── Слот 0 ────────────────────────────────────────────────────────
            ImGui::TextColored(COL_TEXT_DIM, isPBR ? " Albedo (slot 0)" : " Texture (slot 0)");
            ImGui::SetNextItemWidth(-std::floor(60.0f * ScaleX()) - 4.0f);
            ImGui::InputText("##Tex0Path", m_tex0PathBuf, sizeof(m_tex0PathBuf));
            ImGui::SameLine(0, 4);
            ImGui::PushStyleColor(ImGuiCol_Button, COL_ACCENT);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COL_ACCENT_HOV);
            if (ImGui::Button("Apply##T0", ImVec2(-1, 0)))
            {
                if (m_tex0PathBuf[0] != '\0')
                    OutputDebugStringA(("UI: SetObjectTexture() called for slot 0, path=" +
                        std::string(m_tex0PathBuf) + "\n").c_str());

                sceneManager.SetObjectTexture(m_selectedEntity, 0,
                    m_tex0PathBuf, m_device, m_context);
            }
            ImGui::PopStyleColor(2);

            ImGui::Spacing();

            // ── Слот 1 ────────────────────────────────────────────────────────
            ImGui::TextColored(COL_TEXT_DIM, isPBR ? " Normal Map (slot 1)" : " Normal Map (slot 1)");
            ImGui::SetNextItemWidth(-std::floor(60.0f * ScaleX()) - 4.0f);
            ImGui::InputText("##Tex1Path", m_tex1PathBuf, sizeof(m_tex1PathBuf));
            ImGui::SameLine(0, 4);
            ImGui::PushStyleColor(ImGuiCol_Button, COL_ACCENT);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COL_ACCENT_HOV);
            if (ImGui::Button("Apply##T1", ImVec2(-1, 0)))
            {
                if (m_tex1PathBuf[0] != '\0')
                    sceneManager.SetObjectTexture(m_selectedEntity, 1,
                        m_tex1PathBuf, m_device, m_context);
            }
            ImGui::PopStyleColor(2);

            // ── Слоты 2 и 3 — только для PBR ─────────────────────────────────
            if (isPBR)
            {
                ImGui::Spacing();

                ImGui::TextColored(COL_TEXT_DIM, " Metalness (slot 2)");
                ImGui::SetNextItemWidth(-std::floor(60.0f * ScaleX()) - 4.0f);
                ImGui::InputText("##Tex2Path", m_tex2PathBuf, sizeof(m_tex2PathBuf));
                ImGui::SameLine(0, 4);
                ImGui::PushStyleColor(ImGuiCol_Button, COL_ACCENT);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COL_ACCENT_HOV);
                if (ImGui::Button("Apply##T2", ImVec2(-1, 0)))
                {
                    if (m_tex2PathBuf[0] != '\0')
                        sceneManager.SetObjectTexture(m_selectedEntity, 2,
                            m_tex2PathBuf, m_device, m_context);
                }
                ImGui::PopStyleColor(2);

                ImGui::Spacing();

                ImGui::TextColored(COL_TEXT_DIM, " Roughness (slot 3)");
                ImGui::SetNextItemWidth(-std::floor(60.0f * ScaleX()) - 4.0f);
                ImGui::InputText("##Tex3Path", m_tex3PathBuf, sizeof(m_tex3PathBuf));
                ImGui::SameLine(0, 4);
                ImGui::PushStyleColor(ImGuiCol_Button, COL_ACCENT);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COL_ACCENT_HOV);
                if (ImGui::Button("Apply##T3", ImVec2(-1, 0)))
                {
                    if (m_tex3PathBuf[0] != '\0')
                        sceneManager.SetObjectTexture(m_selectedEntity, 3,
                            m_tex3PathBuf, m_device, m_context);
                }
                ImGui::PopStyleColor(2);

                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f),
                    " Tip: если нет metalness/roughness\n"
                    " текстур — используй белый/серый PNG");
            }

            ImGui::Spacing();

            // Пресеты текстур
            ImGui::TextColored(COL_TEXT_DIM, " Texture presets");
            for (int i = 0; i < PRESET_COUNT; ++i)
            {
                if (i > 0) ImGui::SameLine(0, 4);
                if (ImGui::SmallButton(TEXTURE_PRESETS[i]))
                    strncpy_s(m_tex0PathBuf, sizeof(m_tex0PathBuf),
                        TEXTURE_PRESETS[i], _TRUNCATE);
            }
        }

        ImGui::EndTabItem();
    }
}

void EditorUI::RenderPhysicsSettings(SceneManager& sceneManager)
{
    if (ImGui::BeginTabItem("Physics"))
    {
        if (sceneManager.GetECSRegistry().HasComponent<RigidBodyComponent>(m_selectedEntity))
        {
            
        }
    }

    ImGui::EndTabItem();

}


void EditorUI::RenderSubmarineBuilder(SceneManager& sceneManager)
{
    ImGui::Spacing();
    SectionHeader("🚢 Submarine Editor");
    ImGui::Spacing();

    auto& registry = sceneManager.GetECSRegistry();

    // ══════════════════════════════════════════════════════════════════════════
    // 🔹 СОЗДАНИЕ ПУСТОЙ ПОДЛОДКИ С МЕШЕМ
    // ══════════════════════════════════════════════════════════════════════════
    ImGui::TextColored(COL_ACCENT, "1. Create Submarine with Model:");
    ImGui::TextColored(COL_TEXT_DIM, "Creates hull + compartments with visual mesh");
    ImGui::Spacing();

    static float spawnY = 0.0f;
    static float hullRadius = 3.0f;
    static float hullMass = 3000.0f;

    ImGui::DragFloat("Spawn Height (Y)##Sub", &spawnY, 0.5f, -50.0f, 50.0f);
    ImGui::DragFloat("Hull Radius", &hullRadius, 0.2f, 1.0f, 10.0f);
    ImGui::DragFloat("Hull Mass", &hullMass, 100.0f, 1000.0f, 10000.0f);

    ImGui::Spacing();

    // Выбор модели для подлодки
    ImGui::TextColored(COL_TEXT_DIM, " Model preset:");
    const char* modelNames[] = { "Sphere", "Cylinder", "Cube" };
    static int selectedHullModel = 0;

    const float btnW = (ImGui::GetContentRegionAvail().x - 8.0f) / 3.0f;
    for (int i = 0; i < 3; i++)
    {
        if (i > 0) ImGui::SameLine(0, 4);
        if (ImGui::Button(modelNames[i], ImVec2(btnW, 0)))
            selectedHullModel = i;
    }

    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Button, COL_ACCENT);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COL_ACCENT_HOV);
    if (ImGui::Button("🚢 Create Submarine with Model", ImVec2(-1, 0)))
    {
        const char* modelPath = nullptr;
        switch (selectedHullModel)
        {
        case 0: modelPath = "assets/model/sphere.obj"; break;
        case 1: modelPath = "assets/model/cylinder.obj"; break;
        case 2: modelPath = "assets/model/cube.obj"; break;
        }

        Entity newSub = CreateSubmarineWithMesh(
            sceneManager,
            XMFLOAT3(0, spawnY, 0),
            modelPath,
            hullRadius,
            hullMass
        );

        if (newSub != INVALID_ENTITY)
        {
            m_selectedType = SelectedType::Object;
            m_selectedEntity = newSub;
        }
    }
    ImGui::PopStyleColor(2);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ══════════════════════════════════════════════════════════════════════════
    // 🔹 ДОБАВЛЕНИЕ ОТСЕКОВ К ВЫБРАННОЙ ПОДЛОДКЕ
    // ══════════════════════════════════════════════════════════════════════════
    ImGui::TextColored(COL_ACCENT, "2. Add Compartments:");
    ImGui::Spacing();

    if (m_selectedType == SelectedType::Object && m_selectedEntity != INVALID_ENTITY)
    {
        auto* subComp = registry.GetComponent<SubmarineComponent>(m_selectedEntity);
        auto* rbComp = registry.GetComponent<RigidBodyComponent>(m_selectedEntity);

        bool isSubmarineRoot = (subComp && subComp->submarineRoot == m_selectedEntity && rbComp);

        if (isSubmarineRoot)
        {
            ImGui::TextColored(COL_GREEN, "✅ Selected: Submarine #%u", m_selectedEntity);
            ImGui::Text("Compartments: %zu | Mass: %.0f kg", subComp->allCompartments.size(), rbComp->mass);
            ImGui::Spacing();

            ImGui::TextColored(COL_TEXT_DIM, "New Compartment Settings:");

            static float compOffset[3] = { 0, 0, 0 };
            static float compSize[3] = { 3, 2.5f, 3.5f };
            static float compMass = 500.0f;
            static int selectedCompModel = 0;

            ImGui::DragFloat3("##CompOffset", compOffset, 0.5f);
            ImGui::SameLine();
            ImGui::TextColored(COL_TEXT_DIM, "Offset");

            ImGui::DragFloat3("##CompSize", compSize, 0.1f, 1.0f, 10.0f);
            ImGui::SameLine();
            ImGui::TextColored(COL_TEXT_DIM, "Size");

            ImGui::DragFloat("##CompMass", &compMass, 10.0f, 100.0f, 2000.0f);
            ImGui::SameLine();
            ImGui::TextColored(COL_TEXT_DIM, "Mass");

            ImGui::Spacing();

            // Модель для отсека
            ImGui::TextColored(COL_TEXT_DIM, " Compartment model:");
            const char* compModelNames[] = { "Cube", "Cylinder" };

            for (int i = 0; i < 2; i++)
            {
                if (i > 0) ImGui::SameLine(0, 4);
                float w = (ImGui::GetContentRegionAvail().x - 4.0f) / 2.0f;
                if (ImGui::Button(compModelNames[i], ImVec2(w, 0)))
                    selectedCompModel = i;
            }

            ImGui::Spacing();

            // Позиция пресеты
            ImGui::TextColored(COL_TEXT_DIM, "Position Presets:");

            if (ImGui::Button("Front", ImVec2(btnW, 0))) {
                compOffset[2] = 5.0f;
            }
            ImGui::SameLine(0, 4);
            if (ImGui::Button("Center", ImVec2(btnW, 0))) {
                compOffset[0] = compOffset[1] = compOffset[2] = 0;
            }
            ImGui::SameLine(0, 4);
            if (ImGui::Button("Rear", ImVec2(btnW, 0))) {
                compOffset[2] = -5.0f;
            }

            ImGui::Spacing();

            // Размер пресеты
            ImGui::TextColored(COL_TEXT_DIM, "Size Presets:");
            if (ImGui::Button("Small", ImVec2(btnW, 0))) {
                compSize[0] = 2; compSize[1] = 1.5f; compSize[2] = 2;
            }
            ImGui::SameLine(0, 4);
            if (ImGui::Button("Medium", ImVec2(btnW, 0))) {
                compSize[0] = 3; compSize[1] = 2.5f; compSize[2] = 3.5f;
            }
            ImGui::SameLine(0, 4);
            if (ImGui::Button("Large", ImVec2(btnW, 0))) {
                compSize[0] = 4; compSize[1] = 3; compSize[2] = 5;
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Button, COL_GREEN);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COL_GREEN_HOV);
            if (ImGui::Button("➕ Add Compartment with Model", ImVec2(-1, 0)))
            {
                const char* compModelPath = (selectedCompModel == 0)
                    ? "assets/model/cube.obj"
                    : "assets/model/cylinder.obj";

                Entity newComp = AddCompartmentWithMesh(
                    sceneManager,
                    m_selectedEntity,
                    XMFLOAT3(compOffset[0], compOffset[1], compOffset[2]),
                    XMFLOAT3(compSize[0], compSize[1], compSize[2]),
                    compMass,
                    compModelPath
                );

                if (newComp != INVALID_ENTITY)
                {
                    m_selectedType = SelectedType::Object;
                    m_selectedEntity = newComp;
                }
            }
            ImGui::PopStyleColor(2);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Список отсеков
            ImGui::TextColored(COL_TEXT_DIM, "Existing Compartments:");

            if (subComp->allCompartments.empty())
            {
                ImGui::TextColored(COL_TEXT_DIM, "  (none yet)");
            }
            else
            {
                const float avail = ImGui::GetContentRegionAvail().y - 40.0f;
                ImGui::BeginChild("##CompList", ImVec2(-1, avail), true);

                for (size_t i = 0; i < subComp->allCompartments.size(); ++i)
                {
                    Entity compEnt = subComp->allCompartments[i];
                    auto* compData = registry.GetComponent<CompartmentComponent>(compEnt);

                    char label[128];
                    if (compData)
                    {
                        float floodPercent = (compData->volume > 0) ?
                            (compData->waterVolume / compData->volume) * 100.f : 0.f;
                        snprintf(label, sizeof(label),
                            "[%zu] Entity #%u (%.0f%% flooded)",
                            i, compEnt, floodPercent);
                    }
                    else
                    {
                        snprintf(label, sizeof(label), "[%zu] Entity #%u", i, compEnt);
                    }

                    bool isSelected = (m_selectedEntity == compEnt);
                    if (isSelected) ImGui::PushStyleColor(ImGuiCol_Header, COL_SELECTED);

                    if (ImGui::Selectable(label, isSelected, 0, ImVec2(0, 20.0f)))
                    {
                        m_selectedType = SelectedType::Object;
                        m_selectedEntity = compEnt;
                    }

                    if (isSelected) ImGui::PopStyleColor();
                }

                ImGui::EndChild();
            }
        }
        else
        {
            ImGui::TextColored(COL_RED, "⚠️ Not a Submarine Root");
        }
    }
    else
    {
        ImGui::TextColored(COL_TEXT_DIM, "No submarine selected");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ══════════════════════════════════════════════════════════════════════════
    // 🔹 СПИСОК ВСЕХ ПОДЛОДОК
    // ══════════════════════════════════════════════════════════════════════════
    ImGui::TextColored(COL_TEXT_DIM, "All Submarines:");

    auto& subPool = registry.GetComponents<SubmarineComponent>();
    int subCount = 0;

    const float avail = ImGui::GetContentRegionAvail().y - 20.0f;
    ImGui::BeginChild("##SubList", ImVec2(-1, avail), true);

    for (auto& [entity, subComp] : subPool)
    {
        if (subComp.submarineRoot != entity) continue;

        auto* rb = registry.GetComponent<RigidBodyComponent>(entity);
        if (!rb) continue;

        subCount++;

        char label[128];
        snprintf(label, sizeof(label), "🚢 Submarine #%u (%zu comp, %.0f kg)",
            entity, subComp.allCompartments.size(), rb->mass);

        bool isSelected = (m_selectedEntity == entity);
        if (isSelected) ImGui::PushStyleColor(ImGuiCol_Header, COL_SELECTED);

        if (ImGui::Selectable(label, isSelected, 0, ImVec2(0, 20.0f)))
        {
            m_selectedType = SelectedType::Object;
            m_selectedEntity = entity;
        }

        if (isSelected) ImGui::PopStyleColor();
    }

    if (subCount == 0)
        ImGui::TextColored(COL_TEXT_DIM, "  (No submarines)");

    ImGui::EndChild();
}



void EditorUI::RenderJointConnectionUI(ECSRegistry& registry, Entity selectedEntity)
{
    auto* jointComp = registry.GetComponent<JointConnectionComponent>(selectedEntity);

    if (ImGui::CollapsingHeader("Joint Connection", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (!jointComp)
        {
            // Кнопка добавить компонент
            if (ImGui::Button("Add Joint Connection"))
            {
                JointConnectionComponent newComp{};
                registry.AddComponent<JointConnectionComponent>(selectedEntity, newComp);
                jointComp = registry.GetComponent<JointConnectionComponent>(selectedEntity);
            }
        }
        else
        {
            // ── Выбор целевого объекта ────────────────────────────────────────
            ImGui::Text("Target Entity:");
            ImGui::SameLine();

            // Показываем текущую цель или "None"
            std::string targetName = "None";
            if (jointComp->targetEntity != INVALID_ENTITY)
            {
                targetName = "Entity " + std::to_string(jointComp->targetEntity);
            }

            if (ImGui::Button(targetName.c_str()))
            {
                // Открываем список всех объектов
                ImGui::OpenPopup("SelectTargetPopup");
            }

            // Popup со списком объектов
            if (ImGui::BeginPopup("SelectTargetPopup"))
            {
                auto& allTransforms = registry.GetComponents<TransformComponent>();
                auto& allRigidBodies = registry.GetComponents<RigidBodyComponent>();

                for (auto& [entity, transform] : allTransforms)
                {
                    if (entity == selectedEntity) continue; // Пропускаем себя

                    // Проверяем есть ли RigidBody
                    if (!allRigidBodies.Has(entity)) continue;

                    std::string entityName = "Entity " + std::to_string(entity);
                    if (ImGui::Selectable(entityName.c_str()))
                    {
                        // Если джойнт уже создан — удаляем его
                        if (jointComp->joint)
                        {
                            JointConnectionSystem::RemoveJoint(registry, *jointComp);
                        }

                        jointComp->targetEntity = entity;
                        jointComp->isBroken = false;
                    }
                }
                ImGui::EndPopup();
            }

            // Кнопка очистить выбор
            ImGui::SameLine();
            if (jointComp->targetEntity != INVALID_ENTITY && ImGui::SmallButton("Clear"))
            {
                if (jointComp->joint)
                {
                    
                    JointConnectionSystem::RemoveJoint(registry, *jointComp);
                }
                jointComp->targetEntity = INVALID_ENTITY;
                jointComp->joint = nullptr;
                jointComp->isBroken = false;
            }

            ImGui::Separator();

            // ── Pivot A (локальные координаты этого объекта) ─────────────────
            ImGui::Text("Pivot A (This):");
            ImGui::DragFloat3("##PivotA", &jointComp->pivotA[0], 0.1f);

            // ── Pivot B (локальные координаты цели) ──────────────────────────
            ImGui::Text("Pivot B (Target):");
            ImGui::DragFloat3("##PivotB", &jointComp->pivotB[0], 0.1f);

            ImGui::Separator();

            // ── Параметры ────────────────────────────────────────────────────
            ImGui::DragFloat("Break Force", &jointComp->breakForce, 1000.0f, 0.0f, 1000000.0f);
            ImGui::Checkbox("Disable Collisions", &jointComp->disableCollisions);
            ImGui::Checkbox("Show Gizmo", &jointComp->showGizmo);

            ImGui::ColorEdit4("Gizmo Color", &jointComp->gizmoColor.x);

            ImGui::Separator();

            // ── Статус ───────────────────────────────────────────────────────
            ImGui::Text("Status:");
            ImGui::SameLine();

            if (jointComp->isBroken)
            {
                ImGui::TextColored(ImVec4(1, 0, 0, 1), "BROKEN");
            }
            else if (jointComp->joint)
            {
                ImGui::TextColored(ImVec4(0, 1, 0, 1), "Connected");

                // Показываем текущий импульс
                btScalar impulse = 0.f;
                if (jointComp->joint->getJointFeedback()) // или проверка внутреннего флага
                {
                    impulse = jointComp->joint->getAppliedImpulse();
                }
                else
                {
                    // Опционально: включить "на лету", но данные появятся только на следующем кадре
                    jointComp->joint->enableFeedback(true);
                }
                char buf[128];
                sprintf_s(buf, "Impulse: %.2f / %.2f", impulse, jointComp->breakForce);
                ImGui::Text(buf);
            }
            else if (jointComp->targetEntity != INVALID_ENTITY)
            {
                ImGui::TextColored(ImVec4(1, 1, 0, 1), "Pending...");
            }
            else
            {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1), "No target");
            }

            // ── Кнопки управления ────────────────────────────────────────────
            ImGui::Separator();

            if (jointComp->joint && ImGui::Button("Remove Joint"))
            {
                JointConnectionSystem::RemoveJoint(registry, *jointComp);
                jointComp->joint = nullptr;
            }

            ImGui::SameLine();

            if (ImGui::Button("Auto-Calculate Pivots"))
            {
                // Автоматический расчёт pivot'ов на основе позиций
                AutoCalculatePivots(registry, selectedEntity, *jointComp);
            }
        }
    }
}

// Вспомогательный метод для авто-расчёта
void EditorUI::AutoCalculatePivots(ECSRegistry& registry, Entity entity, JointConnectionComponent& jointComp)
{
    auto* transformA = registry.GetComponent<TransformComponent>(entity);
    auto* transformB = registry.GetComponent<TransformComponent>(jointComp.targetEntity);
    auto* rbA = registry.GetComponent<RigidBodyComponent>(entity);
    auto* rbB = registry.GetComponent<RigidBodyComponent>(jointComp.targetEntity);

    if (!transformA || !transformB || !rbA || !rbB) return;

    // Вектор от A к B
    btVector3 posA(transformA->position.x, transformA->position.y, transformA->position.z);
    btVector3 posB(transformB->position.x, transformB->position.y, transformB->position.z);
    btVector3 dir = posB - posA;
    dir.normalize();

    // Определяем главную ось
    float absX = fabs(dir.x());
    float absY = fabs(dir.y());
    float absZ = fabs(dir.z());

    if (absX > absY && absX > absZ)
    {
        // По оси X
        jointComp.pivotA.setValue(dir.x() > 0 ? rbA->shapeSize.x() : -rbA->shapeSize.x(), 0, 0);
        jointComp.pivotB.setValue(dir.x() > 0 ? -rbB->shapeSize.x() : rbB->shapeSize.x(), 0, 0);
    }
    else if (absZ > absX && absZ > absY)
    {
        // По оси Z
        jointComp.pivotA.setValue(0, 0, dir.z() > 0 ? rbA->shapeSize.z() : -rbA->shapeSize.z());
        jointComp.pivotB.setValue(0, 0, dir.z() > 0 ? -rbB->shapeSize.z() : rbB->shapeSize.z());
    }
    else
    {
        // По оси Y
        jointComp.pivotA.setValue(0, dir.y() > 0 ? rbA->shapeSize.y() : -rbA->shapeSize.y(), 0);
        jointComp.pivotB.setValue(0, dir.y() > 0 ? -rbB->shapeSize.y() : rbB->shapeSize.y(), 0);
    }

    OutputDebugStringA("[AutoCalculatePivots] Done!\n");
}






void EditorUI::RenderCompartmentEditor(SceneManager& sceneManager)
{
    if (m_selectedType != SelectedType::Object) return;

    auto& registry = sceneManager.GetECSRegistry();
    auto* comp = registry.GetComponent<CompartmentComponent>(m_selectedEntity);
    if (!comp) return;

    ImGui::Begin("Compartment Editor", nullptr);

    // ══════════════════════════════════════════════════
    // 🔧 ГРАНИЦЫ ОТСЕКА (AABB)
    // ══════════════════════════════════════════════════
    ImGui::Text("Local Bounds:");

    float min[3] = { comp->localMin.x, comp->localMin.y, comp->localMin.z };
    float max[3] = { comp->localMax.x, comp->localMax.y, comp->localMax.z };

    if (ImGui::DragFloat3("Min", min, 0.1f))
    {
        comp->localMin = { min[0], min[1], min[2] };
        comp->RecalculateVolume();
    }

    if (ImGui::DragFloat3("Max", max, 0.1f))
    {
        comp->localMax = { max[0], max[1], max[2] };
        comp->RecalculateVolume();
    }

    // ══════════════════════════════════════════════════
    // 🎨 ВИЗУАЛИЗАЦИЯ
    // ══════════════════════════════════════════════════
    ImGui::Separator();
    ImGui::Text("Visual Settings:");

    static bool showWireframe = true;
    ImGui::Checkbox("Show Wireframe", &showWireframe);

    static float wireframeAlpha = 0.4f;
    ImGui::SliderFloat("Opacity", &wireframeAlpha, 0.0f, 1.0f);

    // ══════════════════════════════════════════════════
    // 📊 ИНФОРМАЦИЯ
    // ══════════════════════════════════════════════════
    ImGui::Separator();
    ImGui::Text("Info:");
    ImGui::Text("Volume: %.2f m³", comp->volume);
    ImGui::Text("Water: %.1f%%", (comp->waterVolume / comp->volume) * 100.0f);

    // ══════════════════════════════════════════════════
    // 🔧 БЫСТРЫЕ ДЕЙСТВИЯ
    // ══════════════════════════════════════════════════
    ImGui::Separator();

    if (ImGui::Button("Center on Submarine"))
    {
        auto* subLink = registry.GetComponent<SubmarineComponent>(m_selectedEntity);
        if (subLink && subLink->submarineRoot != INVALID_ENTITY)
        {
            auto* rootTransform = registry.GetComponent<TransformComponent>(subLink->submarineRoot);
            auto* myTransform = registry.GetComponent<TransformComponent>(m_selectedEntity);

            if (rootTransform && myTransform)
            {
                // Вычисляем центр отсека относительно подлодки
                XMFLOAT3 center = {
                    (comp->localMin.x + comp->localMax.x) * 0.5f,
                    (comp->localMin.y + comp->localMax.y) * 0.5f,
                    (comp->localMin.z + comp->localMax.z) * 0.5f
                };
                comp->localCenter = center;
            }
        }
    }

    if (ImGui::Button("Reset to Default"))
    {
        comp->localMin = { -2, -1.5f, -2 };
        comp->localMax = { 2,  1.5f,  2 };
        comp->RecalculateVolume();
    }

    ImGui::End();
}


void EditorUI::RenderPhysicsDebugPanel()
{
    ImGui::Spacing();
    SectionHeader("Physics Visualization");
    ImGui::Spacing();

    // ══════════════════════════════════════════════════════════════════
    // 🎨 ОСНОВНЫЕ ПЕРЕКЛЮЧАТЕЛИ
    // ══════════════════════════════════════════════════════════════════
    ImGui::TextColored(COL_TEXT_DIM, " Render Modes:");
    ImGui::Checkbox("Show Colliders", &m_debugSettings.showColliders);
    ImGui::Checkbox("Show Contacts", &m_debugSettings.showContacts);
    ImGui::Checkbox("Show AABB", &m_debugSettings.showAABB);
    ImGui::Checkbox("Show Center of Mass", &m_debugSettings.showCenterOfMass);
    ImGui::Checkbox("Show Velocity Vectors", &m_debugSettings.showVelocity);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ══════════════════════════════════════════════════════════════════
    // 🔧 ФИЛЬТРЫ ПО ТИПАМ КОЛЛАЙДЕРОВ
    // ══════════════════════════════════════════════════════════════════
    ImGui::TextColored(COL_TEXT_DIM, " Collider Type Filters:");

    ImGui::BeginDisabled(!m_debugSettings.showColliders);

    ImGui::Columns(2, nullptr, false);
    ImGui::Checkbox("Boxes", &m_debugSettings.showBoxes);
    ImGui::Checkbox("Spheres", &m_debugSettings.showSpheres);
    ImGui::Checkbox("Capsules", &m_debugSettings.showCapsules);
    ImGui::Checkbox("Cylinders", &m_debugSettings.showCylinders);

    ImGui::NextColumn();

    ImGui::Checkbox("Planes", &m_debugSettings.showPlanes);
    ImGui::Checkbox("Convex Hulls", &m_debugSettings.showConvexHulls);
    ImGui::Checkbox("Triangle Meshes", &m_debugSettings.showTriangleMeshes);

    ImGui::Columns(1);
    ImGui::EndDisabled();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ══════════════════════════════════════════════════════════════════
    // 🎨 ЦВЕТОВАЯ СХЕМА
    // ══════════════════════════════════════════════════════════════════
    if (ImGui::CollapsingHeader("🎨 Color Settings", ImGuiTreeNodeFlags_None))
    {
        ImGui::Spacing();
        ImGui::TextColored(COL_TEXT_DIM, " Object States:");

        ImGui::ColorEdit4("Dynamic Bodies##CD", &m_debugSettings.colorDynamic.x);
        ImGui::ColorEdit4("Static Bodies##CS", &m_debugSettings.colorStatic.x);
        ImGui::ColorEdit4("Kinematic Bodies##CK", &m_debugSettings.colorKinematic.x);
        ImGui::ColorEdit4("Sleeping Bodies##CSL", &m_debugSettings.colorSleeping.x);

        ImGui::Spacing();
        ImGui::TextColored(COL_TEXT_DIM, " Debug Elements:");

        ImGui::ColorEdit4("Contacts##CC", &m_debugSettings.colorContact.x);
        ImGui::ColorEdit4("AABB##CAABB", &m_debugSettings.colorAABB.x);

        ImGui::Spacing();

        if (ImGui::Button("Reset to Default##Colors", ImVec2(-1, 0)))
        {
            m_debugSettings.colorDynamic = { 0.2f, 0.8f, 0.2f, 0.6f };
            m_debugSettings.colorStatic = { 0.8f, 0.2f, 0.2f, 0.6f };
            m_debugSettings.colorKinematic = { 0.2f, 0.5f, 0.9f, 0.6f };
            m_debugSettings.colorSleeping = { 0.5f, 0.5f, 0.5f, 0.4f };
            m_debugSettings.colorContact = { 1.0f, 1.0f, 0.0f, 1.0f };
            m_debugSettings.colorAABB = { 0.9f, 0.6f, 0.1f, 0.3f };
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ══════════════════════════════════════════════════════════════════
    // ⚙️ РАЗМЕРЫ И ПАРАМЕТРЫ
    // ══════════════════════════════════════════════════════════════════
    if (ImGui::CollapsingHeader("⚙️ Rendering Parameters"))
    {
        ImGui::Spacing();
        ImGui::SliderFloat("Line Thickness", &m_debugSettings.lineThickness, 0.5f, 5.0f);
        ImGui::SliderFloat("Contact Marker Size", &m_debugSettings.contactMarkerSize, 0.05f, 0.5f);
        ImGui::SliderFloat("Velocity Scale", &m_debugSettings.velocityScale, 0.1f, 2.0f);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ══════════════════════════════════════════════════════════════════
    // 🔄 БЫСТРЫЕ ДЕЙСТВИЯ
    // ══════════════════════════════════════════════════════════════════
    ImGui::TextColored(COL_TEXT_DIM, " Quick Actions:");

    if (ImGui::Button("Enable All##PD", ImVec2(-1, 0)))
    {
        m_debugSettings.showBoxes = m_debugSettings.showSpheres = m_debugSettings.showCapsules = true;
        m_debugSettings.showCylinders = m_debugSettings.showPlanes = true;
        m_debugSettings.showConvexHulls = m_debugSettings.showTriangleMeshes = true;
    }

    if (ImGui::Button("Disable All##PD", ImVec2(-1, 0)))
    {
        m_debugSettings.showBoxes = m_debugSettings.showSpheres = m_debugSettings.showCapsules = false;
        m_debugSettings.showCylinders = m_debugSettings.showPlanes = false;
        m_debugSettings.showConvexHulls = m_debugSettings.showTriangleMeshes = false;
    }

    if (ImGui::Button("Primitives Only##PD", ImVec2(-1, 0)))
    {
        m_debugSettings.showBoxes = m_debugSettings.showSpheres = m_debugSettings.showCapsules = true;
        m_debugSettings.showCylinders = true;
        m_debugSettings.showConvexHulls = m_debugSettings.showTriangleMeshes = false;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ══════════════════════════════════════════════════════════════════
    // 📊 СТАТИСТИКА (ОТЛАДКА)
    // ══════════════════════════════════════════════════════════════════
    if (ImGui::CollapsingHeader("📊 Debug Info"))
    {
        ImGui::Spacing();
        ImGui::TextColored(COL_TEXT_DIM, " Settings Status:");

        ImGui::Text("showColliders: %s", m_debugSettings.showColliders ? "ON" : "OFF");
        ImGui::Text("showBoxes: %s", m_debugSettings.showBoxes ? "ON" : "OFF");
        ImGui::Text("showSpheres: %s", m_debugSettings.showSpheres ? "ON" : "OFF");

        ImGui::Spacing();
        ImGui::TextColored(COL_TEXT_DIM, " Color Values:");
        ImGui::Text("Dynamic: (%.2f, %.2f, %.2f, %.2f)",
            m_debugSettings.colorDynamic.x,
            m_debugSettings.colorDynamic.y,
            m_debugSettings.colorDynamic.z,
            m_debugSettings.colorDynamic.w);
    }
}



// ════════════════════════════════════════════════════════════════════════════
// Helper: Create a new compartment entity linked to a submarine root
// ════════════════════════════════════════════════════════════════════════════
// ════════════════════════════════════════════════════════════════════════════
// Helper: Create a new compartment entity linked to a submarine root
// ════════════════════════════════════════════════════════════════════════════
void EditorUI::CreateNewCompartment(ECSRegistry& registry, Entity submarineRoot)
{
    Entity newComp = SubmarineSystem::AddCompartmentToSubmarine(
        registry,
        submarineRoot,
        XMFLOAT3(0, 0, 3),        // localOffset
        XMFLOAT3(3, 2.5f, 3.5f),  // compartmentSize
        500.0f                      // compartmentMass
    );

    if (newComp != INVALID_ENTITY)
    {
        // Подсвечиваем новый отсек
        m_selectedType = SelectedType::Object;
        m_selectedEntity = newComp;
    }
}

void EditorUI::StartCompartmentEdit(Entity compartment)
{
    m_editingCompartment = compartment;

    // Получаем компонент (предполагаем, что он есть, раз мы в редакторе)
    // В реальном коде лучше добавить проверку через registry
    // Но здесь мы вызываем это только из UI, где компонент уже проверен

    // Кэш будет обновляться в UpdateCacheFromCompartment при каждом кадре,
    // если отсек всё ещё выделен
}

// ════════════════════════════════════════════════════════════════════════════
// UpdateCacheFromCompartment — синхронизация: компонент → кэш
// ════════════════════════════════════════════════════════════════════════════
void EditorUI::UpdateCacheFromCompartment(const CompartmentComponent* comp)
{
    if (!comp) return;

    // Конвертируем Min/Max → Center/Size
    m_compartmentCenter = {
        (comp->localMin.x + comp->localMax.x) * 0.5f,
        (comp->localMin.y + comp->localMax.y) * 0.5f,
        (comp->localMin.z + comp->localMax.z) * 0.5f
    };

    m_compartmentSize = {
        comp->localMax.x - comp->localMin.x,
        comp->localMax.y - comp->localMin.y,
        comp->localMax.z - comp->localMin.z
    };
}

// ════════════════════════════════════════════════════════════════════════════
// UpdateCompartmentFromCache — синхронизация: кэш → компонент
// ════════════════════════════════════════════════════════════════════════════
void EditorUI::UpdateCompartmentFromCache(CompartmentComponent* comp)
{
    if (!comp) return;

    // Конвертируем Center/Size → Min/Max
    comp->localMin = {
        m_compartmentCenter.x - m_compartmentSize.x * 0.5f,
        m_compartmentCenter.y - m_compartmentSize.y * 0.5f,
        m_compartmentCenter.z - m_compartmentSize.z * 0.5f
    };

    comp->localMax = {
        m_compartmentCenter.x + m_compartmentSize.x * 0.5f,
        m_compartmentCenter.y + m_compartmentSize.y * 0.5f,
        m_compartmentCenter.z + m_compartmentSize.z * 0.5f
    };

    comp->RecalculateVolume();
}





// ════════════════════════════════════════════════════════════════════════════
// AddCompartmentToSubmarine — добавляет отсек к существующей подлодке
// ════════════════════════════════════════════════════════════════════════════
// 🔧 ПРОСТО создаёт пустую подлодку (без отсеков)
Entity EditorUI::CreateEmptySubmarine(SceneManager& sceneManager, const XMFLOAT3& position)
{
    auto& registry = sceneManager.GetECSRegistry();

    // Создаём корень
    Entity subRoot = registry.CreateEntity();

    // Transform
    registry.AddComponent<TransformComponent>(subRoot, TransformComponent{
        .position = position,
        .rotation = {0, 0, 0},
        .scale = {1, 1, 1}
        });

    // RigidBody (сфера как placeholder)
    RigidBodyComponent rb{};
    rb.mass = 3000.0f;  // Только корпус
    rb.shapeType = RigidBodyComponent::Sphere;
    rb.shapeRadius = 2.5f;
    rb.isDynamic = true;
    registry.AddComponent<RigidBodyComponent>(subRoot, rb);
    PhysicsSystem::CreateRigidBody(registry, subRoot);

    // Buoyancy
    registry.AddComponent<BuoyancyComponent>(subRoot, BuoyancyComponent{
        .fluidDensity = 1025.0f,
        .volume = 15.0f,  // Только корпус
        .height = 5.0f,
        .buoyancyMultiplier = 1.0f
        });

    // SubmarineComponent (пусто!)
    registry.AddComponent<SubmarineComponent>(subRoot, SubmarineComponent{
        .submarineRoot = subRoot
        });

    // Mesh (опционально - для визуализации корпуса)
    AssimpModelClass* subModel = sceneManager.GetOrCreateModel(
        "assets/model/sphere.obj", m_device, m_context, "assets/texture/stone.tga");

    if (subModel) {
        registry.AddComponent<MeshRendererComponent>(subRoot, MeshRendererComponent{
            .model = subModel,
            .material = nullptr
            });
    }

    OutputDebugStringA(("✅ Created empty submarine #" + std::to_string(subRoot) + "\n").c_str());

    return subRoot;
}

// 🔧 Добавить отсек к подлодке (обновленная версия)
Entity EditorUI::AddCompartmentToSubmarine(SceneManager& sceneManager,
    Entity submarineRoot,
    const XMFLOAT3& localOffset,
    const XMFLOAT3& size,
    float mass)
{
    auto& registry = sceneManager.GetECSRegistry();

    Entity newComp = SubmarineSystem::AddCompartmentToSubmarine(
        registry,
        submarineRoot,
        localOffset,
        size,
        mass
    );

    if (newComp != INVALID_ENTITY)
    {
        // Отмечаем подлодку как нуждающуюся в обновлении физики
        auto* rootSub = registry.GetComponent<SubmarineComponent>(submarineRoot);
        if (rootSub)
        {
            rootSub->needsPhysicsUpdate = true;
        }

        // Выбираем новый отсек в редакторе (опционально)
        m_selectedType = SelectedType::Object;
        m_selectedEntity = newComp;
    }

    return newComp;
}


// 🔧 Добавление отсека с мешем
Entity EditorUI::AddCompartmentWithMesh(
    SceneManager& sceneManager,
    Entity submarineRoot,
    const XMFLOAT3& localOffset,
    const XMFLOAT3& size,
    float mass,
    const char* modelPath)
{
    auto& registry = sceneManager.GetECSRegistry();

    // Используем SubmarineSystem для создания отсека
    Entity compartment = SubmarineSystem::AddCompartmentToSubmarine(
        registry,
        submarineRoot,
        localOffset,
        size,
        mass
    );

    if (compartment == INVALID_ENTITY)
        return INVALID_ENTITY;

    // 🔥 ВАЖНО: Добавляем МЕШ отсеку!
    AssimpModelClass* compModel = sceneManager.GetOrCreateModel(
        modelPath, m_device, m_context, "assets/texture/stone.tga");

    if (compModel)
    {
        registry.AddComponent<MeshRendererComponent>(compartment, MeshRendererComponent{
            .model = compModel,
            .material = nullptr
            });
    }

    // Отмечаем подлодку как нуждающуюся в обновлении
    auto* rootSub = registry.GetComponent<SubmarineComponent>(submarineRoot);
    if (rootSub)
    {
        rootSub->needsPhysicsUpdate = true;
    }

    OutputDebugStringA(("✅ Added compartment #" + std::to_string(compartment) +
        " with model " + std::string(modelPath) + "\n").c_str());

    return compartment;
}


Entity EditorUI::CreateSubmarineWithMesh(
    SceneManager& sceneManager,
    const XMFLOAT3& position,
    const char* modelPath,
    float hullRadius,
    float hullMass)
{
    auto& registry = sceneManager.GetECSRegistry();

    // 1. Создаём корень подлодки
    Entity subRoot = registry.CreateEntity();

    // Transform
    registry.AddComponent<TransformComponent>(subRoot, TransformComponent{
        .position = position,
        .rotation = {0, 0, 0},
        .scale = {hullRadius, hullRadius, hullRadius}
        });

    // RigidBody
    RigidBodyComponent rb{};
    rb.mass = hullMass;
    rb.shapeType = RigidBodyComponent::Sphere;
    rb.shapeRadius = hullRadius;
    rb.isDynamic = true;
    registry.AddComponent<RigidBodyComponent>(subRoot, rb);
    PhysicsSystem::CreateRigidBody(registry, subRoot);

    // Buoyancy
    registry.AddComponent<BuoyancyComponent>(subRoot, BuoyancyComponent{
        .fluidDensity = 1025.0f,
        .volume = 15.0f * (hullRadius / 3.0f),
        .height = hullRadius * 2.0f,
        .buoyancyMultiplier = 1.0f
        });

    // SubmarineComponent
    registry.AddComponent<SubmarineComponent>(subRoot, SubmarineComponent{
        .submarineRoot = subRoot
        });

    // 🔥 ВАЖНО: Добавляем МЕШ!
    AssimpModelClass* hullModel = sceneManager.GetOrCreateModel(
        modelPath, m_device, m_context, "assets/texture/stone.tga");

    if (hullModel)
    {
        registry.AddComponent<MeshRendererComponent>(subRoot, MeshRendererComponent{
            .model = hullModel,
            .material = nullptr
            });
    }

    OutputDebugStringA(("✅ Created submarine #" + std::to_string(subRoot) +
        " with model " + std::string(modelPath) + "\n").c_str());

    return subRoot;
}


void EditorUI::UpdateBreachesOnResize(CompartmentComponent* comp, const XMFLOAT3& oldMin, const XMFLOAT3& oldMax)
{
    if (!comp) return;
    float oldHeight = oldMax.y - oldMin.y;
    float newHeight = comp->localMax.y - comp->localMin.y;
    float scaleY = (oldHeight > 0.0f) ? (newHeight / oldHeight) : 1.0f;

    for (auto& breach : comp->breaches)
    {
        // Пересчёт Y-координаты относительно нового размера (оставляем относительное положение)
        float relY = (breach.localPosition.y - oldMin.y) / oldHeight;
        breach.localPosition.y = comp->localMin.y + relY * newHeight;
        // Аналогично можно сделать для X и Z, если нужно
        float oldWidth = oldMax.x - oldMin.x;
        float newWidth = comp->localMax.x - comp->localMin.x;
        float relX = (breach.localPosition.x - oldMin.x) / oldWidth;
        breach.localPosition.x = comp->localMin.x + relX * newWidth;

        float oldDepth = oldMax.z - oldMin.z;
        float newDepth = comp->localMax.z - comp->localMin.z;
        float relZ = (breach.localPosition.z - oldMin.z) / oldDepth;
        breach.localPosition.z = comp->localMin.z + relZ * newDepth;
    }
}