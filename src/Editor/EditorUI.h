// EditorUI.h
#pragma once
#include <imgui.h>
#include <string>
#include <array>
#include "SceneManager.h"
#include "GameSettings.h"
#include "FileSystem.h"
#include "ShaderManager.h"

class EditorUI
{
public:
    EditorUI() = default;
    ~EditorUI();

    bool Initialize(HWND hwnd, ID3D11Device* device, ID3D11DeviceContext* context, FileSystem* fs);
    void Shutdown();
    void NewFrame();
    void RenderMainMenu(SceneManager& sceneManager);
    void Update(SceneManager& sceneManager);
    void BeginMainMenuBar(SceneManager& sceneManager);
    void Render();

    enum class SelectedType { None, Object, Sprite, Light };

    Entity       GetSelectedEntity() const { return m_selectedEntity; }
    SelectedType GetSelectedType()   const { return m_selectedType; }

    int GetSelectedIndex() const
    {
        return (m_selectedEntity != INVALID_ENTITY)
            ? static_cast<int>(m_selectedEntity) : m_selectedLegacyIndex;
    }

    void SetSelection(SelectedType type, Entity entity)
    {
        m_selectedType = type;
        m_selectedEntity = entity;
        m_selectedLegacyIndex = (entity != INVALID_ENTITY)
            ? static_cast<int>(entity) : -1;
    }

    void SetSelection(SelectedType type, int index)
    {
        m_selectedType = type;
        m_selectedLegacyIndex = index;
        m_selectedEntity = (type == SelectedType::Object)
            ? static_cast<Entity>(index) : INVALID_ENTITY;
    }

    void SetScenePreviewTexture(ID3D11ShaderResourceView* srv);

    struct SpawnRequest
    {
        bool        pending = false;
        std::string modelPath;
        std::string texturePath;
        float       posX = 0.0f;
        float       posY = 0.0f;
        float       posZ = 0.0f;
    };
    SpawnRequest ConsumeSpawnRequest()
    {
        SpawnRequest req = m_spawnRequest;
        m_spawnRequest.pending = false;
        return req;
    }

    struct FileActionRequest
    {
        bool        pending = false;
        std::string path;
    };
    FileActionRequest ConsumeSaveRequest()
    {
        FileActionRequest req = m_saveRequest;
        m_saveRequest.pending = false;
        return req;
    }
    FileActionRequest ConsumeLoadRequest()
    {
        FileActionRequest req = m_loadRequest;
        m_loadRequest.pending = false;
        return req;
    }

    bool WantCaptureMouse() const;
    void RefreshShaderList() { m_shaderNames = ShaderManager::Get().GetNames(); }

private:
    // ── Состояние выделения ───────────────────────────────────────────────────
    SelectedType m_selectedType = SelectedType::None;
    Entity       m_selectedEntity = INVALID_ENTITY;
    int          m_selectedLegacyIndex = -1;

    ID3D11ShaderResourceView* m_scenePreviewSRV = nullptr;

    SpawnRequest      m_spawnRequest;
    FileActionRequest m_saveRequest;
    FileActionRequest m_loadRequest;

    char  m_savePathBuf[256] = "scene.json";
    char  m_loadPathBuf[256] = "scene.json";

    bool  m_openSaveDialog = false;
    bool  m_openLoadDialog = false;

    char  m_spawnModelPath[256] = "assets/model/sphere.obj";
    char  m_spawnTexturePath[256] = "assets/texture/stone.tga";
    float m_spawnPos[3] = { 0.0f, 0.0f, 0.0f };

    static constexpr int PRESET_COUNT = 4;
    static constexpr std::array<const char*, PRESET_COUNT> MODEL_PRESETS = {
        "assets/model/sphere.obj",
        "assets/model/cube.obj",
        "assets/model/plane.obj",
        "assets/model/cylinder.obj"
    };
    static constexpr std::array<const char*, PRESET_COUNT> TEXTURE_PRESETS = {
        "assets/texture/stone.tga",
        "assets/texture/dirt.tga",
        "assets/texture/uv.tga",
        "assets/texture/arrow.tga"
    };

    static constexpr float BASE_WIDTH = 1920.0f;
    static constexpr float BASE_HEIGHT = 1280.0f;
    static constexpr float SIDEBAR_WIDTH_RATIO = 300.0f / BASE_WIDTH;
    static constexpr float PREVIEW_WIDTH_RATIO = 280.0f / BASE_WIDTH;
    static constexpr float PREVIEW_HEIGHT_RATIO = 210.0f / BASE_HEIGHT;

    static float ScaleX() { return ImGui::GetIO().DisplaySize.x / BASE_WIDTH; }
    static float ScaleY() { return ImGui::GetIO().DisplaySize.y / BASE_HEIGHT; }
    static float Scale() { return std::min(ScaleX(), ScaleY()); }
    static float SidebarWidth() { return std::floor(BASE_WIDTH * SIDEBAR_WIDTH_RATIO * ScaleX()); }

    float m_menuBarHeight = 0.0f;

    // ── Методы рендера ────────────────────────────────────────────────────────
    void RenderSidebar(SceneManager& sceneManager);
    void RenderSceneOutliner(SceneManager& sceneManager);
    void RenderDetailsPanel(SceneManager& sceneManager);
    void RenderSpawnPanel(SceneManager& sceneManager);
    void RenderFogPanel(SceneManager& sceneManager);
    void RenderScenePreview();

    void RenderObjectProperties(SceneManager& sceneManager, Entity entity);
    void RenderComponentManager(SceneManager& sceneManager, Entity entity); // ← НОВЫЙ
    void RenderSpriteProperties(WorldSpriteClass* sprite);
    void RenderLightProperties(SceneManager& sceneManager, int lightIndex);

    static void SectionHeader(const char* label);

    void RenderSaveDialog();
    void RenderLoadDialog();

    // Content Browser
    void RenderContentBrowser();
    void RenderFileBrowserToolbar();
    void RenderFileBrowserBreadcrumb();
    void RenderFileBrowserList(const std::vector<const FileEntry*>& entries);
    void RenderFileBrowserItemGrid(const FileEntry* entry, size_t index);
    void RenderFileBrowserItemList(const FileEntry* entry, size_t index);
    void OnEntryDoubleClicked(const FileEntry* entry);
    void OnEntrySelected(const FileEntry* entry);
    void OnFileSelected(const FileEntry* entry);

    // Knife Tool
    void RenderKnifeTool(SceneManager& sceneManager, ClipPlaneShaderClass* clipShader);

    bool             m_knifeEnabled = false;
    int              m_knifeAxis = 1;
    float            m_knifePosition = 0.0f;
    bool             m_knifeFlip = false;
    Entity           m_knifeTargetEntity = INVALID_ENTITY;
    ShaderBaseClass* m_knifeOldShader = nullptr;

    FileSystem* m_fileSystem = nullptr;
    fs::path         m_currentDir;
    std::string      m_searchQuery;
    bool             m_viewModeGrid = true;
    const FileEntry* m_selectedEntry = nullptr;

    // Shader Selector
    void RenderShaderSelector(SceneManager& sceneManager);

    int                      m_selectedShaderIndex = 0;
    std::vector<std::string> m_shaderNames;

    char m_tex0PathBuf[256] = "assets/texture/stone.tga";
    char m_tex1PathBuf[256] = "assets/texture/dirt.tga";
    char m_tex2PathBuf[256] = "";   // metalness — пустой по умолчанию
    char m_tex3PathBuf[256] = "";   // roughness — пустой по умолчанию

    ID3D11Device* m_device = nullptr;
    ID3D11DeviceContext* m_context = nullptr;

    int                      m_activeFilterIndex = 0;
    std::vector<std::string> m_filterExtensions = { "", ".fbx", ".obj", ".png", ".jpg", ".dds" };
    std::vector<std::string> m_filterLabels = { "All", "Models(FBX)", "Models(OBJ)", "Textures(PNG)", "Textures(JPG)", "Textures(DDS)" };

    char m_searchBuf[128] = {};

    bool        m_saveInProgress = false;
    bool        m_loadInProgress = false;
    std::string m_lastSavePath;
    HWND        m_hwnd = nullptr;
};