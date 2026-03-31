
#pragma once
#include <vector>
#include <DirectXMath.h>
#include "colorshaderclass.h"

class ID3D11DeviceContext;
class AssimpModelClass;
class TextureShaderClass;
class ECSRegistry;

using namespace DirectX;

struct DebugContact
{
    XMFLOAT3 position;
    XMFLOAT3 normal;
    const void* bodyA;
    const void* bodyB;
    float distance;
};

class DebugPhysicsVisualizer
{
public:
    DebugPhysicsVisualizer();
    ~DebugPhysicsVisualizer();

    // Сбор контактов из мира Bullet
    void UpdateFromWorld(class btCollisionWorld* world);

    // Рисует маркеры контактных точек (маленькие сферы)
    void RenderContacts(
        ID3D11DeviceContext* context,
        XMMATRIX view,
        XMMATRIX projection,
        AssimpModelClass* sphereModel,
        TextureShaderClass* textureShader,
        float scale = 0.12f);


    void RenderLogicalCompartments(
        ECSRegistry& registry,
        ID3D11DeviceContext* context,
        const XMMATRIX& view,
        const XMMATRIX& projection,
        ColorShaderClass* wireframeShader,
        AssimpModelClass* boxModel,  // ← 🔥 ДОБАВИТЬ параметр!
        const XMFLOAT4& colorDry,
        const XMFLOAT4& colorFlooded,
        float waterAlpha);   // прозрачность воды


    void RenderContacts(
        ID3D11DeviceContext* context,
        XMMATRIX view,
        XMMATRIX projection,
        AssimpModelClass* sphereModel,
        ColorShaderClass* colorShader,  // ← другой тип
        float scale = 0.12f);

    void RenderCollisionShapes(
        ECSRegistry& registry,
        ID3D11DeviceContext* context,
        XMMATRIX view,
        XMMATRIX projection,
        AssimpModelClass* sphereModel,
        AssimpModelClass* boxModel,
        AssimpModelClass* capsuleModel,
        ColorShaderClass* colorShader,  // ← другой тип
        float alpha = 0.45f);

    // Рисует коллайдеры (Box/Sphere/Capsule) по ECS реестру.
    // Передавайте модели: sphereModel, boxModel, capsuleModel и TextureShaderClass* для рендера.
    void RenderCollisionShapes(
        ECSRegistry& registry,
        ID3D11DeviceContext* context,
        XMMATRIX view,
        XMMATRIX projection,
        AssimpModelClass* sphereModel,
        AssimpModelClass* boxModel,
        AssimpModelClass* capsuleModel,
        TextureShaderClass* textureShader,
        float alpha = 0.45f);

    void Clear() { m_contacts.clear(); }
    const std::vector<DebugContact>& GetContacts() const { return m_contacts; }

private:
    std::vector<DebugContact> m_contacts;
};