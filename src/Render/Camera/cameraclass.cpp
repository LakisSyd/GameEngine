#include "cameraclass.h"
#include <DirectXCollision.h> // Для XMVector3Unproject
CameraClass::CameraClass()
{
    m_positionX = 0.0f;
    m_positionY = 0.0f;
    m_positionZ = 0.0f;
    m_rotationX = 0.0f;
    m_rotationY = 0.0f;
    m_rotationZ = 0.0f;
}

CameraClass::CameraClass(const CameraClass& other)
{
}

CameraClass::~CameraClass()
{
}

void CameraClass::SetPosition(float x, float y, float z)
{
    m_positionX = x;
    m_positionY = y;
    m_positionZ = z;
    return;
}

void CameraClass::SetRotation(float x, float y, float z)
{
    m_rotationX = x;
    m_rotationY = y;
    m_rotationZ = z;
    return;
}

XMFLOAT3 CameraClass::GetPosition()
{
    return XMFLOAT3(m_positionX, m_positionY, m_positionZ);
}

XMFLOAT3 CameraClass::GetRotation()
{
    return XMFLOAT3(m_rotationX, m_rotationY, m_rotationZ);
}

// Render строит матрицу вида на основе позиции и вращения камеры.
void CameraClass::Render()
{
    XMFLOAT3 up, position, lookAt;
    XMVECTOR upVector, positionVector, lookAtVector;
    float yaw, pitch, roll;
    XMMATRIX rotationMatrix;

    // Вектор "верх" направлен по оси Y.
    up.x = 0.0f;
    up.y = 1.0f;
    up.z = 0.0f;
    upVector = XMLoadFloat3(&up);

    // Позиция камеры.
    position.x = m_positionX;
    position.y = m_positionY;
    position.z = m_positionZ;
    positionVector = XMLoadFloat3(&position);

    // По умолчанию камера смотрит вдоль оси Z.
    lookAt.x = 0.0f;
    lookAt.y = 0.0f;
    lookAt.z = 1.0f;
    lookAtVector = XMLoadFloat3(&lookAt);

    // Преобразуем углы Эйлера в радианы.
    pitch = m_rotationX * 0.0174532925f;
    yaw = m_rotationY * 0.0174532925f;
    roll = m_rotationZ * 0.0174532925f;

    // Создаем матрицу вращения.
    rotationMatrix = XMMatrixRotationRollPitchYaw(pitch, yaw, roll);

    // Поворачиваем векторы направления и верха.
    lookAtVector = XMVector3TransformCoord(lookAtVector, rotationMatrix);
    upVector = XMVector3TransformCoord(upVector, rotationMatrix);

    // Смещаем точку взгляда на позицию камеры.
    lookAtVector = XMVectorAdd(positionVector, lookAtVector);

    // Создаем итоговую матрицу вида.
    m_viewMatrix = XMMatrixLookAtLH(positionVector, lookAtVector, upVector);

    return;
}

// GetViewMatrix возвращает рассчитанную матрицу вида.
void CameraClass::GetViewMatrix(XMMATRIX& viewMatrix)
{
    viewMatrix = m_viewMatrix;
    return;
}

XMFLOAT3 CameraClass::UnProject(int mouseX, int mouseY,
    int screenWidth, int screenHeight,
    const XMMATRIX& view,
    const XMMATRIX& projection,
    float nearZ, float farZ)
{
    // Координаты экрана (Z = nearZ для точки на ближней плоскости, или любая другая глубина)
    XMVECTOR screenPos = XMVectorSet(
        static_cast<float>(mouseX),
        static_cast<float>(mouseY),
        nearZ,
        1.0f
    );

    // Преобразование в мировые координаты
    XMVECTOR worldPos = XMVector3Unproject(
        screenPos,
        0.0f, 0.0f,                          // Viewport X, Y
        static_cast<float>(screenWidth),     // Viewport Width
        static_cast<float>(screenHeight),    // Viewport Height
        0.0f, 1.0f,                          // Viewport MinZ, MaxZ
        projection,                          // Матрица проекции
        view,                                // Матрица вида
        XMMatrixIdentity()                   // Мировая матрица
    );

    XMFLOAT3 result;
    XMStoreFloat3(&result, worldPos);
    return result;
}


CameraClass::Ray CameraClass::CreatePickingRay(
    int mouseX, int mouseY,
    int screenWidth, int screenHeight,
    const XMMATRIX& view,
    const XMMATRIX& projection)
{
    Ray ray;

    // 1️⃣ Начало луча — позиция камеры в мире
    ray.origin = GetPosition();

    // 2️⃣ Вычисляем точку на дальней плоскости отсечения (far plane)
    //    XMVector3Unproject преобразует экранные координаты в мировые
    XMVECTOR screenPos = XMVectorSet(
        static_cast<float>(mouseX),
        static_cast<float>(mouseY),
        1.0f,           // farZ = 1.0 (дальняя плоскость)
        1.0f
    );

    XMVECTOR farPoint = XMVector3Unproject(
        screenPos,
        0.0f, 0.0f,                          // viewport X, Y
        static_cast<float>(screenWidth),     // viewport Width
        static_cast<float>(screenHeight),    // viewport Height
        0.0f, 1.0f,                          // viewport MinZ, MaxZ
        projection,                          // матрица проекции
        view,                                // матрица вида
        XMMatrixIdentity()                   // мировая матрица (единичная)
    );

    // 3️⃣ Направление луча = вектор от камеры к точке на far plane
    XMVECTOR direction = XMVector3Normalize(
        XMVectorSubtract(farPoint, XMLoadFloat3(&ray.origin))
    );

    XMStoreFloat3(&ray.direction, direction);

    return ray;
}