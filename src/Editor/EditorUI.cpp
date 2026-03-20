// EditorUI.cpp
#include "EditorUI.h"
#include <string>
#include "SceneManager.h"
#include <backends/imgui_impl_win32.h>
#include <imgui-master/backends/imgui_impl_dx11.h>

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

    // ── Объекты (ECS) ─────────────────────────────────────────────────────────
    auto& meshPool = sceneManager.GetECSRegistry().GetComponents<MeshRendererComponent>();
    for (auto& [entity, mesh] : meshPool)
    {
        if (!sceneManager.IsObjectActive(entity)) continue;

        char label[64];
        snprintf(label, sizeof(label), "  Object [%u]", entity);

        bool selected = (m_selectedType == SelectedType::Object && m_selectedEntity == entity);
        if (selected) ImGui::PushStyleColor(ImGuiCol_Header, COL_SELECTED);

        if (ImGui::Selectable(label, selected))
            SetSelection(SelectedType::Object, entity);

        if (selected) ImGui::PopStyleColor();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ── Источники света ───────────────────────────────────────────────────────
    for (int i = 0; i < NUM_LIGHTS; ++i)
    {
        if (!sceneManager.IsLightActive(i)) continue;

        char label[64];
        snprintf(label, sizeof(label), "  Light #%d", i);

        bool selected = (m_selectedType == SelectedType::Light && m_selectedLegacyIndex == i);
        if (selected) ImGui::PushStyleColor(ImGuiCol_Header, COL_SELECTED);

        if (ImGui::Selectable(label, selected))
            SetSelection(SelectedType::Light, i);

        if (selected) ImGui::PopStyleColor();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ── Спрайты ───────────────────────────────────────────────────────────────
    for (int i = 0; i < sceneManager.GetSpriteCount(); ++i)
    {
        char label[64];
        snprintf(label, sizeof(label), "  Sprite #%d", i);

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
    ImGui::Spacing();

    switch (m_selectedType)
    {
    case SelectedType::Object:
        if (m_selectedEntity != INVALID_ENTITY)
            RenderObjectProperties(sceneManager, m_selectedEntity);
        else
            ImGui::TextColored(COL_TEXT_DIM, "No object selected");
        break;

    case SelectedType::Light:
        RenderLightProperties(sceneManager, m_selectedLegacyIndex);
        break;

    case SelectedType::Sprite:
        RenderSpriteProperties(sceneManager.GetSprite(m_selectedLegacyIndex));
        break;

    default:
        ImGui::TextColored(COL_TEXT_DIM, "Select something in Outliner");
        break;
    }
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

        // Light Component
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

        // Сюда легко добавить новые компоненты в будущем:
        // if (ImGui::Selectable("  Rigidbody")) { ... }
        // if (ImGui::Selectable("  Audio Source")) { ... }

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
        if (ImGui::BeginTabItem("Details"))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
            RenderDetailsPanel(sceneManager);
            ImGui::PopStyleVar();
            ImGui::EndTabItem();
        }
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
void EditorUI::RenderLightProperties(SceneManager& sceneManager, int lightIndex)
{
    if (lightIndex < 0 || lightIndex >= NUM_LIGHTS) return;

    SectionHeader("Light");
    ImGui::Spacing();

    auto pos = sceneManager.GetLightPosition(lightIndex);
    auto color = sceneManager.GetLightColor(lightIndex);

    float p[3] = { pos.x,   pos.y,   pos.z };
    float c[3] = { color.x, color.y, color.z };

    ImGui::TextColored(COL_TEXT_DIM, " Position");
    if (DragFloat3Colored("##LPos", p, 0.1f))
        sceneManager.SetLightPosition(lightIndex, { p[0], p[1], p[2] });

    ImGui::Spacing();

    ImGui::TextColored(COL_TEXT_DIM, " Color");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::ColorEdit3("##LCol", c))
        sceneManager.SetLightColor(lightIndex, XMFLOAT4(c[0], c[1], c[2], 1.0f));

    ImGui::Spacing();

    bool active = sceneManager.IsLightActive(lightIndex);
    if (ImGui::Checkbox("Active", &active))
        sceneManager.SetLightActive(lightIndex, active);

    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Button, COL_RED);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
    if (ImGui::Button("Remove Light", ImVec2(-1, 0)))
    {
        sceneManager.SetLightActive(lightIndex, false);
        m_selectedLegacyIndex = -1;
        m_selectedType = SelectedType::None;
    }
    ImGui::PopStyleColor(2);
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
    auto entries = m_fileSystem->GetFilteredEntries(m_currentDir.string(), query, m_filterExtensions[m_activeFilterIndex]);

    RenderFileBrowserList(entries);
}

void EditorUI::RenderFileBrowserToolbar()
{
    const float btnW = std::floor(28.0f * ScaleX());

    if (ImGui::Button("<##Back", ImVec2(btnW, 0)) && m_fileSystem)
    {
        auto parent = fs::path(m_currentDir).parent_path();
        if (!parent.empty()) m_currentDir = parent;
        m_fileSystem->Refresh();
    }
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