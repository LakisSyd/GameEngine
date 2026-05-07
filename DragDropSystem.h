#pragma once
#include <functional>
#include <DirectXMath.h>
#include "cameraclass.h"
#include "PickingSystem.h"
#include "EditorUI.h"
#include "InputManager.h"
class SceneManager;
class InputClass;

using namespace DirectX;

class DragDropSystem
{
public:
    DragDropSystem(CameraClass* camera,
        InputManager* inputManager,
        InputClass* input,
        EditorUI* editorUI,
        SceneManager* sceneManager);

    /// spawnCallback — ApplicationClass::SpawnObjectFromRequest
    void Update(const XMMATRIX& view,
        const XMMATRIX& projection,
        int screenWidth, int screenHeight,
        const std::vector<GameObject>& lightSpheres,
        std::function<void(const EditorUI::SpawnRequest&)> spawnCallback);

    // ── Статические утилиты (раньше жили в ApplicationClass) ─────────────────
    static bool RayIntersectsGroundPlane(const CameraClass::Ray& ray,
        XMFLOAT3& outHit,
        float groundY = 0.f);
    static bool RayIntersectsSphere(const CameraClass::Ray& ray,
        const XMFLOAT3& center,
        float radius);

private:
    void StartDraggingObject(int index, const CameraClass::Ray& ray, const XMFLOAT3& pos);
    void StartDraggingSprite(int index, const CameraClass::Ray& ray, const XMFLOAT3& pos);
    void UpdateDraggedPosition(const CameraClass::Ray& ray);
    void StopDragging();

    CameraClass* m_camera;
    InputManager* m_inputManager;
    InputClass* m_input;
    EditorUI* m_editorUI;
    SceneManager* m_sceneManager;
    PickingSystem      m_pickingSystem;

    bool   m_isDragging = false;
    int    m_draggedObjectIndex = -1;
    int    m_draggedLightIndex = -1;
    int    m_draggedSpriteIndex = -1;
    float  m_dragDistance = 0.f;
    XMFLOAT3 m_dragOffset = {};

    PickingSystem::PickResult m_pendingPickResult = {};
};
