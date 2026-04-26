#pragma once

#include <DirectXMath.h>
#include "cameraclass.h"

using namespace DirectX;

// ============================================================================
// Helper functions for ApplicationClass
// ============================================================================

/// Проверка пересечения луча со сферой
bool RayIntersectsSphere(const CameraClass::Ray& ray, const XMFLOAT3& sphereCenter, float sphereRadius);

/// Построение матрицы отражения для камеры
XMMATRIX BuildReflectionMatrix(CameraClass* camera, float reflectY);

/// Структура для хранения состояния drag-and-drop
struct DragState
{
    bool isDragging = false;
    int draggedObjectIndex = -1;
    int draggedLightIndex = -1;
    int draggedSpriteIndex = -1;
    float dragDistance = 0.0f;
    XMFLOAT3 dragOffset = { 0, 0, 0 };
};

/// Начало перетаскивания спрайта
void StartDraggingSprite(DragState& state, int index, const CameraClass::Ray& pickRay, const XMFLOAT3& spritePos);

/// Начало перетаскивания объекта
void StartDraggingObject(DragState& state, int index, const CameraClass::Ray& pickRay, const XMFLOAT3& objPos);

/// Обновление позиции перетаскиваемого объекта
void UpdateDraggedPosition(DragState& state, const CameraClass::Ray& ray, 
    class SceneManager& sceneManager);

/// Остановка перетаскивания
void StopDragging(DragState& state);
