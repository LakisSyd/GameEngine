// ===== CameraClass.h =====
#pragma once
#include <DirectXMath.h>
using namespace DirectX;

class CameraClass
{
public:
    CameraClass();
    XMFLOAT3 GetForward() const;
    CameraClass(const CameraClass&);
    ~CameraClass();

    void SetPosition(float, float, float);
    void SetRotation(float, float, float);

    XMFLOAT3 GetPosition();
    XMFLOAT3 GetRotation();

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
    float m_positionX, m_positionY, m_positionZ;
    float m_rotationX, m_rotationY, m_rotationZ;
    XMMATRIX m_viewMatrix;
};