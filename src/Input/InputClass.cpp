#include "InputClass.h"
#include <windows.h>  // Для SetCursorPos, GetClientRect

InputClass::InputClass()
{
    for (int i = 0; i < 256; ++i) m_keys[i] = false;
}

void InputClass::Initialize()
{
    for (int i = 0; i < 256; ++i) m_keys[i] = false;
    m_mouseX = m_mouseY = 0;
    m_mouseDeltaX = m_mouseDeltaY = 0;
    m_leftButtonDown = m_rightButtonDown = m_middleButtonDown = false;
    m_mouseCaptured = false;
}

// ... KeyDown/KeyUp/IsKeyDown без изменений ...

// 🔹 Методы мыши
void InputClass::SetMousePosition(int x, int y)
{
    m_mouseX = x;
    m_mouseY = y;
}

void InputClass::GetMousePosition(int& x, int& y) const
{
    x = m_mouseX;
    y = m_mouseY;
}

void InputClass::AddMouseDelta(int dx, int dy)
{
    m_mouseDeltaX += dx;
    m_mouseDeltaY += dy;
}

void InputClass::GetMouseDelta(int& dx, int& dy)
{
    dx = m_mouseDeltaX;
    dy = m_mouseDeltaY;
}

void InputClass::ResetMouseDelta()
{
    m_mouseDeltaX = 0;
    m_mouseDeltaY = 0;
}

void InputClass::UpdateCapturedMouseDelta(HWND hwnd)
{
    if (!m_mouseCaptured || !hwnd) return;

    RECT rect;
    if (!GetClientRect(hwnd, &rect)) return;

    POINT center = {
        rect.left + (rect.right - rect.left) / 2,
        rect.top + (rect.bottom - rect.top) / 2
    };
    ClientToScreen(hwnd, &center);

    POINT cursor;
    if (!GetCursorPos(&cursor)) return;

    int dx = cursor.x - center.x;
    int dy = cursor.y - center.y;

    if (dx != 0 || dy != 0)
    {
        AddMouseDelta(dx, dy);
        SetCursorPos(center.x, center.y);
    }
}

// ===== InputClass.cpp =====

void InputClass::SetMouseCapture(bool capture)
{
    m_mouseCaptured = capture;

    if (capture) {
        // 🔹 СКРЫВАЕМ курсор при захвате
        while (ShowCursor(FALSE) >= 0); // Цикл гарантирует скрытие

        // Центрируем мышь
        RECT rect;
        GetClientRect(GetActiveWindow(), &rect);
        POINT center = {
            rect.left + (rect.right - rect.left) / 2,
            rect.top + (rect.bottom - rect.top) / 2
        };
        ClientToScreen(GetActiveWindow(), &center);
        SetCursorPos(center.x, center.y);

        ResetMouseDelta();
        m_mouseX = center.x;
        m_mouseY = center.y;
    }
    else {
        // 🔹 ПОКАЗЫВАЕМ курсор при освобождении
        while (ShowCursor(TRUE) < 0); // Цикл гарантирует появление
    }
}


void InputClass::KeyDown(unsigned int key)
{
    if (key < 256)
        m_keys[key] = true;
}

// 🔹 ДОБАВЬТЕ ЭТУ ФУНКЦИЮ (её не хватало):
void InputClass::KeyUp(unsigned int key)
{
    if (key < 256)
        m_keys[key] = false;  // Сбрасываем состояние клавиши
}

bool InputClass::IsKeyDown(unsigned int key)
{
    return (key < 256) ? m_keys[key] : false;
}
