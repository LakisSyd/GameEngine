#pragma once
#include <imgui.h>
#include <string>
#include <array>
#include "SceneManager.h"
#include "GameSettings.h"
#include "FileSystem.h"
#include "ShaderManager.h"
#include "PhysicsDebugRenderer.h"
#include "../../GameplayController.h"

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
    void UpdateDebugOverlay(SceneManager& sceneManager);
    void BeginMainMenuBar(SceneManager& sceneManager);
    void Render();


    // Для доступа к состоянию редактирования отсеков из ApplicationClass
    bool IsEditingCompartment() const { return m_compartmentEditMode != CompartmentEditMode::None; }
    Entity GetEditingCompartmentEntity() const { return m_editingCompartment; }
    enum class CompartmentEditMode { None, Move, Resize };
    CompartmentEditMode GetCompartmentEditMode() const { return m_compartmentEditMode; }
    XMFLOAT3 GetCompartmentCenter() const { return m_compartmentCenter; }
    XMFLOAT3 GetCompartmentSize() const { return m_compartmentSize; }

    void SetCompartmentCenter(const XMFLOAT3& center) { m_compartmentCenter = center; }
    void SetCompartmentSize(const XMFLOAT3& size) { m_compartmentSize = size; }
    void ApplyCompartmentCacheToComponent(CompartmentComponent* comp) { UpdateCompartmentFromCache(comp); }



    enum class SelectedType { None, Object, Sprite, Light };

    Entity       GetSelectedEntity() const { return m_selectedEntity; }
    SelectedType GetSelectedType()   const { return m_selectedType; }

    int GetSelectedIndex() const
    {
        return (m_selectedEntity != INVALID_ENTITY)
            ? static_cast<int>(m_selectedEntity) : m_selectedLegacyIndex;
    }

    void SetSelection(SelectedType type, int index)
    {
        m_selectedType = type;
        m_selectedLegacyIndex = index;
        // Для света тоже сохраняем индекс как Entity (для совместимости)
        m_selectedEntity = static_cast<Entity>(index);
    }



    void SetScenePreviewTexture(ID3D11ShaderResourceView* srv);

    struct SpawnRequest
    {
        bool        pending = false;
        std::string modelPath;
        std::string collisionModelPath;

        std::string texturePath;
        float       posX = 0.0f;
        float       posY = 0.0f;
        float       posZ = 0.0f;
        bool useSeparateCollisionMesh;
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


    PhysicsDebugSettings& GetPhysicsDebugSettings() { return m_debugSettings; }


    bool ConsumeRebuildGrassRequest()
    {
        if (!m_rebuildGrassRequested)
            return false;

        m_rebuildGrassRequested = false;
        return true;
    }


    bool WantCaptureMouse() const;
    void ToggleObjectDebugWindow() { m_showObjectDebugWindow = !m_showObjectDebugWindow; }
    bool IsObjectDebugWindowVisible() const { return m_showObjectDebugWindow; }
    void RefreshShaderList() { m_shaderNames = ShaderManager::Get().GetNames(); }

private:
    // ── Состояние выделения ───────────────────────────────────────────────────
    SelectedType m_selectedType = SelectedType::None;
    Entity       m_selectedEntity = INVALID_ENTITY;
    int          m_selectedLegacyIndex = -1;
    bool         m_showObjectDebugWindow = false;

    void RenderPhysicsDebugPanel();
    void CreateNewCompartment(ECSRegistry& registry, Entity submarineRoot);
    ID3D11ShaderResourceView* m_scenePreviewSRV = nullptr;

    SpawnRequest      m_spawnRequest;
    FileActionRequest m_saveRequest;
    FileActionRequest m_loadRequest;


    bool useSeparateCollisionMesh = false;

    char  m_savePathBuf[256] = "scene.json";
    char  m_loadPathBuf[256] = "scene.json";

    bool  m_openSaveDialog = false;
    bool  m_openLoadDialog = false;

    char  m_spawnModelPath[256] = "assets/model/sphere.obj";
	char m_spawnCollisionModelPath[256] = "physics_";
    char  m_spawnTexturePath[256] = "assets/texture/stone.tga";
    float m_spawnPos[3] = { 0.0f, 0.0f, 0.0f };

    char m_skyboxPathBuf[256] = "assets/texture/cubemap/day.hdr";

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
    static constexpr float SIDEBAR_WIDTH_RATIO = 500.0f / BASE_WIDTH;
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
    void RenderObjectDebugWindow(SceneManager& sceneManager);
    void RenderSelectedObjectDebugInfo(SceneManager& sceneManager, Entity entity);
    void RenderSpawnPanel(SceneManager& sceneManager);
    void RenderFogPanel(SceneManager& sceneManager);
    void RenderScenePreview();

    void RenderObjectProperties(SceneManager& sceneManager, Entity entity);
    void RenderComponentManager(SceneManager& sceneManager, Entity entity); // ← НОВЫЙ
    void RenderSpriteProperties(WorldSpriteClass* sprite);
    void RenderLightPropertiesByEntity(SceneManager& sceneManager, Entity lightEntity);

    static void SectionHeader(const char* label);

    void RenderSaveDialog();
    void RenderLoadDialog();


    void RenderPhysicsSettings(SceneManager& sceneManager);


    void RenderJointConnectionUI(ECSRegistry& registry, Entity selectedEntity);

    void AutoCalculatePivots(ECSRegistry& registry, Entity entity, JointConnectionComponent& jointComp);

    void RenderCompartmentEditor(SceneManager& sceneManager);



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

    PhysicsDebugSettings m_debugSettings;


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
    char m_terrainGrassPathBuf[256] = "assets/terrain/textures/grass_base.png";
    char m_terrainDirtPathBuf[256] = "assets/terrain/textures/dirt_base.png";
    char m_terrainMaskPathBuf[256] = "assets/terrain/masks/dirt_mask.png";
    char m_terrainGrassNormalPathBuf[256] = "assets/terrain/textures/grass_normal.jpg";
    char m_terrainDirtNormalPathBuf[256] = "assets/terrain/textures/dirt_normal.jpg";
    char m_terrainGrassVariationPathBuf[256] = "assets/terrain/textures/grass_variation_base.png";
    char m_terrainGrassVariationNormalPathBuf[256] = "assets/terrain/textures/grass_variation_normal.jpg";
    float m_terrainTileScale = 2.0f;
    float m_terrainNormalStrength = 1.0f;
    float m_terrainVariationScale = 0.08f;
    float m_terrainVariationStrength = 1.0f;
    float m_terrainBrightness = 1.0f;
    float m_terrainAmbientBoost = 1.0f;
    float m_terrainMinXZ[2] = { -50.0f, -50.0f };
    float m_terrainSizeXZ[2] = { 100.0f, 100.0f };

    ID3D11Device* m_device = nullptr;
    ID3D11DeviceContext* m_context = nullptr;
    bool m_initialized = false;

    int                      m_activeFilterIndex = 0;
    std::vector<std::string> m_filterExtensions = { "", ".glb", ".fbx", ".obj", ".png", ".jpg", ".dds" };
    std::vector<std::string> m_filterLabels = { "All", "Models(GLB)", "Models(FBX)", "Models(OBJ)", "Textures(PNG)", "Textures(JPG)", "Textures(DDS)" };

    char m_searchBuf[128] = {};

    bool        m_saveInProgress = false;
    bool        m_loadInProgress = false;
    std::string m_lastSavePath;
    HWND        m_hwnd = nullptr;

    bool enableCCD;

    // В классе EditorUI, в private-секцию:


    



    // Для визуального редактирования отсеков
    CompartmentEditMode m_compartmentEditMode = CompartmentEditMode::None;
    Entity m_editingCompartment = INVALID_ENTITY;
    XMFLOAT3 m_compartmentCenter = { 0, 0, 0 };  // Кэш центра для редактора
    XMFLOAT3 m_compartmentSize = { 2, 2, 2 };     // Кэш размера для редактора
    public:
    int  m_dragCornerIndex = -1;       // какой угол тянем (0..7)
    bool m_cornerDragActive = false;    // в процессе перетаскивания?
    XMFLOAT3 m_dragCornerStartMin = {};  // localMin в момент начала drag
    XMFLOAT3 m_dragCornerStartMax = {};  // localM
    void UpdateCompartmentFromCache(CompartmentComponent* comp);
   
    
   

    // Corner edit — доступ для ApplicationClass
    bool  IsCornerEditActive()    const { return m_cornerEditActive; }
    Entity GetCornerEditEntity()  const { return m_cornerEditEntity; }
    int   GetCornerEditElemIndex() const { return m_cornerEditElemIndex; }
   
    bool   m_cornerEditActive = false;  // включён ли режим угловых ручек
    Entity m_cornerEditEntity = INVALID_ENTITY; // какая entity редактируется
    int    m_cornerEditElemIndex = -1;     // индекс compound элемента
    int    m_cornerDragIndex = -1;     // какой угол тянем (-1 = нет)
  
    XMFLOAT3 m_cornerDragStartMin = {};
    XMFLOAT3 m_cornerDragStartMax = {};

    
    bool m_rebuildGrassRequested = false;


    // Синхронизация кэша из внешнего кода (ApplicationClass → corner drag)
    void UpdateCacheFromCompartment_Public(const CompartmentComponent* comp) { UpdateCacheFromCompartment(comp); }
    private:
    // Вспомогательные методы
    void StartCompartmentEdit(Entity compartment);
   
    Entity CreateEmptySubmarine(SceneManager& sceneManager, const XMFLOAT3& position);

    void CreateCompleteSubmarine(SceneManager& sceneManager, const XMFLOAT3& position, int numCompartments);
    Entity AddCompartmentToSubmarine(SceneManager& sceneManager, Entity submarineRoot, const XMFLOAT3& localOffset, const XMFLOAT3& size, float mass);
    Entity AddCompartmentWithMesh(SceneManager& sceneManager, Entity submarineRoot, const XMFLOAT3& localOffset, const XMFLOAT3& size, float mass, const char* modelPath);
    Entity CreateSubmarineWithMesh(SceneManager& sceneManager, const XMFLOAT3& position, const char* modelPath, float hullRadius, float hullMass);
    void UpdateBreachesOnResize(CompartmentComponent* comp, const XMFLOAT3& oldMin, const XMFLOAT3& oldMax);
    void DrawSubmarineCollisionSetup(ECSRegistry& registry, Entity entity);
    void RenderWorldAreaSpawner(SceneManager& sceneManager);
    void UpdateCacheFromCompartment(const CompartmentComponent* comp);
    void RenderCompartmentGizmo(ECSRegistry& registry, const XMMATRIX& view, const XMMATRIX& projection);
};
