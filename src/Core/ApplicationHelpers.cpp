#include "ApplicationHelpers.h"
#include "SceneManager.h"

// ============================================================================
// RayIntersectsSphere
// ============================================================================
bool RayIntersectsSphere(const CameraClass::Ray& ray,
    const XMFLOAT3& sphereCenter, float sphereRadius)
{
    XMVECTOR L = XMVectorSubtract(XMLoadFloat3(&sphereCenter), XMLoadFloat3(&ray.origin));
    float t = XMVectorGetX(XMVector3Dot(L, XMLoadFloat3(&ray.direction)));
    if (t < 0.0f) return false;

    XMVECTOR directionScaled = XMVectorScale(XMLoadFloat3(&ray.direction), t);
    XMVECTOR closest = XMVectorAdd(XMLoadFloat3(&ray.origin), directionScaled);
    XMVECTOR sphereCenterVec = XMLoadFloat3(&sphereCenter);
    float dist = XMVectorGetX(XMVector3Length(XMVectorSubtract(closest, sphereCenterVec)));
    return dist <= sphereRadius;
}

// ============================================================================
// BuildReflectionMatrix
// ============================================================================
XMMATRIX BuildReflectionMatrix(CameraClass* camera, float reflectY)
{
    XMFLOAT3 pos = camera->GetPosition();
    XMFLOAT3 rot = camera->GetRotation();

    float reflectedPosY = 2.0f * reflectY - pos.y;
    float pitch = rot.x * 0.0174532925f;
    float yaw = rot.y * 0.0174532925f;

    XMMATRIX rotMat = XMMatrixRotationRollPitchYaw(pitch, yaw, 0.0f);
    XMVECTOR forward = XMVector3TransformCoord(XMVectorSet(0, 0, 1, 0), rotMat);
    XMVECTOR up = XMVector3TransformCoord(XMVectorSet(0, 1, 0, 0), rotMat);

    XMVECTOR reflectPlane = XMVectorSet(0.0f, 1.0f, 0.0f, -reflectY);
    XMMATRIX reflectMat = XMMatrixReflect(reflectPlane);

    XMVECTOR reflPos = XMVectorSet(pos.x, reflectedPosY, pos.z, 1.0f);
    XMVECTOR reflForward = XMVector3TransformNormal(forward, reflectMat);
    XMVECTOR reflUp = XMVector3TransformNormal(up, reflectMat);

    return XMMatrixLookAtLH(reflPos, XMVectorAdd(reflPos, reflForward), reflUp);
}

// ============================================================================
// StartDraggingSprite
// ============================================================================
void StartDraggingSprite(DragState& state, int index,
    const CameraClass::Ray& pickRay, const XMFLOAT3& spritePos)
{
    state.isDragging = true;
    state.draggedSpriteIndex = index;
    state.draggedLightIndex = -1;
    state.draggedObjectIndex = -1;

    XMVECTOR toSprite = XMVectorSubtract(XMLoadFloat3(&spritePos), XMLoadFloat3(&pickRay.origin));
    state.dragDistance = max(1.0f, XMVectorGetX(XMVector3Length(toSprite)));
    state.dragOffset = { 0, 0, 0 };
}

// ============================================================================
// StartDraggingObject
// ============================================================================
void StartDraggingObject(DragState& state, int index,
    const CameraClass::Ray& pickRay, const XMFLOAT3& objPos)
{
    state.isDragging = true;
    state.draggedObjectIndex = index;
    state.draggedLightIndex = -1;
    state.draggedSpriteIndex = -1;

    XMVECTOR toObj = XMVectorSubtract(XMLoadFloat3(&objPos), XMLoadFloat3(&pickRay.origin));
    state.dragDistance = max(1.0f, XMVectorGetX(XMVector3Length(toObj)));
    state.dragOffset = { 0, 0, 0 };
}

// ============================================================================
// UpdateDraggedPosition
// ============================================================================
void UpdateDraggedPosition(DragState& state, const CameraClass::Ray& ray,
    SceneManager& sceneManager)
{
    XMFLOAT3 newPos = {
        ray.origin.x + ray.direction.x * state.dragDistance + state.dragOffset.x,
        ray.origin.y + ray.direction.y * state.dragDistance + state.dragOffset.y,
        ray.origin.z + ray.direction.z * state.dragDistance + state.dragOffset.z
    };

    if (state.draggedLightIndex >= 0)
    {
        // m_sceneManager.SetLight(m_draggedLightIndex, newPos, XMFLOAT4(1, 1, 0.9f, 1), true);
    }
    else if (state.draggedSpriteIndex >= 0)
    {
        if (auto* spr = sceneManager.GetSprite(state.draggedSpriteIndex))
            spr->SetPosition(newPos);
    }
    else if (state.draggedObjectIndex >= 0)
    {
        if (auto* transform = sceneManager.GetObjectTransform(state.draggedObjectIndex))
            transform->position = newPos;
    }
}

// ============================================================================
// StopDragging
// ============================================================================
void StopDragging(DragState& state)
{
    state.isDragging = false;
    state.draggedObjectIndex = -1;
    state.draggedLightIndex = -1;
    state.draggedSpriteIndex = -1;
    state.dragDistance = 0.0f;
}
