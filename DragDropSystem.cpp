#include "DragDropSystem.h"
#include "SceneManager.h"
#include "InputManager.h"
#include "inputclass.h"
#include <imgui.h>
#include <ImGuizmo.h>

DragDropSystem::DragDropSystem(CameraClass* camera,
    InputManager* inputManager,
    InputClass* input,
    EditorUI* editorUI,
    SceneManager* sceneManager)
    : m_camera(camera)
    , m_inputManager(inputManager)
    , m_input(input)
    , m_editorUI(editorUI)
    , m_sceneManager(sceneManager)
{
}

void DragDropSystem::Update(
    const XMMATRIX& view,
    const XMMATRIX& projection,
    int screenWidth, int screenHeight,
    const std::vector<GameObject>& lightSpheres,
    std::function<void(const EditorUI::SpawnRequest&)> spawnCallback)
{
    if (m_editorUI->IsCornerEditActive() || m_editorUI->IsEditingCompartment())
        return;

   

    // ── Drag ─────────────────────────────────────────────────────────────────
    if (m_inputManager->IsDragging() && m_inputManager->IsLeftDown())
    {
        CameraClass::Ray ray = m_camera->CreatePickingRay(
            m_inputManager->GetMouseX(), m_inputManager->GetMouseY(),
            screenWidth, screenHeight, view, projection);

        if (!m_isDragging && m_pendingPickResult.IsValid())
        {
            switch (m_pendingPickResult.type)
            {
            case PickingSystem::PickedType::Object:
                if (auto* t = m_sceneManager->GetObjectTransform(
                    static_cast<Entity>(m_pendingPickResult.index)))
                    StartDraggingObject(m_pendingPickResult.index, ray, t->position);
                break;
            case PickingSystem::PickedType::Sprite:
                if (auto* spr = m_sceneManager->GetSprite(m_pendingPickResult.index))
                    StartDraggingSprite(m_pendingPickResult.index, ray, spr->GetPosition());
                break;
            default: break;
            }
        }

        if (m_isDragging) UpdateDraggedPosition(ray);
        return;
    }

    // ── Stop drag ─────────────────────────────────────────────────────────────
    if (!m_inputManager->IsDragging() && !m_inputManager->IsLeftDown() && m_isDragging)
    {
        StopDragging();
        m_pendingPickResult = {};
        return;
    }

    if (m_inputManager->IsCameraMode()) return;

    // ── Pick ──────────────────────────────────────────────────────────────────
    if (m_inputManager->IsLeftDown() && !m_isDragging)
    {
        m_pendingPickResult = m_pickingSystem.PickAt(
            m_inputManager->GetMouseX(), m_inputManager->GetMouseY(),
            *m_camera, view, projection,
            *m_sceneManager, lightSpheres,
            screenWidth, screenHeight);

        if (m_pendingPickResult.IsValid())
        {
            switch (m_pendingPickResult.type)
            {
            case PickingSystem::PickedType::Object:
                m_editorUI->SetSelection(EditorUI::SelectedType::Object,
                    static_cast<Entity>(m_pendingPickResult.index));
                break;
            case PickingSystem::PickedType::Light:
                m_editorUI->SetSelection(EditorUI::SelectedType::Light,
                    m_pendingPickResult.index);
                break;
            case PickingSystem::PickedType::Sprite:
                m_editorUI->SetSelection(EditorUI::SelectedType::Sprite,
                    m_pendingPickResult.index);
                break;
            }
        }
        else
        {
            m_pendingPickResult = {};
        }
    }
}

// ── Drag helpers ──────────────────────────────────────────────────────────────
void DragDropSystem::StartDraggingObject(int index, const CameraClass::Ray& ray, const XMFLOAT3& pos)
{
    m_isDragging = true;
    m_draggedObjectIndex = index;
    m_draggedLightIndex = -1;
    m_draggedSpriteIndex = -1;
    XMVECTOR toObj = XMVectorSubtract(XMLoadFloat3(&pos), XMLoadFloat3(&ray.origin));
    m_dragDistance = max(1.f, XMVectorGetX(XMVector3Length(toObj)));
    m_dragOffset = {};
}

void DragDropSystem::StartDraggingSprite(int index, const CameraClass::Ray& ray, const XMFLOAT3& pos)
{
    m_isDragging = true;
    m_draggedSpriteIndex = index;
    m_draggedLightIndex = -1;
    m_draggedObjectIndex = -1;
    XMVECTOR toSpr = XMVectorSubtract(XMLoadFloat3(&pos), XMLoadFloat3(&ray.origin));
    m_dragDistance = max(1.f, XMVectorGetX(XMVector3Length(toSpr)));
    m_dragOffset = {};
}

void DragDropSystem::UpdateDraggedPosition(const CameraClass::Ray& ray)
{
    XMFLOAT3 newPos = {
        ray.origin.x + ray.direction.x * m_dragDistance + m_dragOffset.x,
        ray.origin.y + ray.direction.y * m_dragDistance + m_dragOffset.y,
        ray.origin.z + ray.direction.z * m_dragDistance + m_dragOffset.z
    };

    if (m_draggedSpriteIndex >= 0)
    {
        if (auto* spr = m_sceneManager->GetSprite(m_draggedSpriteIndex))
            spr->SetPosition(newPos.x, newPos.y, newPos.z);
    }
    else if (m_draggedObjectIndex >= 0)
    {
        if (auto* t = m_sceneManager->GetObjectTransform(
            static_cast<Entity>(m_draggedObjectIndex)))
            t->position = newPos;
    }
}

void DragDropSystem::StopDragging()
{
    m_isDragging = false;
    m_draggedObjectIndex = -1;
    m_draggedLightIndex = -1;
    m_draggedSpriteIndex = -1;
    m_dragDistance = 0.f;
}

// ── Статические утилиты ───────────────────────────────────────────────────────
bool DragDropSystem::RayIntersectsGroundPlane(
    const CameraClass::Ray& ray, XMFLOAT3& outHit, float groundY)
{
    XMVECTOR planeN = XMVectorSet(0, 1, 0, 0);
    float    denom = XMVectorGetX(XMVector3Dot(planeN, XMLoadFloat3(&ray.direction)));
    if (fabsf(denom) < 1e-6f) return false;

    XMVECTOR planeP = XMVectorSet(0, groundY, 0, 1);
    XMVECTOR rayO = XMLoadFloat3(&ray.origin);
    float    t = XMVectorGetX(XMVector3Dot(
        XMVectorSubtract(planeP, rayO), planeN)) / denom;
    if (t < 0.f) return false;

    XMStoreFloat3(&outHit,
        XMVectorAdd(rayO, XMVectorScale(XMLoadFloat3(&ray.direction), t)));
    return true;
}

bool DragDropSystem::RayIntersectsSphere(
    const CameraClass::Ray& ray, const XMFLOAT3& center, float radius)
{
    XMVECTOR L = XMVectorSubtract(XMLoadFloat3(&center), XMLoadFloat3(&ray.origin));
    float    t = XMVectorGetX(XMVector3Dot(L, XMLoadFloat3(&ray.direction)));
    if (t < 0.f) return false;

    XMVECTOR closest = XMVectorAdd(XMLoadFloat3(&ray.origin),
        XMVectorScale(XMLoadFloat3(&ray.direction), t));
    float dist = XMVectorGetX(XMVector3Length(
        XMVectorSubtract(closest, XMLoadFloat3(&center))));
    return dist <= radius;
}
