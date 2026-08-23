
#include "InputManager.h"

#include "GameSettings.h"

void InputManager::Update(InputClass * input)
{
    if (!input) return;


    for (unsigned int key = 0; key < 256; ++key)
    {
        const bool isDownNow = input->IsKeyDown(key);

        m_pressedKeys[key] = isDownNow && !m_previousKeys[key];
        m_previousKeys[key] = isDownNow;
    }

    // Обновляем состояние мыши из InputClass
    m_isLeftDown = input->IsLeftButtonDown();
    m_isRightDown = input->IsRightButtonDown();
    m_leftPressed = m_isLeftDown && !m_wasLeftDown;

    if (!input->IsMouseCaptured()) {
        input->GetMousePosition(m_mouseX, m_mouseY);
    }

    // === Логика клика / начала drag ===
    if (m_leftPressed) {
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

void InputManager::SetMouseCapture(InputClass* input, bool capture)
{
    if (!input) return;
    input->SetMouseCapture(capture);

}

void InputManager::GetMouseDelta(int& dx, int& dy) const
{
    dx = 0;
    dy = 0;
}

void InputManager::ResetMouseDelta(InputClass* input)
{
    if (!input) return;
    input->ResetMouseDelta();
}

bool InputManager::IsMouseCaptured(InputClass* input) const
{
    return input ? input->IsMouseCaptured() : false;
}

bool InputManager::IsKeyDown(InputClass* input, unsigned int key) const
{
    return input ? input->IsKeyDown(key) : false;
}

bool InputManager::IsKeyPressed(InputClass* input, unsigned int key) const
{
    (void)input; // оставляем параметр ради текущего API

    return key < 256 && m_pressedKeys[key];
}

void InputManager::Reset()
{
    m_isLeftDown = false;
    m_isRightDown = false;
    m_wasLeftDown = false;
    m_leftPressed = false;
    m_isDragging = false;
    m_potentialDrag = false;
    m_mouseX = 0;
    m_mouseY = 0;
    m_clickStartX = 0;
    m_clickStartY = 0;
    m_previousKeys.fill(false);
    m_pressedKeys.fill(false);
}
