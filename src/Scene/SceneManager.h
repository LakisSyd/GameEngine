#pragma once
#include <vector>
#include <array>
#include <memory>
#include <DirectXMath.h>
#include "gameobject.h"
#include "assimpmodelclass.h"
#include "lightshaderclass.h"
#include "WorldSpriteClass.h"
#include "MaterialClass.h"
#include "Json/json.hpp"
#include "GameSettings.h"

using namespace DirectX;
using json = nlohmann::json;

#ifndef NUM_LIGHTS
#define NUM_LIGHTS 4
#endif

class SceneManager
{
public:
    // 🔹 Данные источника света (совместимы с шейдером)
    struct LightSource {
        XMFLOAT4 position;  // xyz = позиция, w = 1.0
        XMFLOAT4 color;     // rgb = цвет, a = интенсивность
        bool active;
        int attachedToObjectIndex;  // 🔹 Если >= 0 — свет следует за объектом
        XMFLOAT3 offset;            // Смещение относительно объекта

        LightSource()
            : position(0, 0, 0, 1), color(0, 0, 0, 1), active(false),
            attachedToObjectIndex(-1), offset(0, 0, 0) {
        }
    };

    SceneManager();
    ~SceneManager();

    // === Управление объектами ===
    int SpawnObject(AssimpModelClass* model, float x, float y, float z);
    bool RemoveObject(int index);
    void Clear();
    GameObject* GetObject(int index);
    size_t GetObjectCount() const { return m_objects.size(); }


    // ================  СОХРАНЕНИЕ И ЗАГРУЗКА СЦЕНЫ ======

    bool SaveScene(const std::string& filename);
    bool LoadScene(const std::string& filename, ID3D11Device* device, ID3D11DeviceContext* deviceContext);


    

    // === Управление светом 🔹 ===
    void SetLight(int index, const XMFLOAT3& pos, const XMFLOAT4& color, bool active = true);
    void AttachLightToObject(int lightIndex, int objectIndex, const XMFLOAT3& offset = { 0,0,0 });
    void DetachLight(int lightIndex);
    void UpdateLights();  // 🔹 Вызывать перед рендером!

    // ===== Управление спрайтами =====

    int SpawnSprite(ID3D11Device* device, ID3D11DeviceContext* deviceContext, const char* textureName,
        float x, float y, float z, float width, float height);
    bool RemoveSprite(int index);
    WorldSpriteClass* GetSprite(int index);
    void RenderSprites(ID3D11DeviceContext* context, XMMATRIX view, XMMATRIX projection, TextureShaderClass* shader);
    AssimpModelClass* GetOrCreateModel(const std::string& path,
        ID3D11Device* device, ID3D11DeviceContext* deviceContext,
        const std::string& texturePath = "C:\\Users\\Lakis\\Desktop\\DX_FrameWork\\DX_FrameWork\\assets\\texture\\stone.tga");
    int GetSpriteCount() {
        return static_cast<int>(m_sprites.size());
    
}

    // === Обновление и рендеринг ===
    void UpdateAll(float deltaTime);

    // 🔥 ИЗМЕНЕНО: LightShaderClass* → MaterialClass*
    void RenderAll(
        ID3D11DeviceContext* context,
        ID3D11Device* device,
        XMMATRIX viewMatrix,
        XMMATRIX projectionMatrix,
        MaterialClass* material,
        XMFLOAT4 lightColors[],
        XMFLOAT4 lightPositions[],
        int selectedObjectIndex,
        const Settings::FogSettings& fog);  // ← ДОБАВИТЬ
    const XMFLOAT4& GetLightColor(int index) const { return m_lights[index].color; }
    const XMFLOAT4& GetLightPosition(int index) const { return m_lights[index].position; }
    bool IsLightActive(int index) const { return m_lights[index].active; }

    void SetDefaultMaterial(MaterialClass* mat) { m_defaultMaterial = mat; }
    MaterialClass* GetDefaultMaterial() const { return m_defaultMaterial; }
    void SetFallbackModel(AssimpModelClass* model) { m_mainModelFallback = model; }


    // === Визуализация источников света ===
    void SetupLightSpheres(std::vector<GameObject>& spheres, AssimpModelClass* model);
    void RenderLightSpheres(
        std::vector<GameObject>& spheres,
        ID3D11DeviceContext* context,
        XMMATRIX view,
        XMMATRIX projection,
        TextureShaderClass* shader
    );

    void SetFog(const Settings::FogSettings& fog) { m_fogSettings = fog; m_fogSettings.enabled = true; }
	Settings::FogSettings& GetFogSettings() { return m_fogSettings; }
   

private:
    std::vector<GameObject> m_objects;
    std::array<LightSource, NUM_LIGHTS> m_lights; 
    std::vector<std::unique_ptr<WorldSpriteClass>> m_sprites;

    std::unordered_map<std::string, AssimpModelClass*> m_modelCache;
    AssimpModelClass* m_mainModelFallback;


    MaterialClass* m_defaultMaterial; 

	Settings::FogSettings m_fogSettings;


    // === Вспомогательные методы для сериализации ===

    // Объекты
    json SerializeObject(const GameObject& obj, int id);
    GameObject DeserializeObject(const json& j, ID3D11Device* device, ID3D11DeviceContext* deviceContext);

    // Свет
    json SerializeLight(const LightSource& light, int index);
    void DeserializeLight(const json& j, LightSource& light);

    // Материалы (опционально, если хочешь сохранять и их)
    json SerializeMaterial(const MaterialClass* mat);
};