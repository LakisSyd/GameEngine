#include "InputManager.h"
#include "GameSettings.h"

void InputManager::Update(InputClass* input)
{
    // Обновляем сырое состояние
    m_isLeftDown = input->IsLeftButtonDown();    // ← Было?
    m_isRightDown = input->IsRightButtonDown();  // ← Критично! Без этого IsCameraMode() врёт

    if (!input->IsMouseCaptured()) {
        input->GetMousePosition(m_mouseX, m_mouseY);
    }


    // === Логика клика / начала drag ===
    if (m_isLeftDown && !m_wasLeftDown) {
        m_clickStartX = m_mouseX;
        m_clickStartY = m_mouseY;
        m_potentialDrag = true;

        if (OnLeftClick) {
            OnLeftClick(m_mouseX, m_mouseY);
        }
    }

    // === Логика drag ===
    if (m_potentialDrag && m_isLeftDown && !m_isDragging) {
        int dx = m_mouseX - m_clickStartX;
        int dy = m_mouseY - m_clickStartY;
        int distSq = dx * dx + dy * dy;
        int threshold = Settings::DRAG_THRESHOLD_PX;

        if (distSq > threshold * threshold) {
            m_isDragging = true;
            m_potentialDrag = false;

            if (OnLeftDragStart) {
                OnLeftDragStart(m_clickStartX, m_clickStartY);
            }
        }
    }

    // === Процесс drag ===
    if (m_isDragging && m_isLeftDown) {
        if (OnLeftDrag) {
            OnLeftDrag(m_clickStartX, m_clickStartY, m_mouseX, m_mouseY);
        }
    }

    // === Завершение drag ===
    if (!m_isLeftDown && m_isDragging) {
        m_isDragging = false;
        if (OnLeftDragEnd) {
            OnLeftDragEnd();
        }
    }

    // Сброс потенциального drag при отпускании без движения
    if (!m_isLeftDown && m_potentialDrag) {
        m_potentialDrag = false;
    }

    m_wasLeftDown = m_isLeftDown;
}