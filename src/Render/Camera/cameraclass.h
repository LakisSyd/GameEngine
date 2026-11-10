// ===== CameraClass.h =====
#pragma once
#include <DirectXMath.h>
using namespace DirectX;

class CameraClass
{
public:
    CameraClass();
    XMFLOAT3 GetForward() const;
    CameraClass(const CameraClass&) = default;
    CameraClass& operator=(const CameraClass&) = default;
    ~CameraClass() = default;

    void SetPosition(float, float, float);
    void SetRotation(float, float, float);

    XMFLOAT3 GetPosition() const;
    XMFLOAT3 GetRotation() const;

    void Render();
    void GetViewMatrix(XMMATRIX&);

    // 🔹 RAY PICKING
    struct Ray {
        XMFLOAT3 origin;    // Начало луча (позиция камеры)
        XMFLOAT3 direction; // Направление (нормализованное)
    };

    XMFLOAT3 UnProject(int mouseX, int mouseY,
        int screenWidth, int screenHeight,
        const XMMATRIX& view,      // 👈 Матрица вида
        const XMMATRIX& projection,// 👈 Матрица проекции
        float nearZ = 0.0f,
        float farZ = 1.0f);

    // Создаёт луч из координат мыши
    Ray CreatePickingRay(
        int mouseX, int mouseY,
        int screenWidth, int screenHeight,
        const XMMATRIX& view,
        const XMMATRIX& projection
    );

private:
    float m_positionX = 0.0f;
    float m_positionY = 0.0f;
    float m_positionZ = 0.0f;
    float m_rotationX = 0.0f;
    float m_rotationY = 0.0f;
    float m_rotationZ = 0.0f;
    XMMATRIX m_viewMatrix = XMMatrixIdentity();
};
