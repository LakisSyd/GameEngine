#pragma once
#include <vector>
#include <array>
#include <memory>
#include <unordered_map>
#include <DirectXMath.h>

// ============================================================================
// РќР•Р”Р•Р›РЇ 4: gameobject.h РЅСѓР¶РµРЅ С‚РѕР»СЊРєРѕ РґР»СЏ light-СЃС„РµСЂ (SetupLightSpheres).
// GetObject(int) СѓРґР°Р»С‘РЅ вЂ” EditorUI Рё PickingSystem РїРµСЂРµС€Р»Рё РЅР° Entity.
// m_legacyObjects СѓРґР°Р»С‘РЅ.
// ============================================================================
#include "gameobject.h"       // С‚РѕР»СЊРєРѕ РґР»СЏ light-СЃС„РµСЂ (SetupLightSpheres / RenderLightSpheres)
#include "assimpmodelclass.h"
#include "lightshaderclass.h"
#include "WorldSpriteClass.h"
#include "clipplaneshaderclass.h"
#include "MaterialClass.h"
#include "Json/json.hpp"
#include "GameSettings.h"
#include "ECSRegistry.h"
#include "RenderSystem.h"

using namespace DirectX;
using json = nlohmann::json;

class AssetLoadSystem;
struct WeaponComponent;

struct AsyncShipImportSnapshot
{
    bool active = false;
    bool failed = false;
    bool finished = false;
    float progress = 0.0f;
    size_t completedItems = 0;
    size_t totalItems = 0;
    std::string path;
    std::string stage;
    std::string currentItem;
    std::string error;
};


struct WorldSettings
{
    std::string skyboxPath = "assets/texture/cubemap/day.hdr";

    float skyExposure = 1.0f;

    XMFLOAT3 sunDirection = { -0.3f, -1.0f, 0.2f };
    float sunIntensity = 1.0f;

    float ambientStrength = 0.15f;

    bool fogEnabled = false;
    XMFLOAT3 fogColor = { 0.6f, 0.7f, 0.8f };
    float fogStart = 20.0f;
    float fogEnd = 100.0f;

    float shadowDistance = 100.0f;

    bool skyboxReloadPending = false;
};


#ifndef NUM_LIGHTS
#define NUM_LIGHTS 4
#endif

class SceneManager
{
public:


    SceneManager();
    ~SceneManager();

    DirectX::XMMATRIX ConvertAssimpMatrix(const aiMatrix4x4& m);
    void ImportShipNodeRecursive(
        const aiScene* scene,
        aiNode* node,
        const DirectX::XMMATRIX& parentWorld,
        Entity shipRoot,
        ID3D11Device* device,
        ID3D11DeviceContext* context,
        const std::unordered_map<std::string, aiNode*>* collisionProxyNodes = nullptr);

    Entity ImportShipFromGlb(const std::string& path, ID3D11Device* device, ID3D11DeviceContext* context);
    bool RequestShipImportAsync(const std::string& path);
    void UpdateAsyncShipImport(
        ID3D11Device* device,
        ID3D11DeviceContext* context,
        float mainThreadBudgetMilliseconds = 4.0f);
    void AssignRoomMembership();
    AsyncShipImportSnapshot GetAsyncShipImportSnapshot() const;
    bool HasImportedShip();
    Entity FindRoomByName(const std::string& roomName);
    bool ConnectRooms(Entity roomA, Entity roomB);
    bool ConnectRoomsByName(const std::string& roomAName, const std::string& roomBName);
    void ConnectDefaultShipRooms();
    void ResolveDoorRoomLinks();
   

    // =========================================================================
    // РЈРїСЂР°РІР»РµРЅРёРµ РѕР±СЉРµРєС‚Р°РјРё
    // =========================================================================
    Entity SpawnObject(AssimpModelClass* model, float x, float y, float z);
    Entity CreateWorldArea(WorldAreaType type);
    bool   RemoveObject(Entity entity);
    void   Clear();

    void SetRefractionSRV(ID3D11ShaderResourceView* srv) { m_refractionSRV = srv; }

    // -- ECS-РґРѕСЃС‚СѓРї ------------------------------------------------------------
    TransformComponent* GetObjectTransform(Entity entity)
    {
        return m_registry.GetComponent<TransformComponent>(entity);
    }

    MeshRendererComponent* GetObjectMesh(Entity entity)
    {
        return m_registry.GetComponent<MeshRendererComponent>(entity);
    }

    void SetObjectMaterial(Entity entity, MaterialClass* mat)
    {
        if (auto* mesh = m_registry.GetComponent<MeshRendererComponent>(entity))
            mesh->material = mat;
    }


    WorldSettings& GetWorldSettings();
    const WorldSettings& GetWorldSettings() const;
    void SetWorldSettings(const WorldSettings& settings);
	std::string GetSkyboxPath() const { return GetWorldSettings().skyboxPath; }


    bool IsObjectActive(Entity entity)
    {
        return m_registry.HasComponent<ActiveComponent>(entity);
    }

    // РљРѕР»РёС‡РµСЃС‚РІРѕ РѕР±СЉРµРєС‚РѕРІ (MeshRenderer + Active)
    size_t GetObjectCount() const
    {
        return const_cast<ECSRegistry&>(m_registry)
            .GetComponents<MeshRendererComponent>().GetCount();
    }

    // =========================================================================
    // РЎРѕС…СЂР°РЅРµРЅРёРµ / Р—Р°РіСЂСѓР·РєР°
    // =========================================================================
    bool SaveScene(const std::string& filename);
    bool LoadScene(const std::string& filename,
        ID3D11Device* device, ID3D11DeviceContext* deviceContext);
    bool SaveWeaponProfile(
        const WeaponComponent& weapon,
        const std::string& filename = "assets/config/weapon_grip.json") const;
    bool LoadWeaponProfile(
        WeaponComponent& weapon,
        const std::string& filename = "assets/config/weapon_grip.json") const;

    // =========================================================================
    // РЎРІРµС‚
    // =========================================================================

    void UpdateLights();

    void RemapEntityReferences(const std::unordered_map<uint32_t, Entity>& remap);

    void ResolveRepairZones();

    Entity SpawnObject(SpawnObjectDesc&& desc);



    // Convenience setters for individual light properties (used by EditorUI)


    // =========================================================================
    // РЎРїСЂР°Р№С‚С‹
    // =========================================================================
    int               SpawnSprite(ID3D11Device* device, ID3D11DeviceContext* deviceContext,
        const char* textureName,
        float x, float y, float z, float width, float height);
    bool              RemoveSprite(int index);
    WorldSpriteClass* GetSprite(int index);
    int               GetSpriteCount() { return static_cast<int>(m_sprites.size()); }
    void              RenderSprites(ID3D11DeviceContext* context,
        XMMATRIX view, XMMATRIX projection,
        TextureShaderClass* shader);

    // =========================================================================
    // РљСЌС€ РјРѕРґРµР»РµР№
    // =========================================================================
    AssimpModelClass* GetOrCreateModel(
        const std::string& path,
        ID3D11Device* device, ID3D11DeviceContext* deviceContext,
        const std::string& texturePath =
        "C:\\Users\\Lakis\\Desktop\\DX_FrameWork\\DX_FrameWork\\assets\\texture\\stone.tga",
        bool allowFallback = true);

    // Set shader on an object's material.
    // device is required to create a material if the object doesn't have one yet.
    void SetObjectShader(Entity entity, ShaderBaseClass* shader, ID3D11Device* device = nullptr);

    void SetObjectTexture(Entity entity, int slot, const std::string& path,
        ID3D11Device* device, ID3D11DeviceContext* context, bool forceSRGB = false);

    void SetLightModel(AssimpModelClass* model) { m_lightModel = model; }        // ? РќРћР’РћР•
    AssimpModelClass* GetLightModel() const { return m_lightModel; }
    void SetAssetLoadSystem(AssetLoadSystem* system) { m_assetLoadSystem = system; }
    AssetLoadSystem* GetAssetLoadSystem() const { return m_assetLoadSystem; }

    // =========================================================================
    // РћР±РЅРѕРІР»РµРЅРёРµ Рё СЂРµРЅРґРµСЂ
    // =========================================================================
    void UpdateAll(float deltaTime, Entity player);

    int RenderAllECS(
        ID3D11DeviceContext* context,
        const XMMATRIX& viewMatrix,
        const XMMATRIX& projectionMatrix,
        MaterialClass* defaultMaterial,
        XMFLOAT4        lightColors[],
        XMFLOAT4        lightPositions[],
        const Settings::FogSettings& fog,
        Entity selectedEntity = INVALID_ENTITY, XMFLOAT3 cameraPos = { 0.0f, 0.0f, 0.0f }, float deltaTime = 0.0f);

    int RenderFirstPersonECS(
        ID3D11DeviceContext* context,
        const XMMATRIX& viewMatrix,
        const XMMATRIX& projectionMatrix,
        MaterialClass* defaultMaterial,
        const Settings::FogSettings& fog,
        Entity localPlayer,
        XMFLOAT3 cameraPos,
        float deltaTime = 0.0f);

    void SetFirstPersonViewContext(bool active, Entity localPlayer)
    {
        m_firstPersonViewActive = active;
        m_firstPersonLocalPlayer = localPlayer;
    }

    void RenderAllECSExcluding(
        ID3D11DeviceContext* context,
        const XMMATRIX& viewMatrix,
        const XMMATRIX& projectionMatrix,
        MaterialClass* defaultMaterial,
        XMFLOAT4        lightColors[],
        XMFLOAT4        lightPositions[],
        const Settings::FogSettings& fog,
        Entity excludeEntity = INVALID_ENTITY,
        float deltaTime = 0.0f,
        XMFLOAT3 cameraPos = { 0.0f, 0.0f, 0.0f });

    // =========================================================================
    // Light-СЃС„РµСЂС‹ (С‚РѕР»СЊРєРѕ РґР»СЏ СЂРµРґР°РєС‚РѕСЂР°, РЅРµ С‡Р°СЃС‚СЊ ECS)
    // =========================================================================
    void SetupLightSpheres(std::vector<GameObject>& spheres, AssimpModelClass* model);
    
    // =========================================================================
    // РќР°СЃС‚СЂРѕР№РєРё
    // =========================================================================
    void SetDefaultMaterial(MaterialClass* mat) { m_defaultMaterial = mat; }
    MaterialClass* GetDefaultMaterial() const { return m_defaultMaterial; }
    void SetFallbackModel(AssimpModelClass* model) { m_mainModelFallback = model; }
    ECSRegistry& GetECSRegistry() { return m_registry; }

    Settings::FogSettings BuildFogSettingsFromWorld() const
    {
        Settings::FogSettings fog;
        fog.enabled = m_worldSettings.fogEnabled;
        fog.color = m_worldSettings.fogColor;
        fog.FOG_START = m_worldSettings.fogStart;
        fog.FOG_END = m_worldSettings.fogEnd;
        return fog;
    }

    Entity SpawnLight(const XMFLOAT3& position, const XMFLOAT4& color, bool isGizmo = false);
    bool   RemoveLight(Entity entity);

    bool IsLightActive(Entity i)
    {

        if (m_registry.HasComponent<LightComponent>(i))
        {
            return m_registry.GetComponent<LightComponent>(i)->enabled;
        }

    }

    bool SetObjectActive(Entity entity, bool active);



    void SetLightPosition(Entity entity, const XMFLOAT3& position);
    void SetLightEnabled(Entity entity, bool enabled);
    void AttachLightToObject(Entity lightEntity, Entity targetEntity, const XMFLOAT3& offset = {});
    void DetachLight(Entity lightEntity);
    void SetLightColor(Entity entity, const XMFLOAT4& color);

    // Р”Р»СЏ СЂРµРЅРґРµСЂР°: СЃРѕР±РёСЂР°РµРј РґР°РЅРЅС‹Рµ Рѕ СЃРІРµС‚Р°С… РІ РјР°СЃСЃРёРІС‹ РґР»СЏ С€РµР№РґРµСЂР°
    void GatherLightsForShader(
        XMFLOAT4 outColors[], XMFLOAT4 outPositions[],
        XMFLOAT4 outDirectionsAndOuterCos[], XMFLOAT4 outInnerCosAndFlags[],
        int maxLights = NUM_LIGHTS);



private:

    bool ShouldRenderInWorldPass(Entity entity);

    struct AsyncShipImportState;
    std::shared_ptr<AsyncShipImportState> m_asyncShipImport;

    void ImportShipNode(
        const aiScene* scene,
        aiNode* node,
        const DirectX::XMMATRIX& parentWorld,
        Entity shipRoot,
        ID3D11Device* device,
        ID3D11DeviceContext* context,
        const std::unordered_map<std::string, aiNode*>* collisionProxyNodes);

	WorldSettings m_worldSettings;

    ECSRegistry m_registry;


    std::vector<std::unique_ptr<WorldSpriteClass>>  m_sprites;
    std::unordered_map<std::string, AssimpModelClass*> m_modelCache;

    // РњРѕРґРµР»Рё, СЃРѕР±СЂР°РЅРЅС‹Рµ РёР· РѕС‚РґРµР»СЊРЅС‹С… aiMesh РїСЂРё РёРјРїРѕСЂС‚Рµ РєРѕСЂР°Р±Р»СЏ.
    // РћРЅРё РЅРµ Р»РµР¶Р°С‚ РІ РѕР±С‰РµРј cache РїРѕ РїСѓС‚Рё, РїРѕС‚РѕРјСѓ С‡С‚Рѕ РѕРґРёРЅ GLB РґР°РµС‚ РјРЅРѕРіРѕ СЂР°Р·РЅС‹С… РјРѕРґРµР»РµР№.
    std::vector<std::unique_ptr<AssimpModelClass>> m_importedShipModels;

    AssimpModelClass* m_mainModelFallback = nullptr;
    MaterialClass* m_defaultMaterial = nullptr;
    AssetLoadSystem* m_assetLoadSystem = nullptr;
    bool m_firstPersonViewActive = false;
    Entity m_firstPersonLocalPlayer = INVALID_ENTITY;
    json   SerializeObject(Entity entity, int id);
    Entity DeserializeObject(const json& j, ID3D11Device* device,
        ID3D11DeviceContext* deviceContext);

    json   SerializeMaterial(const MaterialClass* mat);

    ID3D11ShaderResourceView* m_refractionSRV = nullptr;

    AssimpModelClass* m_lightModel = nullptr;


    //WATER 
    float m_waterLevel = 0.0f;  // ?? РЈСЂРѕРІРµРЅСЊ РїРѕРІРµСЂС…РЅРѕСЃС‚Рё РІРѕРґС‹

public:
    float GetWaterLevel() const { return m_waterLevel; }
    void SetWaterLevel(float level) { m_waterLevel = level; }

};
