#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include "ECSRegistry .h"
#include "colorshaderclass.h"

using namespace DirectX;

// ══════════════════════════════════════════════════════════════════════════════
// Настройки визуализации (управляются через EditorUI)
// ══════════════════════════════════════════════════════════════════════════════
struct PhysicsDebugSettings
{
    bool showColliders = true;
    bool showContacts = false;
    bool showAABB = false;
    bool showCenterOfMass = false;
    bool showVelocity = false;

    // Фильтры по типам
    bool showBoxes = true;
    bool showSpheres = true;
    bool showCapsules = true;
    bool showCylinders = true;
    bool showPlanes = true;
    bool showConvexHulls = true;
    bool showTriangleMeshes = true;

    // Цвета для разных состояний
    XMFLOAT4 colorDynamic = { 0.2f, 0.8f, 0.2f, 0.6f };    // Зелёный - динамические
    XMFLOAT4 colorStatic = { 0.8f, 0.2f, 0.2f, 0.6f };     // Красный - статические
    XMFLOAT4 colorKinematic = { 0.2f, 0.5f, 0.9f, 0.6f };  // Синий - кинематические
    XMFLOAT4 colorSleeping = { 0.5f, 0.5f, 0.5f, 0.4f };   // Серый - спящие
    XMFLOAT4 colorContact = { 1.0f, 1.0f, 0.0f, 1.0f };    // Жёлтый - контакты
    XMFLOAT4 colorAABB = { 0.9f, 0.6f, 0.1f, 0.3f };       // Оранжевый - AABB

    float lineThickness = 1.0f;
    float contactMarkerSize = 0.1f;
    float velocityScale = 0.5f;
};

// ══════════════════════════════════════════════════════════════════════════════
// Основной класс рендера
// ══════════════════════════════════════════════════════════════════════════════
class PhysicsDebugRenderer
{
public:
    PhysicsDebugRenderer();
    ~PhysicsDebugRenderer();

    bool Initialize(ID3D11Device* device);
    void Shutdown();

    // Главный метод рендера - рисует ВСЁ согласно настройкам
    void Render(
        ECSRegistry& registry,
        class btCollisionWorld* world,
        ID3D11DeviceContext* context,
        const XMMATRIX& view,
        const XMMATRIX& projection,
        ColorShaderClass* shader,
        const PhysicsDebugSettings& settings);

    PhysicsDebugSettings& GetSettings() { return m_settings; }

private:
    // Геометрические примитивы
    struct PrimitiveBuffers
    {
        ID3D11Buffer* vertexBuffer = nullptr;
        ID3D11Buffer* indexBuffer = nullptr;
        int indexCount = 0;
    };

    PrimitiveBuffers m_cubeBuffers;
    PrimitiveBuffers m_sphereBuffers;
    PrimitiveBuffers m_capsuleBuffers;
    PrimitiveBuffers m_cylinderBuffers;
    PrimitiveBuffers m_lineBuffers;

    PhysicsDebugSettings m_settings;

    // Создание примитивов
    void CreateCube(ID3D11Device* device);
    void CreateSphere(ID3D11Device* device, int segments = 16);
    void CreateCapsule(ID3D11Device* device, int segments = 12);
    void CreateCylinder(ID3D11Device* device, int segments = 16);
    void CreateLine(ID3D11Device* device);

    // Рендер отдельных элементов
    void RenderCollider(
        ID3D11DeviceContext* context,
        const XMMATRIX& world,
        const XMMATRIX& view,
        const XMMATRIX& projection,
        ColorShaderClass* shader,
        const PrimitiveBuffers& buffers,
        const XMFLOAT4& color);

    void RenderBox(
        ID3D11DeviceContext* context,
        const btVector3& halfExtents,
        const btTransform& transform,
        const XMMATRIX& view,
        const XMMATRIX& projection,
        ColorShaderClass* shader,
        const XMFLOAT4& color);

    void RenderSphere(
        ID3D11DeviceContext* context,
        float radius,
        const btVector3& position,
        const XMMATRIX& view,
        const XMMATRIX& projection,
        ColorShaderClass* shader,
        const XMFLOAT4& color);

    void RenderCapsule(
        ID3D11DeviceContext* context,
        float radius,
        float height,
        const btTransform& transform,
        const XMMATRIX& view,
        const XMMATRIX& projection,
        ColorShaderClass* shader,
        const XMFLOAT4& color);

    void RenderAABB(
        ID3D11DeviceContext* context,
        const btVector3& min,
        const btVector3& max,
        const XMMATRIX& view,
        const XMMATRIX& projection,
        ColorShaderClass* shader);

    void RenderContactPoint(
        ID3D11DeviceContext* context,
        const XMFLOAT3& position,
        const XMFLOAT3& normal,
        const XMMATRIX& view,
        const XMMATRIX& projection,
        ColorShaderClass* shader);

    // Вершина с цветом
    struct ColorVertex
    {
        XMFLOAT3 position;
        XMFLOAT4 color;
    };

    // Хелпер для выбора цвета
    XMFLOAT4 GetColorForRigidBody(const btRigidBody* body);
    void RenderCompoundShape(ID3D11DeviceContext* context, btCompoundShape* compound, const btTransform& parentTransform, const XMMATRIX& view, const XMMATRIX& projection, ColorShaderClass* shader, const XMFLOAT4& color, const PhysicsDebugSettings& settings);
};
