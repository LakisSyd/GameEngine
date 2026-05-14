#pragma once
#include <vector>
#include <array>
#include <memory>
#include <unordered_map>
#include <DirectXMath.h>

// ============================================================================
// НЕДЕЛЯ 4: gameobject.h нужен только для light-сфер (SetupLightSpheres).
// GetObject(int) удалён — EditorUI и PickingSystem перешли на Entity.
// m_legacyObjects удалён.
// ============================================================================
#include "gameobject.h"       // только для light-сфер (SetupLightSpheres / RenderLightSpheres)
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

#ifndef NUM_LIGHTS
#define NUM_LIGHTS 4
#endif

class SceneManager
{
public:


    SceneManager();
    ~SceneManager();
   

    // =========================================================================
    // Управление объектами
    // =========================================================================
    Entity SpawnObject(AssimpModelClass* model, float x, float y, float z);
    bool   RemoveObject(Entity entity);
    void   Clear();

    void SetRefractionSRV(ID3D11ShaderResourceView* srv) { m_refractionSRV = srv; }

    // ── ECS-доступ ────────────────────────────────────────────────────────────
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





    bool IsObjectActive(Entity entity)
    {
        return m_registry.HasComponent<ActiveComponent>(entity);
    }

    // Количество объектов (MeshRenderer + Active)
    size_t GetObjectCount() const
    {
        return const_cast<ECSRegistry&>(m_registry)
            .GetComponents<MeshRendererComponent>().GetCount();
    }

    // =========================================================================
    // Сохранение / Загрузка
    // =========================================================================
    bool SaveScene(const std::string& filename);
    bool LoadScene(const std::string& filename,
        ID3D11Device* device, ID3D11DeviceContext* deviceContext);

    // =========================================================================
    // Свет
    // =========================================================================

    void UpdateLights();

    void RemapEntityReferences(const std::unordered_map<uint32_t, Entity>& remap);



    // Convenience setters for individual light properties (used by EditorUI)


    // =========================================================================
    // Спрайты
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
    // Кэш моделей
    // =========================================================================
    AssimpModelClass* GetOrCreateModel(
        const std::string& path,
        ID3D11Device* device, ID3D11DeviceContext* deviceContext,
        const std::string& texturePath =
        "C:\\Users\\Lakis\\Desktop\\DX_FrameWork\\DX_FrameWork\\assets\\texture\\stone.tga");

    // Set shader on an object's material.
    // device is required to create a material if the object doesn't have one yet.
    void SetObjectShader(Entity entity, ShaderBaseClass* shader, ID3D11Device* device = nullptr);

    void SetObjectTexture(Entity entity, int slot, const std::string& path, ID3D11Device* device, ID3D11DeviceContext* context);

    void SetLightModel(AssimpModelClass* model) { m_lightModel = model; }        // ✅ НОВОЕ
    AssimpModelClass* GetLightModel() const { return m_lightModel; }

    // =========================================================================
    // Обновление и рендер
    // =========================================================================
    void UpdateAll(float deltaTime);

    void RenderAllECS(
        ID3D11DeviceContext* context,
        const XMMATRIX& viewMatrix,
        const XMMATRIX& projectionMatrix,
        MaterialClass* defaultMaterial,
        XMFLOAT4        lightColors[],
        XMFLOAT4        lightPositions[],
        const Settings::FogSettings& fog,
        Entity selectedEntity = INVALID_ENTITY, XMFLOAT3 cameraPos = { 0.0f, 0.0f, 0.0f }, float deltaTime = 0.0f);

    void RenderAllECSExcluding(
        ID3D11DeviceContext* context,
        const XMMATRIX& viewMatrix,
        const XMMATRIX& projectionMatrix,
        MaterialClass* defaultMaterial,
        XMFLOAT4        lightColors[],
        XMFLOAT4        lightPositions[],
        const Settings::FogSettings& fog,
        Entity excludeEntity = INVALID_ENTITY, float deltaTime = 0.0f);

    // =========================================================================
    // Light-сферы (только для редактора, не часть ECS)
    // =========================================================================
    void SetupLightSpheres(std::vector<GameObject>& spheres, AssimpModelClass* model);
    void RenderLightSpheres(std::vector<GameObject>& spheres,
        ID3D11DeviceContext* context,
        XMMATRIX view, XMMATRIX projection,
        TextureShaderClass* shader);

    // =========================================================================
    // Настройки
    // =========================================================================
    void SetDefaultMaterial(MaterialClass* mat) { m_defaultMaterial = mat; }
    MaterialClass* GetDefaultMaterial() const { return m_defaultMaterial; }
    void SetFallbackModel(AssimpModelClass* model) { m_mainModelFallback = model; }
    ECSRegistry& GetECSRegistry() { return m_registry; }

    void SetFog(const Settings::FogSettings& fog) { m_fogSettings = fog; m_fogSettings.enabled = true; }
    Settings::FogSettings& GetFogSettings() { return m_fogSettings; }

    // Convenience fog accessors (used by EditorUI)
    XMFLOAT3 GetFogColor() const { return m_fogSettings.color; }
    float    GetFogStart() const { return m_fogSettings.FOG_START; }
    float    GetFogEnd()   const { return m_fogSettings.FOG_END; }
    void SetFogColor(const XMFLOAT3& color) { m_fogSettings.color = color; m_fogSettings.enabled = true; }
    void SetFogStart(float start) { m_fogSettings.FOG_START = start; m_fogSettings.enabled = true; }
    void SetFogEnd(float end) { m_fogSettings.FOG_END = end; m_fogSettings.enabled = true; }

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

    // Для рендера: собираем данные о светах в массивы для шейдера
    void GatherLightsForShader(XMFLOAT4 outColors[], XMFLOAT4 outPositions[], int maxLights = NUM_LIGHTS);



private:
    ECSRegistry m_registry;


    std::vector<std::unique_ptr<WorldSpriteClass>>  m_sprites;
    std::unordered_map<std::string, AssimpModelClass*> m_modelCache;

    AssimpModelClass* m_mainModelFallback = nullptr;
    MaterialClass* m_defaultMaterial = nullptr;
    Settings::FogSettings m_fogSettings;

    json   SerializeObject(Entity entity, int id);
    Entity DeserializeObject(const json& j, ID3D11Device* device,
        ID3D11DeviceContext* deviceContext);

    json   SerializeMaterial(const MaterialClass* mat);

    ID3D11ShaderResourceView* m_refractionSRV = nullptr;

    AssimpModelClass* m_lightModel = nullptr;


    //WATER 
    float m_waterLevel = 0.0f;  // 🌊 Уровень поверхности воды

public:
    float GetWaterLevel() const { return m_waterLevel; }
    void SetWaterLevel(float level) { m_waterLevel = level; }

};